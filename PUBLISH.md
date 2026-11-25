# StreamPlayer store edition publication

User authorized publication and a separate reduced branch on 2026-11-25; the original uncommitted UI changes remain in the original working tree. Original application license: MIT, explicitly selected by the user.

## Store branch and package

- Branch: `store-compliant-20261125`; project: `StreamPlayer-store`.
- Version: `0.1.0-m5stack17`, `arm64`.
- Package: `artifacts/streamplayer_0.1.0-m5stack17_arm64.deb`; a copy and QA files are retained in `artifacts/publish-20261125/`.
- SHA-256: `7ddcf0513aa64414dd026410aa461a7c6ca1748145b7513d5b93d991671d3c68`.
- Executable SHA-256: `150ad8e9a778058d63164041d94294071ecf9c20ffce97ef427e772feef2baf4`.
- Maintainer: `Zhu Zhe <1917345+zhuzhe1983@users.noreply.github.com>`; public source: <https://github.com/zhuzhe1983/cp0-StreamPlayer>, store branch listed above; hide email true.
- Original source worktree has not been reset or overwritten. This branch starts at committed `03bccbb`; its separate unfinished numeric-key UI changes remain in the original worktree.
- Build with `CardputerZero=1 scons`, then `./scripts/build_deb.sh --no-build`. Existing SDK/sysroot reused; no global SDK installation.

## Reduced functionality

- Keeps Emby/Jellyfin browsing, search, music and server-transcoded 320×170 LCD H.264 video.
- No root systemd services, sudoers, NetworkManager dispatcher, boot/CMA or Wi-Fi tuning, PackageKit/LightDM control, HDMI mode changes or HDMI video/photo output are installed or requested.
- LCD uses FFmpeg software H.264 decoding with one decoder thread. Music uses mpv. GStreamer is not a required dependency of this edition. Audio retains its natural rate; no time-stretch or speed adjustment is used.
- Core LVGL and bundled-library notices, application MIT and Noto OFL notices are packaged. The bundled SC font is private to `/usr/share/streamplayer/fonts`; Japanese selects the system JP TTC face. Language switching deletes the old labels and reloads the font family before rebuilding the screen.
- Current `app-builder.json` is the authoritative store copy. Category Media, author Zhu Zhe / zhuzhe1983, license MIT, share code STRP. Network true; all other six store permissions false.
- Source icon: `share/images/stream-player.png`. Screenshot: `store/screenshots/setup.png` (clean actual-device 320×170 image; no private account or server data).

## Validation and limitations

- ARM64 cross-build and strict local package/listing checks passed. Current server install-path policy passed; no root services or privileged commands remain in runtime code. Dependencies installed through apt; installed files were backed up before each deployment.
- Device: `192.168.199.179`, UID1000. Initial backup: `/home/pi/streamplayer-backups/20261125-231155`; subsequent development replacements have separate timestamped backups.
- Physical-device test uses generated test-pattern video and a generated 440 Hz tone with the local protocol fixture `tests/store_fixture.py`. These checks do not establish compatibility with a production Emby/Jellyfin server, every transcode format, Bluetooth hardware, or perceived audio quality. Real-device user video remains required for store review.
- GStreamer plugin discovery blocked during the first trial; this edition therefore uses FFmpeg for LCD playback. Actual later framebuffer capture showed the generated moving test pattern and the decoder ran as pi.
- Final binary music, video, pause, return, language and long-Esc checks: in progress.
- Official CLI metadata policy and final checksum verification: pending final run.

## Submission

- User explicitly requested submission; no further publication permission is needed.
- Publish PR: pending.
- Status: preparing final validation; physical-device launch/use/exit video and maintainer review pending.

Catalog uniqueness verified against all 191 `pool/main/*/meta.json` records at official main `21ea24cfdb8d082487a4030d2551d4615d827552`: no share-code or title collision for STRP / StreamPlayer. The same check passed for Calendar / CALE and CardputerAdv / CADV.

The store branch also removes obsolete privileged packaging helpers and their tests. Long Esc is monitored independently in the keyboard thread and requests the existing clean SIGINT shutdown, including without hardware repeat. Per-key debug output was removed so login input is not echoed to logs.
