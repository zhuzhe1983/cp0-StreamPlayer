#!/usr/bin/env bash
set -euo pipefail

stream_host="${1:-${CARDPUTER_ZERO_HOST:-}}"
stream_user="${2:-${CARDPUTER_ZERO_USER:-}}"
if [[ -z "${stream_host}" || -z "${stream_user}" ]]; then
    echo "usage: $0 <host> <ssh-user>" >&2
    echo "or set CARDPUTER_ZERO_HOST and CARDPUTER_ZERO_USER" >&2
    exit 2
fi

stream_project_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
stream_version="$(awk -F': ' '$1 == "Version" { print $2; exit }' \
    "${stream_project_dir}/packaging/control")"
stream_deb="${stream_project_dir}/artifacts/streamplayer_${stream_version}_arm64.deb"
stream_remote_deb="/tmp/streamplayer_${stream_version}_arm64.deb"
stream_local_binary="${stream_project_dir}/dist/M5CardputerZero-StreamPlayer"
stream_ssh=(
    ssh
    -o StrictHostKeyChecking=no
    -o KexAlgorithms=curve25519-sha256
    -o ConnectTimeout=30
    -o ServerAliveInterval=15
    -o ServerAliveCountMax=60
)
stream_scp=(
    scp
    -o StrictHostKeyChecking=no
    -o KexAlgorithms=curve25519-sha256
    -o ConnectTimeout=30
    -o ServerAliveInterval=15
    -o ServerAliveCountMax=60
)

if [[ -n "${CARDPUTER_ZERO_SSH_PASSWORD:-}" ]]; then
    command -v sshpass >/dev/null 2>&1 || {
        echo "sshpass is required when CARDPUTER_ZERO_SSH_PASSWORD is set" >&2
        exit 1
    }
    export SSHPASS="${CARDPUTER_ZERO_SSH_PASSWORD}"
    stream_ssh+=(
        -o PreferredAuthentications=password
        -o PubkeyAuthentication=no
        -o NumberOfPasswordPrompts=1
    )
    stream_scp+=(
        -o PreferredAuthentications=password
        -o PubkeyAuthentication=no
        -o NumberOfPasswordPrompts=1
    )
    stream_ssh=(sshpass -e "${stream_ssh[@]}")
    stream_scp=(sshpass -e "${stream_scp[@]}")
fi

"${stream_project_dir}/scripts/build_deb.sh"
test -f "${stream_deb}"
test -x "${stream_local_binary}"

stream_backup_dir="$("${stream_ssh[@]}" "${stream_user}@${stream_host}" \
    'set -eu
     backup_dir="/home/pi/backups/streamplayer-$(date +%Y%m%d-%H%M%S)"
     mkdir -p "${backup_dir}/rootfs"
     for installed_path in \
         /usr/share/APPLaunch/bin/M5CardputerZero-StreamPlayer \
         /usr/share/APPLaunch/applications/stream-player.desktop \
         /usr/share/APPLaunch/share/images/stream-player.png \
         /usr/share/APPLaunch/share/font/NotoSansSC-Regular.ttf \
         /usr/share/APPLaunch/fonts/NotoSansSC-Regular.ttf \
         /etc/sudoers.d/streamplayer \
         /etc/NetworkManager/dispatcher.d/90-streamplayer-wifi-power-save \
         /usr/lib/systemd/system/streamplayer-tuning.service \
         /usr/lib/streamplayer/prepare-system \
         /usr/lib/streamplayer/apply-runtime-tuning \
         /usr/lib/streamplayer/validate-runtime \
         /usr/lib/streamplayer/resource-guard \
         /usr/lib/systemd/system/streamplayer-background-guard.service \
         /usr/lib/systemd/system/streamplayer-display-guard.service \
         /var/lib/streamplayer \
         /boot/firmware/config.txt; do
         if [ -e "${installed_path}" ] || [ -L "${installed_path}" ]; then
             relative_path=${installed_path#/}
             sudo -n mkdir -p -- "${backup_dir}/rootfs/$(dirname "${relative_path}")"
             sudo -n cp -a -- "${installed_path}" "${backup_dir}/rootfs/${relative_path}"
         fi
     done
     dpkg-query -W streamplayer > "${backup_dir}/package-status.txt" 2>/dev/null || true
     dpkg-query -L streamplayer > "${backup_dir}/package-files.txt" 2>/dev/null || true
     sha256sum /boot/firmware/config.txt > "${backup_dir}/boot-config.sha256" 2>/dev/null || true
     printf "%s\n" "${backup_dir}"')"

"${stream_scp[@]}" "${stream_deb}" \
    "${stream_user}@${stream_host}:${stream_remote_deb}"

stream_run_remote_install()
{
    "${stream_ssh[@]}" "${stream_user}@${stream_host}" \
        "sh -s -- '${stream_remote_deb}' '${stream_version}'" \
        <<'STREAMPLAYER_REMOTE_INSTALL'
set -eu

stream_remote_deb=$1
stream_expected_version=$2
stream_user_uid=$(id -u)
stream_user_runtime_dir="/run/user/${stream_user_uid}"
stream_applaunch_was_active=0
stream_lightdm_was_active=0
stream_packagekit_was_active=0
stream_system_baseline_captured=0

if XDG_RUNTIME_DIR="${stream_user_runtime_dir}" \
   systemctl --user is-active --quiet APPLaunch.service; then
    stream_applaunch_was_active=1
fi

stream_restore_system_service()
{
    stream_service_name=$1
    stream_service_was_active=$2
    if [ "${stream_service_was_active}" -eq 1 ]; then
        if ! sudo -n /usr/bin/systemctl start "${stream_service_name}" \
                >/dev/null 2>&1 ||
           ! sudo -n /usr/bin/systemctl is-active --quiet \
                "${stream_service_name}"; then
            printf '%s\n' \
                "streamplayer deploy: failed to restore active ${stream_service_name}" >&2
            return 1
        fi
    else
        if ! sudo -n /usr/bin/systemctl stop "${stream_service_name}" \
                >/dev/null 2>&1 ||
           sudo -n /usr/bin/systemctl is-active --quiet \
                "${stream_service_name}"; then
            printf '%s\n' \
                "streamplayer deploy: failed to restore inactive ${stream_service_name}" >&2
            return 1
        fi
    fi
}

stream_restore_install_state()
{
    stream_original_status=$?
    trap - EXIT HUP INT TERM
    set +e
    stream_restore_status=0

    if [ "${stream_system_baseline_captured}" -eq 1 ]; then
        if ! stream_restore_system_service \
                packagekit.service "${stream_packagekit_was_active}"; then
            stream_restore_status=1
        fi
        if ! stream_restore_system_service \
                lightdm.service "${stream_lightdm_was_active}"; then
            stream_restore_status=1
        fi
    fi

    if [ "${stream_original_status}" -ne 0 ]; then
        if [ "${stream_applaunch_was_active}" -eq 1 ]; then
            if ! XDG_RUNTIME_DIR="${stream_user_runtime_dir}" \
                    systemctl --user restart APPLaunch.service >/dev/null 2>&1 ||
               ! XDG_RUNTIME_DIR="${stream_user_runtime_dir}" \
                    systemctl --user is-active --quiet APPLaunch.service; then
                printf '%s\n' \
                    "streamplayer deploy: failed to restore active APPLaunch.service" >&2
                stream_restore_status=1
            fi
        else
            if ! XDG_RUNTIME_DIR="${stream_user_runtime_dir}" \
                    systemctl --user stop APPLaunch.service >/dev/null 2>&1 ||
               XDG_RUNTIME_DIR="${stream_user_runtime_dir}" \
                    systemctl --user is-active --quiet APPLaunch.service; then
                printf '%s\n' \
                    "streamplayer deploy: failed to restore inactive APPLaunch.service" >&2
                stream_restore_status=1
            fi
        fi
    fi

    if [ "${stream_original_status}" -ne 0 ]; then
        exit "${stream_original_status}"
    fi
    exit "${stream_restore_status}"
}

trap stream_restore_install_state EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

XDG_RUNTIME_DIR="${stream_user_runtime_dir}" \
    systemctl --user stop APPLaunch.service
if XDG_RUNTIME_DIR="${stream_user_runtime_dir}" \
   systemctl --user is-active --quiet APPLaunch.service; then
    printf '%s\n' "streamplayer deploy: APPLaunch.service remained active" >&2
    exit 1
fi

for stream_executable_link in /proc/[0-9]*/exe; do
    [ -L "${stream_executable_link}" ] || continue
    stream_executable=$(readlink "${stream_executable_link}" 2>/dev/null || true)
    case "${stream_executable}" in
        /usr/share/APPLaunch/bin/M5CardputerZero-StreamPlayer|\
        "/usr/share/APPLaunch/bin/M5CardputerZero-StreamPlayer (deleted)")
            printf '%s\n' \
                "streamplayer deploy: exit StreamPlayer before deployment" >&2
            exit 1
            ;;
    esac
done

for stream_guard_service in \
    streamplayer-display-guard.service \
    streamplayer-background-guard.service; do
    stream_guard_load_state=$(sudo -n /usr/bin/systemctl show \
        "${stream_guard_service}" --property=LoadState --value 2>/dev/null || true)
    if [ "${stream_guard_load_state}" = loaded ]; then
        if ! sudo -n /usr/bin/systemctl stop \
                "${stream_guard_service}" >/dev/null 2>&1; then
            printf '%s\n' \
                "streamplayer deploy: failed to stop ${stream_guard_service}" >&2
            exit 1
        fi
    fi
    if sudo -n /usr/bin/systemctl is-active --quiet \
            "${stream_guard_service}"; then
        printf '%s\n' \
            "streamplayer deploy: ${stream_guard_service} remained active" >&2
        exit 1
    fi
done

if [ -x /usr/lib/streamplayer/resource-guard ]; then
    sudo -n /usr/lib/streamplayer/resource-guard restore-display
    sudo -n /usr/lib/streamplayer/resource-guard restore-background
fi
for stream_guard_state in display background; do
    if [ -d "/run/streamplayer-resource-guard/${stream_guard_state}" ]; then
        printf '%s\n' \
            "streamplayer deploy: stale ${stream_guard_state} guard state remains" >&2
        exit 1
    fi
done

# An app that was running before deployment may have held these services
# through the StreamPlayer guards.  Capture their real baseline only after the
# app and both guards have released and restored it.
if sudo -n /usr/bin/systemctl is-active --quiet lightdm.service; then
    stream_lightdm_was_active=1
fi
if sudo -n /usr/bin/systemctl is-active --quiet packagekit.service; then
    stream_packagekit_was_active=1
fi
stream_system_baseline_captured=1

sudo -n /usr/bin/systemctl stop lightdm.service
if sudo -n /usr/bin/systemctl is-active --quiet lightdm.service; then
    printf '%s\n' "streamplayer deploy: lightdm.service remained active" >&2
    exit 1
fi

sudo -n /usr/bin/systemctl stop packagekit.service
if sudo -n /usr/bin/systemctl is-active --quiet packagekit.service; then
    printf '%s\n' "streamplayer deploy: packagekit.service remained active" >&2
    exit 1
fi

sudo -n /usr/bin/env DEBIAN_FRONTEND=noninteractive \
    /usr/bin/apt-get install -y --reinstall "${stream_remote_deb}"
test "$(dpkg-query -W -f='${db:Status-Status}' streamplayer)" = installed
test "$(dpkg-query -W -f='${db:Status-Abbrev}' streamplayer)" = 'ii '
test "$(dpkg-query -W -f='${Version}' streamplayer)" = "${stream_expected_version}"
stream_audit_output=$(dpkg --audit)
test -z "${stream_audit_output}"
if grep -Fqx streamplayer /run/reboot-required.pkgs 2>/dev/null &&
   [ "$(awk '$1 == "CmaTotal:" { print $2; exit }' /proc/meminfo)" -lt 49152 ]; then
    echo 'StreamPlayer installed, but a reboot is required before validation.' >&2
    exit 75
fi

XDG_RUNTIME_DIR="${stream_user_runtime_dir}" \
    systemctl --user restart APPLaunch.service
test "$(XDG_RUNTIME_DIR="${stream_user_runtime_dir}" \
    systemctl --user is-active APPLaunch.service)" = active
test -x /usr/share/APPLaunch/bin/M5CardputerZero-StreamPlayer
grep -Fqx 'Exec=/usr/share/APPLaunch/bin/M5CardputerZero-StreamPlayer' \
    /usr/share/APPLaunch/applications/stream-player.desktop
/usr/lib/streamplayer/validate-runtime
sudo -n -l \
    /usr/bin/systemctl start streamplayer-background-guard.service >/dev/null
sudo -n -l \
    /usr/bin/systemctl start streamplayer-display-guard.service >/dev/null
sudo -n /usr/lib/streamplayer/prepare-system status
systemctl is-active streamplayer-tuning.service
exit 0
STREAMPLAYER_REMOTE_INSTALL
}

# macOS ships Bash 3.2, whose parser can incorrectly expand a quoted heredoc
# when the heredoc is nested directly inside a quoted command substitution.
# Keep the remote program in a function so its variables are expanded only by
# the target's shell.
set +e
stream_install_output="$(stream_run_remote_install)"
stream_install_status=$?
set -e
printf '%s\n' "${stream_install_output}"
if [[ "${stream_install_status}" -ne 0 && "${stream_install_status}" -ne 75 ]]; then
    exit "${stream_install_status}"
fi

stream_local_checksum="$(sha256sum "${stream_local_binary}" | awk '{ print $1 }')"
stream_remote_checksum="$("${stream_ssh[@]}" "${stream_user}@${stream_host}" \
    "sha256sum /usr/share/APPLaunch/bin/M5CardputerZero-StreamPlayer | awk '{ print \$1 }'")"
if [[ "${stream_local_checksum}" != "${stream_remote_checksum}" ]]; then
    echo "deployed binary checksum mismatch" >&2
    exit 1
fi

echo "Installed StreamPlayer ${stream_version} on ${stream_host}."
if [[ "${stream_install_status}" -eq 75 ]]; then
    echo "A reboot is required before hardware playback validation."
fi
echo "Backup: ${stream_backup_dir}"
echo "Binary SHA256: ${stream_remote_checksum}"
