# StreamPlayer for CardputerZero

Browse/search Emby and Jellyfin libraries, play music, and watch video on the
built-in LCD or a connected HDMI screen. Version `0.1.0-m5stack21` keeps the
refreshed-image keyboard-symbol fix and restores ordinary-user HDMI video.

Version m5stack21 also initializes the HDMI window at the full output size
without decorations, avoiding a first-start DMA fullscreen placement offset.
Video retains its aspect ratio and any letterboxing; it is not cropped to fit.

Connect HDMI before opening a video: it starts on the external display and
the LCD keeps the playback controls. **Tab** switches LCD/HDMI, **Space**
pauses/resumes, **O** cycles audio output, short **Esc** returns, and holding
**Esc** exits. Without a connected HDMI desktop it uses the LCD.

HDMI uses mpv through the existing Wayland compositor. Server requests are
bounded by the connected screen and the device profile: at most 1280×720,
24 fps, 8 Mbps H.264 video and 128 kbps AAC audio. Returned server URLs are
constrained too, including duplicate option names with different casing.
This is a conservative working profile, not a claim about the chip's absolute
decoder limit or the speed of every Wi-Fi connection. The screen mode stays
unchanged; the compositor scales the video while preserving its aspect ratio.

On the supported Raspberry Pi image, V4L2 decodes directly into BGR0 DMA-BUF
frames accepted by the desktop. A bounded frame pool avoids exhausting the
existing CMA allocation. This does not need root at runtime. The `wlshm`
compatibility fallback has different performance. The internal LCD keeps its
320×170 server-transcoded profile. Photo playback is not enabled. Audio keeps
its original playback speed. The application does not install root guards or
sudoers, change boot/CMA or Wi-Fi settings, or start/stop PackageKit/LightDM.

The short real-Emby sweep reached 20 Mbps without observed buffering; at
24 Mbps buffering appeared. Three local Wi-Fi payload tests measured about
18–26 Mbps, below the advertised link rate. The 8 Mbps default leaves headroom
for variable-bitrate peaks and changing reception. See [measurements and
limits](docs/hdmi-validation-20261127.md) for the complete sweep and the failed
trials; it is not a full-film or universal playback guarantee.

Series and season directories open their episodes instead of being sent to
PlaybackInfo. The private local HLS seek fallback explicitly permits its
HTTP/HTTPS segments; unrelated local files do not get that extra permission.

Build with `./scripts/build_deb.sh`; install with
`sudo apt install ./artifacts/streamplayer_0.1.0-m5stack21_arm64.deb` so runtime
dependencies are resolved. The app runs as the APPLaunch user; HDMI requires
an existing Wayland desktop and mpv. Source branch: `store-compliant-20261125`.

Configure your own server URL and account in the GUI. Never commit populated
configuration, credentials or playback logs. The default package contains no
account or server endpoint. Original application code is MIT; bundled Noto
font is OFL, with the notices included. `PUBLISH.md` distinguishes local
upgrades from the older package still under store review.

The legacy deployment helper is historical reference. Use the build and apt
commands above; this branch ships no legacy service/tuning payload.

For supported images the DMA settings are automatic; users need no sudo
command or boot-memory adjustment. On an older image without the required
V4L2 BGR0 DMA-BUF support, use the LCD fallback or update to a supported image.
`M5_STREAMPLAYER_DEBUG_MPV=1` enables an optional private player log for
diagnosis; keep it private because playback URLs can contain credentials.
