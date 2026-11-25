#!/usr/bin/env bash
set -euo pipefail
stream_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
if [[ "${1:-}" != "--no-build" ]]; then
    (cd "$stream_root"; CardputerZero=1 scons)
fi
stream_version="$(awk -F': ' '$1 == "Version" {print $2; exit}' "$stream_root/packaging/control")"
stream_artifacts="$stream_root/artifacts"
mkdir -p "$stream_artifacts"
stream_stage="$(mktemp -d "$stream_artifacts/store-stage.XXXXXX")"
trap 'rm -rf -- "$stream_stage"' EXIT
install -d "$stream_stage/DEBIAN" "$stream_stage/usr/share/APPLaunch/applications" \
    "$stream_stage/usr/share/APPLaunch/bin" "$stream_stage/usr/share/APPLaunch/share/images" \
    "$stream_stage/usr/share/streamplayer/fonts" "$stream_stage/usr/share/doc/streamplayer"
install -m 0644 "$stream_root/packaging/control" "$stream_stage/DEBIAN/control"
install -m 0755 "$stream_root/dist/M5CardputerZero-StreamPlayer" "$stream_stage/usr/share/APPLaunch/bin/"
install -m 0644 "$stream_root/applications/stream-player.desktop" "$stream_stage/usr/share/APPLaunch/applications/"
install -m 0644 "$stream_root/share/images/stream-player.png" "$stream_stage/usr/share/APPLaunch/share/images/"
install -m 0644 "$stream_root/fonts/NotoSansSC-Regular.ttf" "$stream_stage/usr/share/streamplayer/fonts/"
install -m 0644 "$stream_root/LICENSE" "$stream_stage/usr/share/doc/streamplayer/copyright"
install -m 0644 "$stream_root/packaging/Noto-OFL.txt" "$stream_stage/usr/share/doc/streamplayer/Noto-OFL.txt"
cp -R "$stream_root/packaging/licenses" "$stream_stage/usr/share/doc/streamplayer/third-party"
dpkg-deb --root-owner-group -Zxz -b "$stream_stage" "$stream_artifacts/streamplayer_${stream_version}_arm64.deb"
