#!/usr/bin/env bash
set -euo pipefail

stream_script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
stream_project_dir="$(CDPATH= cd -- "${stream_script_dir}/.." && pwd)"
stream_binary="${stream_project_dir}/dist/M5CardputerZero-StreamPlayer"
stream_control="${stream_project_dir}/packaging/control"
stream_artifacts="${stream_project_dir}/artifacts"
stream_version="$(awk -F': ' '$1 == "Version" { print $2; exit }' "${stream_control}")"
stream_deb="${stream_artifacts}/streamplayer_${stream_version}_arm64.deb"
stream_stage_root="$(mktemp -d /tmp/streamplayer-deb.XXXXXX)"
stream_stage="${stream_stage_root}/debian-streamplayer"

cleanup_stream_stage() {
    case "${stream_stage_root}" in
        /tmp/streamplayer-deb.*) rm -rf -- "${stream_stage_root}" ;;
    esac
}
trap cleanup_stream_stage EXIT

if [[ "${1:-}" != "--no-build" ]]; then
    command -v scons >/dev/null 2>&1 || {
        echo "scons is required to cross-compile StreamPlayer" >&2
        exit 1
    }
    (
        cd "${stream_project_dir}"
        env CardputerZero=1 scons
    )
fi

command -v dpkg-deb >/dev/null 2>&1 || {
    echo "dpkg-deb is required to build the Debian package" >&2
    exit 1
}

test -x "${stream_binary}" || {
    echo "missing AArch64 binary: ${stream_binary}" >&2
    exit 1
}

mkdir -p \
    "${stream_stage}/DEBIAN" \
    "${stream_stage}/usr/share/APPLaunch/applications" \
    "${stream_stage}/usr/share/APPLaunch/bin" \
    "${stream_stage}/usr/share/APPLaunch/fonts" \
    "${stream_stage}/usr/share/APPLaunch/share/font" \
    "${stream_stage}/usr/share/APPLaunch/share/images" \
    "${stream_stage}/usr/lib/streamplayer" \
    "${stream_stage}/usr/lib/systemd/system" \
    "${stream_stage}/etc/sudoers.d" \
    "${stream_stage}/etc/NetworkManager/dispatcher.d" \
    "${stream_artifacts}"

install -m 0644 "${stream_control}" "${stream_stage}/DEBIAN/control"
for stream_maintainer_script in postinst prerm postrm; do
    sh -n "${stream_project_dir}/packaging/${stream_maintainer_script}"
    install -m 0755 "${stream_project_dir}/packaging/${stream_maintainer_script}" \
        "${stream_stage}/DEBIAN/${stream_maintainer_script}"
done
install -m 0755 "${stream_binary}" \
    "${stream_stage}/usr/share/APPLaunch/bin/M5CardputerZero-StreamPlayer"
install -m 0644 "${stream_project_dir}/applications/stream-player.desktop" \
    "${stream_stage}/usr/share/APPLaunch/applications/stream-player.desktop"
install -m 0644 "${stream_project_dir}/share/images/stream-player.png" \
    "${stream_stage}/usr/share/APPLaunch/share/images/stream-player.png"
install -m 0644 "${stream_project_dir}/fonts/NotoSansSC-Regular.ttf" \
    "${stream_stage}/usr/share/APPLaunch/share/font/NotoSansSC-Regular.ttf"
ln -s ../share/font/NotoSansSC-Regular.ttf \
    "${stream_stage}/usr/share/APPLaunch/fonts/NotoSansSC-Regular.ttf"
sh -n "${stream_project_dir}/packaging/prepare-system"
sh -n "${stream_project_dir}/packaging/apply-runtime-tuning"
sh -n "${stream_project_dir}/packaging/validate-runtime"
sh -n "${stream_project_dir}/packaging/networkmanager-dispatcher"
sh -n "${stream_project_dir}/packaging/resource-guard"
install -m 0755 "${stream_project_dir}/packaging/prepare-system" \
    "${stream_stage}/usr/lib/streamplayer/prepare-system"
install -m 0755 "${stream_project_dir}/packaging/apply-runtime-tuning" \
    "${stream_stage}/usr/lib/streamplayer/apply-runtime-tuning"
install -m 0755 "${stream_project_dir}/packaging/validate-runtime" \
    "${stream_stage}/usr/lib/streamplayer/validate-runtime"
install -m 0755 "${stream_project_dir}/packaging/resource-guard" \
    "${stream_stage}/usr/lib/streamplayer/resource-guard"
install -m 0644 "${stream_project_dir}/packaging/streamplayer-tuning.service" \
    "${stream_stage}/usr/lib/systemd/system/streamplayer-tuning.service"
install -m 0644 "${stream_project_dir}/packaging/streamplayer-background-guard.service" \
    "${stream_stage}/usr/lib/systemd/system/streamplayer-background-guard.service"
install -m 0644 "${stream_project_dir}/packaging/streamplayer-display-guard.service" \
    "${stream_stage}/usr/lib/systemd/system/streamplayer-display-guard.service"
install -m 0755 "${stream_project_dir}/packaging/networkmanager-dispatcher" \
    "${stream_stage}/etc/NetworkManager/dispatcher.d/90-streamplayer-wifi-power-save"
install -m 0440 "${stream_project_dir}/packaging/streamplayer.sudoers" \
    "${stream_stage}/etc/sudoers.d/streamplayer"
if command -v visudo >/dev/null 2>&1; then
    visudo -cf "${stream_stage}/etc/sudoers.d/streamplayer" >/dev/null
fi

dpkg-deb --root-owner-group --build "${stream_stage}" "${stream_deb}"

echo
echo "Built: ${stream_deb}"
echo "Install with apt so Depends and Recommends are resolved:"
echo "  sudo apt install ./${stream_deb##*/}"
