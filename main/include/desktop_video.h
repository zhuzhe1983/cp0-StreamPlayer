#ifndef STREAMPLAYER_DESKTOP_VIDEO_H
#define STREAMPLAYER_DESKTOP_VIDEO_H

#include <string>
#include <vector>
#include <utility>

namespace streamplayer {
// Conservative application limits, independent of the monitor's EDID modes.
constexpr int kHdmiMaxWidth = 1280;
constexpr int kHdmiMaxHeight = 720;
constexpr int kHdmiMaxVideoBitrate = 8000000;
struct VideoProfile {
    int width;
    int height;
    int video_bitrate;
    int audio_bitrate;
    int max_framerate;
};
std::pair<int, int> desktop_output_size(const std::string &graphics_directory = "/sys/class/graphics");
VideoProfile video_profile(bool small, int output_width, int output_height);
std::string constrain_transcode_url(const std::string &url, const VideoProfile &profile);
// The returned name is a compositor output, not a framebuffer number. Never
// select the internal SPI LCD when no connected HDMI connector is present.
std::string connected_hdmi_output(const std::string &drm_directory = "/sys/class/drm");
bool use_small_video_output(bool force_small, bool hdmi_connected,
                            bool desktop_available);
std::vector<std::string> desktop_video_options(const std::string &output,
                                               bool paused);
}

#endif
