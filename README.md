# CardputerZero StreamPlayer

LVGL-based stream player app for M5Stack CardputerZero.

## Repository Layout

- `main/src/` - application, keyboard input, and media controller source code
- `main/include/` - application headers
- `main/ui/` - generated LVGL UI sources and image assets
- `fonts/` - runtime CJK font packaged with the app
- `*.sh` - deployment and debug helper scripts
- `SConstruct` and `main/SConstruct` - SCons build scripts
- `config_defaults.mk` and `setup.ini` - default build and project configuration

Generated build outputs, toolchain sysroots, and runtime poster caches are intentionally ignored by git.

## Server defaults from the environment

StreamPlayer does not embed a server address or account in the binary. When no
saved app configuration is available, it reads these optional environment
variables:

- `M5_STREAMPLAYER_BASE_URL`
- `M5_STREAMPLAYER_USERNAME`
- `M5_STREAMPLAYER_PASSWORD`
- `M5_STREAMPLAYER_CACHE_DIR` (optional poster cache root; default `/home/pi/.cache/m5streamplayer`)

For APPLaunch, inject them with a protected systemd `EnvironmentFile`; do not
commit a populated credentials file to this repository.

## Runtime payload and dependencies

The `.deb` contains the StreamPlayer binary, APPLaunch desktop entry and icon,
runtime validation and system-tuning helpers, systemd guards, and
`fonts/NotoSansSC-Regular.ttf`. The font is installed at
`/usr/share/APPLaunch/share/font/NotoSansSC-Regular.ttf`, with the compatibility
symlink `/usr/share/APPLaunch/fonts/NotoSansSC-Regular.ttf`. This
application-owned font covers Simplified/Traditional Chinese, Japanese, Latin,
Greek, Cyrillic, and common symbols. The required `fonts-dejavu-core` package
provides a DejaVu Sans fallback for additional Unicode scripts and monochrome
symbols that are absent from Noto Sans SC.

Required Debian dependencies are declared in `packaging/control` and are
installed by apt:

| Purpose | Packages |
| --- | --- |
| C/C++ runtime and device input | `libc6`, `libgcc-s1`, `libstdc++6`, `libinput10`, `libxkbcommon0`, `libudev1` |
| Emby/Jellyfin HTTPS requests | `curl`, `ca-certificates` |
| Unicode fallback glyphs | `fonts-dejavu-core` |
| Video and audio playback | `gstreamer1.0-tools`, `gstreamer1.0-plugins-base`, `gstreamer1.0-plugins-good`, `gstreamer1.0-plugins-bad`, `gstreamer1.0-libav`, `gstreamer1.0-alsa`, `alsa-utils` |
| Device tuning and HDMI mode control | `util-linux`, `systemd`, `sudo`, `iw`, `wlr-randr` |

`gst-launch-1.0`, used to assemble the hardware-decoding playback pipelines,
comes from `gstreamer1.0-tools`. Installation also validates the required
GStreamer elements (`playbin`, `v4l2h264dec`, `kmssink`, `fbdevsink`, and
`alsasink`) and access to the V4L2 decoder, DRM/KMS device, and LCD framebuffer.

`ffmpeg`, `mpv`, `pipewire-bin`, `wireplumber`, and `bluez` are optional
`Recommends`; the primary CardputerZero LCD and HDMI hardware paths do not
depend on them.

## Build the installable Debian package

Cross-compile and package StreamPlayer with:

```bash
./scripts/build_deb.sh
```

Install with apt so Debian resolves both dependency sets:

```bash
sudo apt install ./artifacts/streamplayer_0.1.0-m5stack16_arm64.deb
```

Do not use `dpkg -i` alone; it does not fetch missing dependencies.
The repository deployment helper records the APPLaunch, LightDM, and
PackageKit state, temporarily stops them while apt processes the package on the
256 MiB device, and restores the recorded state even when installation fails.

## System adjustments made by the package

The package performs these idempotent setup steps:

- **CMA reservation:** on CardputerZero systems with less than 512 MiB RAM and
  less than 48 MiB CMA, it changes the existing `vc4-kms-v3d` overlay to
  reserve 64 MiB CMA for V4L2 hardware decoding. It records the exact previous
  line before editing it.
- **Wi-Fi stability:** it disables Wi-Fi power saving at boot and reapplies the
  setting after NetworkManager reconnects. The original setting for each
  wireless interface is recorded.
- **Narrow privilege bridge:** it grants `pi` passwordless sudo access only to
  start the PackageKit and display guards. It does not grant permission to stop
  arbitrary services, a general shell, or unrestricted systemd access.
- **Runtime validation and privacy:** it validates commands, GStreamer
  elements, hardware-device access, and guard permissions as the `pi` launcher
  user. Existing StreamPlayer configuration and log files are tightened to
  user-only permissions.
- **Low-memory guard:** while StreamPlayer is open, PackageKit is stopped and
  runtime-masked to prevent package-index refreshes from competing for memory.
- **Foreground display guard:** while StreamPlayer is open, LightDM is stopped
  so its desktop session does not consume scarce RAM or compete for HDMI KMS.
  APPLaunch and StreamPlayer continue to use the independent CardputerZero LCD
  framebuffer; LightDM is restored only after StreamPlayer exits.

For diagnostics or a legacy desktop playback backend,
`M5_STREAMPLAYER_KEEP_LIGHTDM=1` skips the foreground LightDM pause at app
startup. A later KMS or small-LCD hardware playback request can still acquire
the display guard when exclusive output is required. Leaving this override set
reduces the RAM available to StreamPlayer.

The PackageKit and LightDM changes are held by root-owned guards. Their recorded
pre-launch state is restored after normal exit, a crash, `SIGKILL`, or an OOM
kill. Each guard identifies the exact StreamPlayer executable once, records its
PID, process group, and kernel start time, and then follows only that identity;
it does not continuously rescan all of `/proc`, and PID reuse cannot extend the
guard lifetime. These are not permanent service disables.

The package does not restart APPLaunch from inside `postinst`, because an
Application Store installation may itself be running in APPLaunch's cgroup.
The deployment helper restarts APPLaunch only after `apt` has returned; the
Application Store can perform its normal launcher refresh after installation.

If CMA needs to change, installation creates `/run/reboot-required` and prints a
reboot notice. StreamPlayer also refuses to start hardware playback while the
old, undersized CMA reservation is still active, so the failure is explicit.
On package removal, the previous boot and Wi-Fi power-save settings are
restored only when they have not subsequently been edited by the user. If the
package cannot restore an app-managed setting safely, removal stops with an
explicit error instead of silently overwriting a newer system configuration.

Playback defaults are 320×170 at 500 Kbps for the built-in LCD and 1920×1080 at
24 fps with 3.8 Mbps video plus 128 Kbps audio for HDMI (about 3.928 Mbps
total). Both GStreamer paths prefer the V4L2 H.264 decoder and use a
9-second/4-MiB compressed-stream buffer. A cold server transcode may take up to
60 seconds to start; playback is reported to the server only after the pipeline
reaches PLAYING and the hardware decoder device is verified.

Inspect the installed tuning state with:

```bash
sudo /usr/lib/streamplayer/prepare-system status
systemctl status streamplayer-tuning.service
/usr/sbin/iw dev wlan0 get power_save
```
