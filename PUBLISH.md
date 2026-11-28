# Current delivery: 0.1.0-m5stack21

The exact tested m5stack21 is approved and live in the STRP store listing, with
`external_display: true`. Publication and screenshot updates are recorded at the
end. Earlier sections retain historical m5stack17/18 states and limitations;
they do not describe the current release.

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

## HDMI DMA-BUF release — 2026-11-27

User authorized real-device bitrate testing, automatic ordinary-user DMA-BUF
integration, source synchronization to GitHub and a validated new release.
The default branch remains `store-compliant-20261125`; the unrelated original
worktree and its unfinished numeric-key changes are preserved.

- Version: `0.1.0-m5stack20`, arm64.
- Package: `artifacts/streamplayer_0.1.0-m5stack20_arm64.deb`.
- Package SHA-256: `aafef86bfaab35f83f1dcda4f66c721f918a6cd055806b049b53641bbff42632`.
- Executable SHA-256: `6d222b16797743acef91f3c6f7f4af211011b7f873329f5fb8e79b9663872cbb`.
- Payload: 19 regular files; current strict prepublish/listing check passed.
  Runtime dependencies were checked with `ldd` on the device. Network and
  external_display are true; the other five store permissions are false.
- Backup before final installation:
  `/home/pi/streamplayer-hdmi-upgrade-20261127T133717Z/delivery/backups/20261127T153416Z`.
  Backup archive SHA-256:
  `97f6e870f620d7a087dc27aee5c524d812bf2d58349bc96d87a2480e2f3cb94f`.
  Prior installed version was m5stack19. All 19 installed payload checksums
  match, the existing account configuration is byte-for-byte unchanged, and
  APPLaunch user service is active.

HDMI automatically uses existing Wayland and mpv, V4L2 H.264 → BGR0 DRM PRIME
→ DMA-BUF. The frame pool is bounded to 4 capture / 2 compressed output
buffers and zero extra hardware frames. Requests respect the attached screen
and an upper profile of 1280×720, 24 fps, 8 Mbps video + 128 kbps AAC.
Server-returned URLs are bounded too. The app performs no root service,
CMA/boot, Wi-Fi or desktop-mode tuning. Playback speed stays 1. The `wlshm`
fallback has different performance and is not the claimed DMA fast path.

The same version also fixes Series/Season container navigation, real physical
Tab handling, paused LCD/HDMI switching, and local HLS seeks. The seek fallback
fetches the full playlist once before trimming, permits only its own private
local playlist to read HTTP(S) segments, and preserves authentication using
same-origin segment/key/map URLs without sending the token to other origins.
It avoids passing an HTTP-only `headers` option to a file input.

Validation completed:

- Native ASan/UBSan: desktop connector/profile/URL/keyboard options, HLS seek
  and authentication rewriting, LAN discovery and the 32-symbol keymap tests.
- ARM framebuffer SCons build and Debian package construction passed.
- Accepted app fixture run (v15), with debug logging off and media authentication
  required: Season → episodes, automatic HDMI 1280×720 DMA output, Space pause,
  Tab to paused LCD, resume with changing framebuffer pixels, seek with
  authenticated HTTP segments, Tab back to DMA HDMI, short Esc back and long
  Esc clean exit; account configuration unchanged and launcher restored.
- Final installed binary was launched as UID 1000 with the existing real Emby
  account. The actual movie “一切如你所见 / You Can See Everything” played through
  `dmabuf-wayland` + `v4l2m2m`, with no buffering or dropped-frame increments in
  10 polling samples, speed 1, and MPV CPU about 12.8% of one core. Physical Space
  pause/resume and long Esc passed. These short checks do not claim full-film
  or subjective audio-quality validation.

[The public validation note](docs/hdmi-validation-20261127.md) records all six
real-Emby bitrate arms and two other-media controls, including 24 Mbps buffering,
plus sanitized numeric results. 20 Mbps was the highest short arm without
observed buffering; 8 Mbps is the conservative app default. Wi-Fi payload
capacity measured about 18–26 Mbps, separately from its 72.2 Mbps link rate.
Earlier failed YUV/GPU/large-pool/startup/seek trials are retained in the private
local task archive, not discarded or presented as successes. Generated fixture
media and real-server measurements are identified separately.

Release verified: [v0.1.0-m5stack20](https://github.com/zhuzhe1983/cp0-StreamPlayer/releases/tag/v0.1.0-m5stack20),
published 2026-11-27 15:41:47 UTC from source
`bf85143e92361aad388efcbb3e6667961b4a4626`. Its 5,578,596-byte Debian asset
reports GitHub digest `sha256:aafef86bfaab35f83f1dcda4f66c721f918a6cd055806b049b53641bbff42632`,
matching the exact tested/installed package. `SHA256SUMS` is attached too.
The source is pushed to the public default/store branch.

The current official Python czdev metadata policy passed and created
[new review PR #980](https://github.com/CardputerZero/packages/pull/980).
The user then explicitly said not to submit a new review yet and confirmed
having supplied a video with the old version. PR #980 was therefore closed,
with no merge or deployment. The older [PR #840](https://github.com/CardputerZero/packages/pull/840)
and all its submitted video/comments remain unchanged. m5stack20 is a GitHub
release and a verified local installation, **not a newly approved store release**.
The new review is not being pursued; no replacement video is requested here.

## Fullscreen placement correction — 2026-11-28

- Version `0.1.0-m5stack21`, ARM64, installed on the physical device.
- Package SHA-256: `8817f8bfc7b8ecaef28eb6adfcda507eac6b0df83d313a4872df0f64a8dcf2be`.
- Executable SHA-256: `abbe56362f18c796e1d5716d809349054ea8e43923ce0cb869206389fe486dbe`.
- Backup before installation: `/home/pi/streamplayer-center-20261127/delivery/backups/20261127T160213Z`.
- Prior installed version: m5stack20. All 19 payload files and runtime
  dependencies verified; account configuration unchanged; APPLaunch active.
- The reported *The Dog Stars* playback is centered both on initial startup
  and after LCD→HDMI, verified using visible same-frame decoder/HDMI images
  and a displaced-reference negative control. Generated 16:9/4:3 border clips,
  ASan/UBSan option regression, ARM build and package preflight passed.
- [Placement evidence and limitations](docs/fullscreen-placement-20261128.md).
  Natural aspect ratio and source letterboxing are preserved. DMA settings,
  bitrate, playback speed and desktop mode are unchanged.
- GitHub source/binary update authorized by the existing delivery request.
  New application-store review remains deferred as explicitly requested.
  Verified GitHub release: [v0.1.0-m5stack21](https://github.com/zhuzhe1983/cp0-StreamPlayer/releases/tag/v0.1.0-m5stack21).
  The uploaded asset is 5,578,340 bytes and its GitHub digest matches the
  tested package SHA-256 above. No new store review was created.

## App Store update authorized — 2026-11-28

The user explicitly reauthorized publication after confirming m5stack21 on the
physical device. This supersedes the earlier temporary deferral. The exact
5,578,340-byte package is reused: SHA-256
`8817f8bfc7b8ecaef28eb6adfcda507eac6b0df83d313a4872df0f64a8dcf2be`,
with executable SHA-256
`abbe56362f18c796e1d5716d809349054ea8e43923ce0cb869206389fe486dbe`.
Strict local preflight, archive ownership/modes and current official metadata
policy passed. STRP and the existing app identity are preserved; the listing
now declares ordinary-user HDMI/DMA support and external_display permission.

The official current Python czdev initially created same-repository #1024.
Its workflows completed, but the bot explicitly reported **Validation skipped**
and its trusted workflow deliberately excludes maintainer-path auto-merging.
#1024 was closed and the exact same metadata commit
`497ea9db99dfc6e57d00356158a1be0192e9c274` submitted from the normal contributor
fork as [PR #1025](https://github.com/CardputerZero/packages/pull/1025).
No package rebuild or manual merge is involved. The org-controlled upload and
staging bytes are reused, and the existing-app full-validation/automatic-merge
workflow ran successfully. PR #1025 was automatically merged at
`2026-11-28T02:32:27Z` as `adf504fcebe01c2c8f8951cf6a837e92d741c3ff`.
The bot explicitly reported **Package Validation Passed**, including all 19
payload files, ARM64 ELF and the exact package SHA. Full validation run:
<https://github.com/CardputerZero/packages/actions/runs/46098533241>.
Deployment run <https://github.com/CardputerZero/packages/actions/runs/46098583682>
and website deployment <https://github.com/CardputerZero/cardputerzero.github.io/actions/runs/46098741562>
completed successfully. The user-facing, global and China catalogs all deliver
approved m5stack21 with `external_display: true`; the public apt-pool download
was fetched and independently hashed to the exact tested package SHA above.
The installed program was not rebuilt or reinstalled for publication.

## Store screenshots refreshed — 2026-11-28

The user requested meaningful UI/playback captures and replacement of diagnostic
color bars. The listing now uses four clean native 320×170 PNGs:

| Asset | Actual content and provenance |
| --- | --- |
| `store/screenshots/library.png` | Movie library, captured from the installed m5stack21 device. |
| `store/screenshots/lcd-movie.png` | Real *Shrek Forever After* frame on the LCD, captured from m5stack21; original aspect/letterboxing preserved. |
| `store/screenshots/hdmi-movie-controls.png` | The same real movie's HDMI controls on the LCD, captured from m5stack21. |
| `store/screenshots/setup.png` | Retained empty-account setup UI; no private server/account information. |

The generated fixture/color-bar captures and black introduction attempts remain
private validation records and are not store assets. Device account configuration
hash stayed unchanged, APPLaunch remained active, and the app was returned to its
movie detail screen. No package rebuild, installation or system-setting change
was needed.

Strict package/listing preflight and the current official metadata policy passed.
The normal contributor-fork [metadata PR #1026](https://github.com/CardputerZero/packages/pull/1026)
contains only `meta.json` and three PNG changes; the existing manifest, binary,
STRP/UUID and `external_display: true` are preserved. Validation
[46099954449](https://github.com/CardputerZero/packages/actions/runs/46099954449)
passed with the explicit **Package Validation Passed** bot result. It auto-merged
at `2026-11-28T02:50:13Z` as `d6b40a46156323a8b2c975b994d27e532ee88a6c`.
The merged image blobs match the checked source bytes and dimensions.
Deployment [46099991405](https://github.com/CardputerZero/packages/actions/runs/46099991405)
and website deployment [46100123223](https://github.com/CardputerZero/cardputerzero.github.io/actions/runs/46100123223)
succeeded. All three catalogs then listed four screenshots; the China mirror
served all four exact files, while the unchanged GitHub branch URL still cached
the earlier fixture controls.

To avoid that stale-image URL, the identical real-movie controls PNG was renamed
to `hdmi-movie-controls.png`. [Follow-up PR #1027](https://github.com/CardputerZero/packages/pull/1027)
changes only that asset path and metadata. Full validation
[46100455714](https://github.com/CardputerZero/packages/actions/runs/46100455714)
passed; automatic merge completed at `2026-11-28T02:56:46Z`, commit
`60b1026de44f4c3e56b21d697ac6d3b388da0365`. Deployment
[46100489425](https://github.com/CardputerZero/packages/actions/runs/46100489425)
completed successfully, as did website deployment
[46100612752](https://github.com/CardputerZero/cardputerzero.github.io/actions/runs/46100612752).
The user-facing, global and China catalogs now all list the four final asset
paths. Every public PNG was downloaded from its catalog URL and matches the
source SHA-256 and native 320×170 dimensions. Both global and China package
downloads still match the exact tested m5stack21 SHA-256; HDMI support is YES.
No review, deployment or image-verification step remains pending for this update.
