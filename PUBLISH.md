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
- Final binary video playback, pause/resume, short Esc back and long Esc clean exit: passed on the actual framebuffer using real evdev input; long Esc was tested with press and release only (no injected repeat). Receipt: `/tmp/streamplayer-publish-qa/jellyfin-result.json`, exit code 0. Music decoding/playback timing, pause/resume and long Esc exit passed. The native mpv IPC reported time-pos 0.228806 s, 48 kHz mono decoded input, pause false → true, and app exit code 0; this is not a subjective listening test. Language switching EN → zh-CN → ja-JP → EN passed on the real framebuffer; SC used the private Noto SC font and Japanese used the system JP TTC face. All result receipts are archived under `artifacts/publish-20261125/`.
- Official CLI current metadata policy and installed final executable checksum: passed. Committed pool metadata and release manifest were fetched and checked after submission; SHA and declared permissions match the prepared package.

## Submission

- User explicitly requested submission; no further publication permission is needed.
- Publish PR: <https://github.com/CardputerZero/packages/pull/840>, head `18123c4a3d443962d9780fb6ff1943989fff6348`.
- Status: submitted, open and unmerged; final device acceptance passed. Physical-device launch/use/exit video and maintainer review pending.

Catalog uniqueness verified against all 191 `pool/main/*/meta.json` records at official main `21ea24cfdb8d082487a4030d2551d4615d827552`: no share-code or title collision for STRP / StreamPlayer. The same check passed for Calendar / CALE and CardputerAdv / CADV.

The store branch also removes obsolete privileged packaging helpers and their tests. Long Esc is monitored independently in the keyboard thread and requests the existing clean SIGINT shutdown, including without hardware repeat. Per-key debug output was removed so login input is not echoed to logs.

The CLI upload and metadata branch completed. PR creation returned HTTP 500, then HTTP 504; a subsequent direct query of that exact head branch found PR #840. No new branch or duplicate package upload was created. Final package was downloaded from the public buffer release onto the device, SHA-256 verified, forcibly reinstalled for same-version development replacement, and the installed executable checksum passed. Backup: `/home/pi/streamplayer-backups/20261125-234528`.

Final accepted executable: `150ad8e9a778058d63164041d94294071ecf9c20ffce97ef427e772feef2baf4`, installed from the actual public buffer-release package. The protocol fixture and generated media are limited QA evidence; production server compatibility and perceived sound quality are not claimed as tested. Original third-party license copies are retained byte-for-byte, including their upstream whitespace; source whitespace checks excluding those copied notice files passed.

Session cleanup: disposable QA credentials were archived and removed from normal startup configuration. APPLaunch user service is active, the installed final binary checksum matches, and the boot configuration hash is unchanged. LightDM stayed active. PackageKit naturally started and quit during the session (journal records `daemon quit` and successful deactivation after its idle period); it was active in the initial snapshot and inactive at the end. The app did not control it, and it was not restarted merely to force the snapshots to match. No unrelated background process was stopped.

The PR staging-capture job initially failed on GitHub's release-upload endpoint with HTTP 500 after successfully downloading the exact package and passing its checksum check. Requests to rerun the failed job also returned HTTP 500, with the run still at attempt 1. A final listing update pins the correct reduced source branch and its completion timestamp; the normal synchronize event will re-run the same package-capture checks. No package or policy check is bypassed.

The requested independent store worktree is retained because its branch is intentionally separate and unmerged, with store review still open. Task HTTP/fixture servers and SSH sessions were closed after archiving the actual-device receipts; normal startup has no QA account.

GitHub staging asset `792140019` is uploaded, 5,565,292 bytes, with digest `sha256:7ddcf0513aa64414dd026410aa461a7c6ca1748145b7513d5b93d991671d3c68`: the earlier capture failure was its HTTP 500 response after upload. Final listing metadata was pushed normally via Git as `18123c4a3d443962d9780fb6ff1943989fff6348`, adding the correct reduced-source link and acceptance timestamp, which triggers the normal capture/validation workflow again. The temporary sparse review checkout `/tmp/cp0-streamplayer-review-20261126` is retained while the review branch remains unmerged.

Final submission state: PR #840 remains open and unmerged. At head `18123c4a3d443962d9780fb6ff1943989fff6348`, `capture` and `validate` jobs both completed with success. As with the other same-repository maintainer submissions, these jobs do not imply full third-party package validation; the explicit local/current-policy and physical-device checks are recorded above. Required physical-device review video and maintainer review remain pending.

## Local keyboard compatibility fix — 2026-11-27

The reflashed 2026-11-19 image names the keyboard `tca8418c` and supplies
symbols on different evdev key codes. For example, `@` is code 27, `:` is
80, and `/` is 92. The submitted m5stack17 binary still used a built-in
183–213 symbol table, so it interpreted these keys using a standard PC
layout instead. The installed m5stack17 checksum matched the submission;
this was an image-compatibility defect rather than an older package.

The local fix reads the image's existing
`/usr/share/keymaps/tca8418_keypad_m5stack_keymap.map` (or
`LV_LINUX_KEYBOARD_MAP`) at keyboard initialization, on the TCA8418 device
only. Ordinary letters, modifiers and navigation retain xkbcommon behavior;
USB keyboards do not use the board's symbol map. Images without a usable
map retain the legacy fallback. It neither rewrites the system keymap nor
logs typed characters.

- Local version: `0.1.0-m5stack18`, arm64.
- Package: `artifacts/streamplayer_0.1.0-m5stack18_arm64.deb`; frozen copy in
  `artifacts/publish-20261127-keymap/`.
- Package SHA-256: `549ed1efa87093311442a9bee50d95118619826ab67e1c5dc4b20eafbb84b6fb`.
- Executable SHA-256: `b52a457a6fb1ee01950d1046318183e236080c125928d8257b3d50b2c678b097`.
- Portable C loader regression with AddressSanitizer/UndefinedBehaviorSanitizer:
  passed all 32 current symbols, legacy input, missing/malformed maps,
  duplicate entries and isolation of normal keys.
- ARM test using the production keyboard decoder and the device's real
  libinput/xkbcommon libraries: passed all 32 symbols on press/release,
  ordinary a/b, Shift-A then lowercase a, Backspace/Tab, legacy fallback
  and USB-keyboard isolation. This test opens no input device.
- Existing HLS-seek and LAN-scan regressions: passed. ARM framebuffer build
  and package construction: passed.
- Installation preserved the old package files and configuration under
  `/home/pi/streamplayer-backups/20261127T130609Z`; all 19 installed package
  files were verified against the archive. No prior server configuration
  files existed at backup time, and the normal configuration was not
  populated by this test.
- Real 320×170 framebuffer check with synthetic evdev input and a separate
  test configuration: `ht://a.b` displayed correctly in an input field,
  password input remained masked, and the new running process checksum
  matched the package. No test login was submitted. APPLaunch was restored
  active after the isolated test.
- This is a local device fix. PR #840 and its m5stack17 submission above
  have not been replaced or republished by this task. Production media
  playback was not retested for this keyboard-only change.
