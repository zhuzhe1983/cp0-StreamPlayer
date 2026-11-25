# StreamPlayer store edition

This branch is the user-authorized reduced Application Store edition. The original development working tree is preserved separately.

Emby/Jellyfin music and LCD video playback, search, subtitles, and English/Chinese/Japanese UI remain. The server must supply the 320x170 H.264 LCD profile. LCD software decoding avoids privileged CMA reservation. Music uses mpv; video uses FFmpeg.

Removed from this edition: root systemd guards, sudoers and NetworkManager dispatcher files, boot/CMA and Wi-Fi tuning, PackageKit/LightDM control, HDMI video/photo playback and automatic HDMI mode changes. The application never starts, stops or masks system services or changes desktop settings. Existing manual system settings are not undone.

Build with `./scripts/build_deb.sh`; install with `sudo apt install ./artifacts/streamplayer_0.1.0-m5stack17_arm64.deb` so dependencies are resolved. The app runs as the APPLaunch user. Source branch: `store-compliant-20261125`.

Set your own server URL and account in the GUI. Never commit populated configuration, credentials or playback logs. The default copy contains no account or server endpoint. Original application code is MIT; bundled Noto font is OFL, with both notices included in the package. See `PUBLISH.md` for verified limitations and submission status.

The old deployment helper is retained as historical developer reference; use the store build and apt installation commands above for this branch. No legacy service/tuning payload or maintainer scripts are shipped.
