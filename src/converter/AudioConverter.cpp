#include "converter/AudioConverter.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

extern "C" {
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
}

AudioConverter::AudioConverter(int bitrate)
    : bitrate_bps_(bitrate),
      input_ctx_(nullptr),
      output_ctx_(nullptr),
      input_codec_ctx_(nullptr),
      output_codec_ctx_(nullptr),
      resample_ctx_(nullptr),
      audio_stream_index_(-1) {
    InitLibav();
}

AudioConverter::~AudioConverter() {
    Cleanup();
}

void AudioConverter::InitLibav() {
    avformat_network_init();
}

void AudioConverter::OpenInputFile(const std::string& input_path) {
    if (avformat_open_input(&input_ctx_, input_path.c_str(), nullptr, nullptr) < 0) {
        throw std::runtime_error("Could not open input file: " + input_path);
    }

    if (avformat_find_stream_info(input_ctx_, nullptr) < 0) {
        throw std::runtime_error("Could not find stream information");
    }

    audio_stream_index_ = av_find_best_stream(input_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_stream_index_ < 0) {
        throw std::runtime_error("No audio stream found in " + input_path);
    }

    const AVCodec* input_codec = avcodec_find_decoder(input_ctx_->streams[audio_stream_index_]->codecpar->codec_id);
    input_codec_ctx_ = avcodec_alloc_context3(input_codec);
    if (input_codec_ctx_ == nullptr) {
        throw std::runtime_error("Failed to allocate input codec context");
    }

    avcodec_parameters_to_context(input_codec_ctx_, input_ctx_->streams[audio_stream_index_]->codecpar);
    if (avcodec_open2(input_codec_ctx_, input_codec, nullptr) < 0) {
        throw std::runtime_error("Could not open input codec");
    }
}

void AudioConverter::SetupOutputFile(const std::string& output_path) {
    const AVCodec* output_codec = avcodec_find_encoder(OutputCodecId());
    output_codec_ctx_ = avcodec_alloc_context3(output_codec);
    if (output_codec_ctx_ == nullptr) {
        throw std::runtime_error("Failed to allocate output codec context");
    }

    ConfigureOutputCodecContext(*output_codec_ctx_, *input_codec_ctx_);

    if (avcodec_open2(output_codec_ctx_, output_codec, nullptr) < 0) {
        throw std::runtime_error("Could not open output codec");
    }

    output_ctx_ = avformat_alloc_context();
    const std::string container = PreferredContainer(output_path);
    output_ctx_->oformat = av_guess_format(container.c_str(), output_path.c_str(), nullptr);
    if (output_ctx_->oformat == nullptr) {
        output_ctx_->oformat = av_guess_format(nullptr, output_path.c_str(), nullptr);
    }
    if (output_ctx_->oformat == nullptr) {
        throw std::runtime_error("Could not find suitable output format");
    }

    if (avio_open(&output_ctx_->pb, output_path.c_str(), AVIO_FLAG_WRITE) < 0) {
        throw std::runtime_error("Could not open output file");
    }

    AVStream* output_stream = avformat_new_stream(output_ctx_, nullptr);
    if (output_stream == nullptr) {
        throw std::runtime_error("Could not create output stream");
    }

    output_stream->time_base = {1, output_codec_ctx_->sample_rate};
    if (avcodec_parameters_from_context(output_stream->codecpar, output_codec_ctx_) < 0) {
        throw std::runtime_error("Failed to copy codec parameters");
    }

    if (input_ctx_->metadata != nullptr) {
        av_dict_copy(&output_ctx_->metadata, input_ctx_->metadata, 0);
    }

    if (avformat_write_header(output_ctx_, nullptr) < 0) {
        throw std::runtime_error("Failed to write header");
    }
}

void AudioConverter::SetupResampler() {
    resample_ctx_ = swr_alloc();
    if (resample_ctx_ == nullptr) {
        throw std::runtime_error("Could not allocate resample context");
    }

    av_opt_set_chlayout(resample_ctx_, "in_chlayout", &input_codec_ctx_->ch_layout, 0);
    av_opt_set_chlayout(resample_ctx_, "out_chlayout", &output_codec_ctx_->ch_layout, 0);
    av_opt_set_int(resample_ctx_, "in_sample_rate", input_codec_ctx_->sample_rate, 0);
    av_opt_set_int(resample_ctx_, "out_sample_rate", output_codec_ctx_->sample_rate, 0);
    av_opt_set_sample_fmt(resample_ctx_, "in_sample_fmt", input_codec_ctx_->sample_fmt, 0);
    av_opt_set_sample_fmt(resample_ctx_, "out_sample_fmt", output_codec_ctx_->sample_fmt, 0);

    if (swr_init(resample_ctx_) < 0) {
        throw std::runtime_error("Could not initialize resampler");
    }
}

int AudioConverter::TargetFrameSize(const AVCodecContext& output_ctx) const {
    if (output_ctx.frame_size > 0) {
        return output_ctx.frame_size;
    }
    return 960;
}

bool AudioConverter::ShouldConvertFile(const std::string& extension) const {
    return extension == ".mp3";
}

void AudioConverter::ConvertAudio() {
    AVPacket* input_packet = av_packet_alloc();
    AVPacket* output_packet = av_packet_alloc();
    AVFrame* input_frame = av_frame_alloc();
    AVFrame* resampled_frame = av_frame_alloc();
    AVFrame* output_frame = av_frame_alloc();

    const int frame_size = TargetFrameSize(*output_codec_ctx_);

    AVAudioFifo* fifo = av_audio_fifo_alloc(
        output_codec_ctx_->sample_fmt,
        output_codec_ctx_->ch_layout.nb_channels,
        1
    );

    if (fifo == nullptr) {
        throw std::runtime_error("Could not allocate FIFO");
    }

    int64_t pts = 0;
    int frame_count = 0;
    int64_t processed_samples = 0;
    int64_t expected_samples = 0;
    if (input_ctx_ != nullptr && input_ctx_->duration > 0 && output_codec_ctx_ != nullptr) {
        const double duration_seconds = static_cast<double>(input_ctx_->duration) / AV_TIME_BASE;
        expected_samples = static_cast<int64_t>(duration_seconds * output_codec_ctx_->sample_rate);
    }

    while (av_read_frame(input_ctx_, input_packet) >= 0) {
        if (input_packet->stream_index == audio_stream_index_) {
            if (avcodec_send_packet(input_codec_ctx_, input_packet) < 0) {
                throw std::runtime_error("Failed to send packet to decoder");
            }

            while (avcodec_receive_frame(input_codec_ctx_, input_frame) >= 0) {
                resampled_frame->sample_rate = output_codec_ctx_->sample_rate;
                resampled_frame->format = output_codec_ctx_->sample_fmt;
                av_channel_layout_copy(&resampled_frame->ch_layout, &output_codec_ctx_->ch_layout);
                resampled_frame->nb_samples = av_rescale_rnd(
                    swr_get_delay(resample_ctx_, input_codec_ctx_->sample_rate) + input_frame->nb_samples,
                    output_codec_ctx_->sample_rate,
                    input_codec_ctx_->sample_rate,
                    AV_ROUND_UP
                );

                if (av_frame_get_buffer(resampled_frame, 0) < 0) {
                    throw std::runtime_error("Could not allocate resampled frame buffer");
                }

                const uint8_t** input_data = const_cast<const uint8_t**>(input_frame->data);
                int converted = swr_convert(
                    resample_ctx_,
                    resampled_frame->data, resampled_frame->nb_samples,
                    input_data, input_frame->nb_samples
                );

                if (converted < 0) {
                    throw std::runtime_error("Resampling failed");
                }

                resampled_frame->nb_samples = converted;

                if (av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + converted) < 0) {
                    throw std::runtime_error("Could not realloc FIFO");
                }

                if (av_audio_fifo_write(fifo, reinterpret_cast<void**>(resampled_frame->data), converted) < converted) {
                    throw std::runtime_error("Could not write to FIFO");
                }

                while (av_audio_fifo_size(fifo) >= frame_size) {
                    output_frame->nb_samples = frame_size;
                    output_frame->sample_rate = output_codec_ctx_->sample_rate;
                    output_frame->format = output_codec_ctx_->sample_fmt;
                    av_channel_layout_copy(&output_frame->ch_layout, &output_codec_ctx_->ch_layout);

                    if (av_frame_get_buffer(output_frame, 0) < 0) {
                        throw std::runtime_error("Could not allocate output frame buffer");
                    }

                    if (av_audio_fifo_read(fifo, reinterpret_cast<void**>(output_frame->data), frame_size) < frame_size) {
                        throw std::runtime_error("FIFO read failed");
                    }

                    output_frame->pts = pts;
                    pts += frame_size;
                    processed_samples += frame_size;

                    if (avcodec_send_frame(output_codec_ctx_, output_frame) < 0) {
                        throw std::runtime_error("Encoder send failed");
                    }

                    while (avcodec_receive_packet(output_codec_ctx_, output_packet) == 0) {
                        output_packet->stream_index = 0;
                        av_write_frame(output_ctx_, output_packet);
                        av_packet_unref(output_packet);
                        frame_count++;
                    }

                    av_frame_unref(output_frame);

                    if (progress_cb_ && expected_samples > 0) {
                        double progress = static_cast<double>(processed_samples) / static_cast<double>(expected_samples);
                        if (progress > 1.0) {
                            progress = 1.0;
                        }
                        progress_cb_(progress);
                    }
                }

                av_frame_unref(resampled_frame);
            }
        }

        av_packet_unref(input_packet);
    }

    while (av_audio_fifo_size(fifo) > 0) {
        int remaining = std::min(av_audio_fifo_size(fifo), frame_size);

        output_frame->nb_samples = remaining;
        output_frame->sample_rate = output_codec_ctx_->sample_rate;
        output_frame->format = output_codec_ctx_->sample_fmt;
        av_channel_layout_copy(&output_frame->ch_layout, &output_codec_ctx_->ch_layout);

        if (av_frame_get_buffer(output_frame, 0) < 0) {
            throw std::runtime_error("Could not allocate final frame buffer");
        }

        if (av_audio_fifo_read(fifo, reinterpret_cast<void**>(output_frame->data), remaining) < remaining) {
            throw std::runtime_error("Failed to read from FIFO");
        }

        output_frame->pts = pts;
        pts += remaining;
        processed_samples += remaining;

        if (avcodec_send_frame(output_codec_ctx_, output_frame) < 0) {
            throw std::runtime_error("Failed to flush frame");
        }

        while (avcodec_receive_packet(output_codec_ctx_, output_packet) == 0) {
            output_packet->stream_index = 0;
            av_write_frame(output_ctx_, output_packet);
            av_packet_unref(output_packet);
        }

        av_frame_unref(output_frame);

        if (progress_cb_ && expected_samples > 0) {
            double progress = static_cast<double>(processed_samples) / static_cast<double>(expected_samples);
            if (progress > 1.0) {
                progress = 1.0;
            }
            progress_cb_(progress);
        }
    }

    avcodec_send_frame(output_codec_ctx_, nullptr);
    while (avcodec_receive_packet(output_codec_ctx_, output_packet) == 0) {
        output_packet->stream_index = 0;
        av_write_frame(output_ctx_, output_packet);
        av_packet_unref(output_packet);
    }

    av_write_trailer(output_ctx_);

    av_audio_fifo_free(fifo);
    av_packet_free(&input_packet);
    av_packet_free(&output_packet);
    av_frame_free(&input_frame);
    av_frame_free(&resampled_frame);
    av_frame_free(&output_frame);

    if (progress_cb_) {
        progress_cb_(1.0);
    }
}

void AudioConverter::Cleanup() {
    if (input_ctx_ != nullptr) {
        avformat_close_input(&input_ctx_);
        input_ctx_ = nullptr;
    }
    if (output_ctx_ != nullptr) {
        if (!(output_ctx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&output_ctx_->pb);
        }
        avformat_free_context(output_ctx_);
        output_ctx_ = nullptr;
    }
    if (input_codec_ctx_ != nullptr) {
        avcodec_free_context(&input_codec_ctx_);
        input_codec_ctx_ = nullptr;
    }
    if (output_codec_ctx_ != nullptr) {
        avcodec_free_context(&output_codec_ctx_);
        output_codec_ctx_ = nullptr;
    }
    if (resample_ctx_ != nullptr) {
        swr_free(&resample_ctx_);
        resample_ctx_ = nullptr;
    }
}

void AudioConverter::ConvertFile(const std::string& input_path, const std::string& output_path) {
    try {
        OpenInputFile(input_path);
        SetupOutputFile(output_path);
        SetupResampler();
        ConvertAudio();
    } catch (...) {
        Cleanup();
        throw;
    }
    Cleanup();
}

void AudioConverter::ConvertDirectory(const std::string& input_dir, const std::string& output_dir) {
    std::filesystem::path input_path(input_dir);
    std::filesystem::path output_path(output_dir);
    std::filesystem::create_directories(output_path);

    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(input_path)) {
        if (entry.is_regular_file() && ShouldConvertFile(entry.path().extension().string())) {
            std::string input_file = entry.path().string();
            std::filesystem::path relative_path = std::filesystem::relative(entry.path(), input_path);
            std::filesystem::path output_file = output_path / relative_path;
            output_file.replace_extension(PreferredContainer(output_file.string()));
            std::filesystem::create_directories(output_file.parent_path());

            try {
                ConvertFile(input_file, output_file.string());
            } catch (const std::exception& e) {
                (void)e;
            }
        }
    }
}
