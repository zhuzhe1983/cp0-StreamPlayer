#include "desktop_video.h"

#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <cctype>

namespace streamplayer {
std::pair<int, int> desktop_output_size(const std::string &graphics_directory)
{
    DIR *dir = opendir(graphics_directory.c_str());
    if (!dir) return {};
    std::pair<int, int> size{};
    int matches = 0;
    while (dirent *entry = readdir(dir)) {
        const std::string path = graphics_directory + "/" + entry->d_name;
        std::ifstream name(path + "/name");
        std::string driver;
        std::getline(name, driver);
        if (driver != "vc4drmfb") continue;  // Never use the internal LCD mode.
        std::ifstream dimensions(path + "/virtual_size");
        int width = 0, height = 0;
        char comma = 0;
        if (dimensions >> width >> comma >> height && comma == ',' &&
            width >= 2 && height >= 2 && width <= 16384 && height <= 16384) {
            size = {width, height};
            ++matches;
        }
    }
    closedir(dir);
    // Do not guess which framebuffer belongs to HDMI on multi-DRM systems.
    return matches == 1 ? size : std::pair<int, int>{};
}

VideoProfile video_profile(bool small, int output_width, int output_height)
{
    if (small) return {320, 170, 500000, 128000, 24};
    const int width = output_width >= 2 ? std::min(output_width, kHdmiMaxWidth) : kHdmiMaxWidth;
    const int height = output_height >= 2 ? std::min(output_height, kHdmiMaxHeight) : kHdmiMaxHeight;
    return {width & ~1, height & ~1, kHdmiMaxVideoBitrate, 128000, 24};
}

namespace {
std::string option_key(std::string key)
{
    for (char &c : key) c = std::tolower(static_cast<unsigned char>(c));
    return key;
}
std::string set_option(std::string url, const std::string &key, const std::string &value)
{
    const size_t fragment_at = url.find('#');
    const std::string fragment = fragment_at == std::string::npos ? "" : url.substr(fragment_at);
    if (fragment_at != std::string::npos) url.resize(fragment_at);
    const size_t query = url.find('?');
    std::vector<std::string> parts;
    if (query != std::string::npos) {
        std::istringstream stream(url.substr(query + 1));
        std::string part;
        while (std::getline(stream, part, '&')) {
            if (!part.empty() && option_key(part.substr(0, part.find('='))) != option_key(key))
                parts.push_back(part);
        }
        url.resize(query);
    }
    if (!value.empty()) parts.push_back(key + "=" + value);  // Numeric/codec values only.
    for (size_t i = 0; i < parts.size(); ++i) url += (i ? "&" : "?") + parts[i];
    return url + fragment;
}
}

std::string constrain_transcode_url(const std::string &url, const VideoProfile &profile)
{
    if (url.empty()) return url;
    VideoProfile bounded = video_profile(false, profile.width, profile.height);
    bounded.video_bitrate = std::max(1, std::min(profile.video_bitrate, kHdmiMaxVideoBitrate));
    bounded.audio_bitrate = std::max(1, std::min(profile.audio_bitrate, 128000));
    bounded.max_framerate = std::max(1, std::min(profile.max_framerate, 24));
    // An Emby-returned URL may pin a larger Width/Height or contain a different
    // casing of the options. Remove duplicates before applying device limits.
    std::string result = set_option(set_option(url, "Width", ""), "Height", "");
    for (const auto &option : std::vector<std::pair<std::string, std::string>>{
            {"VideoCodec", "h264"}, {"AudioCodec", "aac"},
            {"VideoBitrate", std::to_string(bounded.video_bitrate)},
            {"AudioBitrate", std::to_string(bounded.audio_bitrate)},
            {"MaxStreamingBitrate", std::to_string(bounded.video_bitrate + bounded.audio_bitrate)},
            {"MaxWidth", std::to_string(bounded.width)}, {"MaxHeight", std::to_string(bounded.height)},
            {"MaxFramerate", std::to_string(bounded.max_framerate)},
            {"MaxAudioChannels", "2"}, {"TranscodingMaxAudioChannels", "2"},
            {"allowVideoStreamCopy", "false"}, {"allowAudioStreamCopy", "false"}})
        result = set_option(result, option.first, option.second);
    return result;
}

std::string connected_hdmi_output(const std::string &drm_directory)
{
    DIR *dir = opendir(drm_directory.c_str());
    if (!dir) return {};
    std::vector<std::string> outputs;
    while (dirent *entry = readdir(dir)) {
        const std::string name = entry->d_name;
        const size_t marker = name.find("-HDMI-");
        if (marker == std::string::npos) continue;
        std::ifstream file(drm_directory + "/" + name + "/status");
        std::string status;
        if (file >> status && status == "connected")
            outputs.push_back(name.substr(marker + 1));
    }
    closedir(dir);
    std::sort(outputs.begin(), outputs.end());
    return outputs.empty() ? std::string() : outputs.front();
}

bool use_small_video_output(bool force_small, bool hdmi_connected,
                            bool desktop_available)
{
    return force_small || !hdmi_connected || !desktop_available;
}

std::vector<std::string> desktop_video_options(const std::string &output,
                                               bool paused)
{
    if (output.empty()) return {};
    return {
        "--no-config", "--fs-screen-name=" + output,
        // Map the first window at the output size, before the compositor's
        // fullscreen configure. A video-sized initial window can leave its
        // centered offset attached to the larger DMA-BUF surface on labwc.
        "--geometry=100%x100%", "--border=no",
        "--vo=dmabuf-wayland,wlshm", "--hwdec=v4l2m2m", "--vd-lavc-threads=2",
        "--vd-lavc-o=pixel_format=bgr0,num_capture_buffers=4,num_output_buffers=2",
        "--hwdec-extra-frames=0",
        "--sws-scaler=fast-bilinear", "--sws-fast=yes", "--sws-allow-zimg=no",
        "--demuxer-max-bytes=8MiB", "--demuxer-max-back-bytes=2MiB",
        "--demuxer-readahead-secs=4", "--speed=1",
        "--input-default-bindings=no", "--input-vo-keyboard=no", "--osc=no",
        "--cursor-autohide=always", paused ? "--pause=yes" : "--pause=no"
    };
}
}
