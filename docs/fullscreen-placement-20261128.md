# First fullscreen placement — 2026-11-28

A real playback on m5stack20 showed an approximately 320 px right / 180 px
down displacement. MPV reported fullscreen 1920×1080, zero pan/alignment/zoom
and the expected video dimensions, but the visible surface extended beyond
the bottom and right edges. A native decoder screenshot showed the complete,
symmetrically letterboxed image. Re-entering fullscreen immediately corrected
the same running playback. This identifies a window-placement/configure issue,
rather than a server resolution or an intentional crop.

m5stack21 initializes the window at `--geometry=100%x100%` with `--border=no`.
This avoids mapping a smaller centered decorated window before the fullscreen
configure. Wayland still owns placement. MPV preserves the video's aspect
ratio and the original black bars. DMA-BUF, buffer counts, bitrate, frame rate,
playback speed, display mode, audio and window-readiness handling are unchanged.

Validation on the physical device:

- Native desktop-option regression with ASan/UBSan, ARM build and strict
  Debian/listing preflight passed. All 19 installed files and dependencies
  matched; the existing account configuration stayed byte-for-byte unchanged.
- Generated border clips: two 16:9 and two 4:3 cold starts with the new options
  retained all four edges. The 4:3 video occupied x=240…1679 with equal black
  sidebars on the 1920×1080 output. Untuned standalone controls also passed,
  so these clips alone did not reproduce the original intermittent offset.
- The actual reported title, *The Dog Stars*, was opened through the app's
  search/controller path and checked both on first start and after LCD→HDMI.
  Both runs used the installed m5stack21 binary, `dmabuf-wayland`, `v4l2m2m`,
  speed 1 and the existing 1920×1080 mode. Pause/resume and output switching
  were exercised; APPLaunch returned active afterward.
- Each spatial check compares the same paused frame's native decoder image
  against the real HDMI screencopy, fitted to the screen without cropping.
  Both accepted frames contain substantial visible image data; black intro
  captures are explicitly excluded. Centered reference errors were 0.458 / 0.434
  on the 0…255 RGB scale, versus 37.797 / 38.827 for a reference displaced 320/180.
  The complete decoded 1280×720 canvas maps to 1920×1080; letterboxing inside
  that canvas remains symmetric and intact.

Early black/dark intro captures were rejected as insufficient spatial evidence.
A later attempt timed out during unscheduled output switches; its origin was
not established. Both failed/insufficient trials are retained locally. The
accepted final run used the app's existing control pipe to select the exact
reported title, with actual visible frames and a shifted-reference negative
control. Private server URLs, tokens and film screenshots are not published.

This is a targeted placement regression, not a new bitrate/performance sweep
or a guarantee for every compositor/image. The m5stack20 bitrate limits and
other playback limitations still apply. No new store review was submitted.
