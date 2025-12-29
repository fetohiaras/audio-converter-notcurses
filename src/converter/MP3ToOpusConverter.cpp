#include "converter/MP3ToOpusConverter.hpp"

#include <stdexcept>
#include <string>

extern "C" {
#include <libavutil/opt.h>
}

MP3ToOpusConverter::MP3ToOpusConverter(int bitrate)
    : AudioConverter(bitrate) {}

AVCodecID MP3ToOpusConverter::OutputCodecId() const {
    return AV_CODEC_ID_OPUS;
}

void MP3ToOpusConverter::ConfigureOutputCodecContext(AVCodecContext& output_ctx, const AVCodecContext& input_ctx) {
    // Opus encoder expects 48kHz and commonly uses float samples; pick the first supported fmt.
    output_ctx.bit_rate = bitrate_bps_;
    output_ctx.sample_rate = 48000;

    const AVCodec* opus_codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    if (opus_codec == nullptr) {
        throw std::runtime_error("Opus encoder not found");
    }

    AVSampleFormat chosen_fmt = AV_SAMPLE_FMT_FLT;
    if (opus_codec->sample_fmts != nullptr) {
        chosen_fmt = opus_codec->sample_fmts[0];
        for (int i = 0; opus_codec->sample_fmts[i] != AV_SAMPLE_FMT_NONE; ++i) {
            const AVSampleFormat fmt = opus_codec->sample_fmts[i];
            if (fmt == AV_SAMPLE_FMT_FLT || fmt == AV_SAMPLE_FMT_FLTP) {
                chosen_fmt = fmt;
                break;
            }
        }
    }
    output_ctx.sample_fmt = chosen_fmt;

    if (av_channel_layout_copy(&output_ctx.ch_layout, &input_ctx.ch_layout) < 0) {
        // Fallback to stereo if input layout is not available.
        av_channel_layout_default(&output_ctx.ch_layout, 2);
    }

    av_opt_set_int(&output_ctx, "frame_size", 960, 0);
    av_opt_set_int(&output_ctx, "vbr", 1, 0);
    output_ctx.compression_level = 10;
}

std::string MP3ToOpusConverter::PreferredContainer(const std::string& output_path) const {
    (void)output_path;
    return "opus";
}

int MP3ToOpusConverter::TargetFrameSize(const AVCodecContext& output_ctx) const {
    (void)output_ctx;
    return 960;
}

bool MP3ToOpusConverter::ShouldConvertFile(const std::string& extension) const {
    return extension == ".mp3";
}
