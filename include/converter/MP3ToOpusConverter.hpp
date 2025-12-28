#ifndef MP3_TO_OPUS_CONVERTER_HPP
#define MP3_TO_OPUS_CONVERTER_HPP

#include "converter/AudioConverter.hpp"

// Concrete converter that transcodes MP3 input to Opus output.
class MP3ToOpusConverter : public AudioConverter {
public:
    explicit MP3ToOpusConverter(int bitrate);
    ~MP3ToOpusConverter() override = default;

protected:
    AVCodecID OutputCodecId() const override;
    void ConfigureOutputCodecContext(AVCodecContext& output_ctx, const AVCodecContext& input_ctx) override;
    std::string PreferredContainer(const std::string& output_path) const override;
    int TargetFrameSize(const AVCodecContext& output_ctx) const override;
    bool ShouldConvertFile(const std::string& extension) const override;
};

#endif // MP3_TO_OPUS_CONVERTER_HPP
