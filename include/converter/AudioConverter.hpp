#ifndef AUDIO_CONVERTER_HPP
#define AUDIO_CONVERTER_HPP

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

// Abstract base for audio converters built on libav*.
// Derived classes supply codec-specific configuration while the base handles
// file I/O, resampling, encoding loop, and cleanup.
class AudioConverter {
public:
    explicit AudioConverter(int bitrate);
    virtual ~AudioConverter();

    // Convert a single input file to the provided output path.
    void ConvertFile(const std::string& input_path, const std::string& output_path);

    // Recursively walk a directory, converting all ".mp3" (or other) files to the output tree.
    // Derived classes can override ShouldConvertFile if they need a different extension filter.
    void ConvertDirectory(const std::string& input_dir, const std::string& output_dir);

protected:
    // Codec/format hooks that derived classes must implement.
    virtual AVCodecID OutputCodecId() const = 0;
    virtual void ConfigureOutputCodecContext(AVCodecContext& output_ctx, const AVCodecContext& input_ctx) = 0;
    virtual std::string PreferredContainer(const std::string& output_path) const = 0;
    virtual int TargetFrameSize(const AVCodecContext& output_ctx) const;
    virtual bool ShouldConvertFile(const std::string& extension) const;

    int bitrate_bps_;

    // Low-level helpers exposed for derived classes if needed.
    AVFormatContext* input_ctx_;
    AVFormatContext* output_ctx_;
    AVCodecContext* input_codec_ctx_;
    AVCodecContext* output_codec_ctx_;
    SwrContext* resample_ctx_;
    int audio_stream_index_;

private:
    void InitLibav();
    void OpenInputFile(const std::string& input_path);
    void SetupOutputFile(const std::string& output_path);
    void SetupResampler();
    void ConvertAudio();
    void Cleanup();
};

#endif // AUDIO_CONVERTER_HPP
