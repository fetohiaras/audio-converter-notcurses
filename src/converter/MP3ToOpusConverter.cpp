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
    output_ctx.bit_rate = bitrate_bps_;
    output_ctx.sample_rate = input_ctx.sample_rate;
    output_ctx.sample_fmt = AV_SAMPLE_FMT_S16;

    if (av_channel_layout_copy(&output_ctx.ch_layout, &input_ctx.ch_layout) < 0) {
        throw std::runtime_error("Failed to copy channel layout");
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
