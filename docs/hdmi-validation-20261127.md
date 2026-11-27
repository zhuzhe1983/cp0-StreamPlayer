# HDMI DMA-BUF validation — 2026-11-27

This is a short physical-device compatibility and performance check, not a
full-film or universal network guarantee. The public data intentionally omits
private server addresses, account details and playback URLs.

## Pipeline and limits

- Ordinary UID 1000; existing Wayland desktop, mpv 0.40 and Raspberry Pi FFmpeg
  V4L2 decoder. No root process, DRM-master takeover, boot/CMA change, system
  service stop or Wi-Fi tuning is required by playback.
- HDMI mode stays 1920×1080 at 60 Hz. Emby H.264/AAC requests are bounded to
  1280×720, 24 fps and the target bitrate. Aspect ratio may produce a shorter
  decoded frame. Playback speed is exactly 1; no audio time stretching.
- `v4l2m2m` produces `drm_prime[bgr0]` frames for `dmabuf-wayland`. The compositor
  accepts this RGB format; a YUV DMA trial did not produce a usable picture.
- Capture buffers 4, compressed output buffers 2, extra hardware frames 0.
  The unbounded/default RGB pool exhausted the existing 64 MiB CMA allocation;
  reducing bitrate alone did not fix that. This release bounds the pool.

## Real Emby bitrate sweep

A is a real episode from a high-resolution source. B is a second episode,
C a movie. Each arm records 20 polling samples after startup; IPC adds time
beyond the nominal one-second sample interval. CPU is the mpv process's CPU
seconds divided by the observation wall time (100% = one core), excluding
server work. These are short backend tests; application controls are tested
separately below.

| Media | Requested video Mbps | Observed mean / peak Mbps | Interface RX Mbps | MPV CPU % | Dropped frames | Buffering polls / 20 |
|---|---:|---:|---:|---:|---:|---:|
| A | 5 | 4.96 / 6.61 | 5.59 | 13.3 | 0 | 0 |
| A | 8 | 7.94 / 10.56 | 10.23 | 13.3 | 0 | 0 |
| A | 12 | 11.69 / 15.85 | 14.22 | 15.3 | 0 | 0 |
| A | 16 | 15.84 / 17.54 | 19.08 | 17.3 | 0 | 0 |
| A | 20 | 20.07 / 26.78 | 23.19 | 20.1 | 0 | 0 |
| A | 24 | 21.04 / 27.76 | 21.82 | 19.0 | 0 | 5 |
| B | 8 | 8.00 / 8.66 | 9.71 | 13.4 | 0 | 0 |
| C | 8 | 7.98 / 9.28 | 9.49 | 13.8 | 0 | 0 |

The Wi-Fi link reported 72.2 Mbps, while three HTTP transfers of a 5.58 MB
payload measured 25.57, 25.49 and 17.95 Mbps. Link rate is not payload capacity;
that local transfer route is also not necessarily the Emby route. The 24 Mbps
arm buffered despite zero dropped frames. 20 Mbps was the highest short arm
without observed buffering. **The application's default is 8 Mbps video plus
128 kbps audio**, leaving room for bitrate peaks and changing Wi-Fi conditions.
This is not a measured absolute decoder ceiling. The raw video-bit-rate
property and whole-interface RX measure different things.

## Actual application

The exact m5stack20 binary uses the same bounded DMA path automatically when
HDMI and a desktop are available. The generated-media fixture requires the
QA token on playlists and segments, and exposes a Season container first.
Physical evdev/real framebuffer checks passed: Season → episodes, automatic
HDMI full screen, Space pause once, Tab to paused LCD, resume with changing
LCD pixels, LCD seek with authenticated HTTP segments, Tab back to HDMI,
short Esc back and long Esc clean exit. MPV reported `dmabuf-wayland`,
`v4l2m2m`, BGR0 DRM PRIME, 1280×720 and speed 1 on both HDMI starts. The
normal account configuration stayed byte-for-byte unchanged, and APPLaunch
was restored active. Debug frame logging was disabled for the accepted run.

The fixture media is generated, and does not replace the real Emby sweep.
Earlier trials include software scaling that dropped frames, a GPU-backend
crash, unsupported YUV DMA output, large-pool stalls, startup timeouts, and an
FFmpeg local-HLS `headers` failure. Those failed receipts remain in the local
experiment archive. The accepted local-HLS path uses authenticated same-origin
segment URLs in a private playlist and does not pass an HTTP-only option to
a file input; third-party URLs are not given the server token.

Subjective audio quality, Bluetooth hardware, every server codec, a full-film
run, disconnect recovery and other image/compositor versions are not claimed
as validated. The compatibility `wlshm` fallback has different performance.
