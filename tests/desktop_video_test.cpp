#include "desktop_video.h"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>

int main()
{
    char temp[] = "/tmp/streamplayer-desktop-test.XXXXXX";
    const char *directory = mkdtemp(temp);
    assert(directory);
    const std::filesystem::path root(directory);
    auto connector = [&](const char *name, const char *status) {
        std::filesystem::create_directories(root / name);
        std::ofstream(root / name / "status") << status;
    };
    connector("card1-SPI-1", "connected\n");
    connector("card0-Writeback-1", "unknown\n");
    connector("card0-HDMI-A-1", "disconnected\n");
    assert(streamplayer::connected_hdmi_output(root.string()).empty());
    connector("card0-HDMI-A-2", "connected\n");
    assert(streamplayer::connected_hdmi_output(root.string()) == "HDMI-A-2");
    connector("card0-HDMI-A-1", "connected\n");
    assert(streamplayer::connected_hdmi_output(root.string()) == "HDMI-A-1");
    connector("card0-HDMI-A-1", "unknown\n");
    assert(streamplayer::connected_hdmi_output(root.string()) == "HDMI-A-2");
    connector("card0-HDMI-A-2", "disconnected\n");
    assert(streamplayer::connected_hdmi_output(root.string()).empty());
    assert(streamplayer::connected_hdmi_output((root / "missing").string()).empty());
    // A connected screen is not enough without a desktop, and explicit LCD
    // selection always wins. Never substitute the internal SPI display.
    assert(!streamplayer::use_small_video_output(false, true, true));
    assert(streamplayer::use_small_video_output(false, false, true));
    assert(streamplayer::use_small_video_output(false, true, false));
    assert(streamplayer::use_small_video_output(true, true, true));
    assert(streamplayer::desktop_video_options("", false).empty());
    const auto graphics = root / "graphics";
    std::filesystem::create_directories(graphics / "fb0");
    std::filesystem::create_directories(graphics / "fb1");
    std::ofstream(graphics / "fb0/name") << "panel-mipi-dbid\n";
    std::ofstream(graphics / "fb0/virtual_size") << "320,170\n";
    std::ofstream(graphics / "fb1/name") << "vc4drmfb\n";
    std::ofstream(graphics / "fb1/virtual_size") << "1920,1080\n";
    assert(streamplayer::desktop_output_size(graphics.string()) == std::make_pair(1920,1080));
    auto profile = streamplayer::video_profile(false,1920,1080);
    assert(profile.width == 1280 && profile.height == 720);
    assert(profile.video_bitrate == streamplayer::kHdmiMaxVideoBitrate);
    profile = streamplayer::video_profile(false, 641, 481);
    assert(profile.width == 640 && profile.height == 480);
    assert(profile.video_bitrate <= streamplayer::kHdmiMaxVideoBitrate);
    const auto fallback = streamplayer::video_profile(false,0,0);
    assert(fallback.width == 1280 && fallback.height == 720);
    const auto small = streamplayer::video_profile(true,1920,1080);
    assert(small.width == 320 && small.height == 170 && small.video_bitrate == 500000);
    const std::string bounded = streamplayer::constrain_transcode_url(
        "https://example.invalid/hls?VideoBitrate=20000000&videobitrate=15000000&width=3840&Height=2160&"
        "MaxWidth=3840&maxWidth=1920&api_key=preserve%2Btoken#keep", profile);
    assert(bounded.find("VideoBitrate=" + std::to_string(profile.video_bitrate)) != std::string::npos);
    assert(bounded.find("MaxStreamingBitrate=" + std::to_string(profile.video_bitrate + profile.audio_bitrate)) != std::string::npos);
    assert(bounded.find("MaxWidth=640") != std::string::npos);
    assert(bounded.find("MaxHeight=480") != std::string::npos);
    assert(bounded.find("videobitrate=") == std::string::npos);
    assert(bounded.find("maxWidth=") == std::string::npos);
    assert(bounded.find("width=") == std::string::npos);
    assert(bounded.find("Height=2160") == std::string::npos);
    assert(bounded.find("api_key=preserve%2Btoken") != std::string::npos);
    assert(bounded.substr(bounded.size()-5) == "#keep");
    assert(streamplayer::constrain_transcode_url(bounded,profile) == bounded);
    const auto excessive = streamplayer::constrain_transcode_url("https://example.invalid/hls",
        {3840,2160,20000000,500000,120});
    assert(excessive.find("MaxWidth=1280") != std::string::npos);
    assert(excessive.find("MaxHeight=720") != std::string::npos);
    assert(excessive.find("MaxStreamingBitrate=" + std::to_string(streamplayer::kHdmiMaxVideoBitrate+128000)) != std::string::npos);
    assert(excessive.find("MaxFramerate=24") != std::string::npos);
    std::ofstream(graphics / "fb1/virtual_size") << "bad\n";
    assert(streamplayer::desktop_output_size(graphics.string()) == std::make_pair(0,0));
    for (bool paused : {false, true}) {
        const auto options = streamplayer::desktop_video_options("HDMI-A-2", paused);
        auto has = [&](const char *s) { return std::find(options.begin(), options.end(), s) != options.end(); };
        assert(has("--fs-screen-name=HDMI-A-2"));
        assert(has("--geometry=100%x100%"));
        assert(has("--border=no"));
        assert(has("--speed=1"));
        assert(has("--vo=dmabuf-wayland,wlshm"));
        assert(has("--hwdec=v4l2m2m"));
        assert(has("--vd-lavc-o=pixel_format=bgr0,num_capture_buffers=4,num_output_buffers=2"));
        assert(has("--hwdec-extra-frames=0"));
        // The app owns keyboard handling. MPV must not also consume the same
        // physical Space/Tab/Esc events through the desktop compositor.
        assert(has("--input-vo-keyboard=no"));
        assert(has("--input-default-bindings=no"));
        assert(has(paused ? "--pause=yes" : "--pause=no"));
        for (const auto &option : options) {
            assert(option.find("drm-device") == std::string::npos);
            assert(option.find("fb0") == std::string::npos);
        }
    }
    std::filesystem::remove_all(root);
    std::cout << "desktop output: connector/disconnect, internal LCD isolation, output policy, keyboard ownership and pause/speed options passed\n";
}
