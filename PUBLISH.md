# Application Store Publish Record

Keep this file in the application project root. Update it when package
metadata, listing copy, assets, privacy choices, or submission status changes.

Portal: <https://dev.cardputer.cc/#/upload>

## Package

- Debian package path: `artifacts/streamplayer_0.1.0-m5stack15_arm64.deb`
- Package: `streamplayer`
- Version: `0.1.0-m5stack15`
- Architecture: `arm64`
- Maintainer: `Zhu Zhe <zhuzhe1983@gmail.com>`
- APPLaunch desktop path: `/usr/share/APPLaunch/applications/stream-player.desktop`
- Executable path: `/usr/share/APPLaunch/bin/M5CardputerZero-StreamPlayer`

## Source and Ownership

- Public source repository: <https://github.com/zhuzhe1983/cp0-StreamPlayer>
- Expected GitHub uploader/owner: `zhuzhe1983`
- Hide uploader email: `true`

## Listing

- Application name: `StreamPlayer`
- One-line summary (maximum 80 characters): `Emby and Jellyfin player for CardputerZero LCD and 1080p HDMI.`
- Description: use the reviewed Chinese or English
  `store.locales.<locale>.description` from `app-builder.json`. It explains the
  required apt dependencies, bundled Noto Sans SC font, the
  `fonts-dejavu-core` Unicode fallback, optional recommended packages,
  64 MiB CMA/reboot behavior, Wi-Fi power-save tuning, and the temporary
  PackageKit/LightDM guards and their restoration behavior.
- Categories (maximum 6): `Media`, `Video`
- AppBuilder capabilities: `keyboard`, `audio`, `network`, `filesystem`,
  `process`
- Store permissions: `audio_output=true`, `network=true`, `filesystem=full`,
  `keyboard_input=true`, `background_service=true`,
  `external_hardware=true`, `hdmi_output=true`
- Full-filesystem rationale: package installation and removal can manage the
  CardputerZero CMA boot setting under `/boot`, install narrowly scoped
  service/sudo/NetworkManager files under `/etc` and `/usr`, keep reversible
  state under `/var`, and use runtime guards under `/run`. The listing
  description documents these adjustments and their restoration behavior.

## Assets

- 256 x 256 source icon: `share/images/stream-player.png`
- Icon packaged in `.deb`: `/usr/share/APPLaunch/share/images/stream-player.png`
- Screenshot 1 (320 x 170): TODO
- Screenshot 2 (320 x 170): TODO
- Screenshot 3 (320 x 170): TODO
- Screenshot 4 (320 x 170): TODO
- Optional screenshot 5 (portal supports up to 6): TODO
- Optional screenshot 6 (portal supports up to 6): TODO

## Preflight

- [x] `.deb` exists and can be parsed.
- [x] Control fields contain Package, Version, Architecture, and Maintainer.
- [x] Architecture is appropriate for the package (`arm64` for native binaries).
- [x] APPLaunch `.desktop`, executable, PNG icon, and bundled font exist inside the package.
- [ ] Package name ownership and update version are valid.
- [ ] Listing copy and categories have been reviewed.
- [ ] Four clean 320 x 170 baseline screenshots exist.
- [x] No credentials, tokens, or private server details are embedded.
- [ ] The user explicitly approved external submission.

## Submission Result

- Status: `not submitted`
- Submitted at: TODO
- Portal message: TODO
- Actions URL: TODO
- Tracking or pull-request URL: TODO
