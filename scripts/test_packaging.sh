#!/usr/bin/env bash
set -euo pipefail

test_script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
test_project_dir="$(CDPATH= cd -- "${test_script_dir}/.." && pwd)"
test_root="$(mktemp -d /tmp/streamplayer-packaging-test.XXXXXX)"

cleanup_packaging_test() {
    case "${test_root}" in
        /tmp/streamplayer-packaging-test.*) rm -rf -- "${test_root}" ;;
    esac
}
trap cleanup_packaging_test EXIT

run_prepare_system() {
    STREAMPLAYER_STATE_DIR="${test_root}/state" \
    STREAMPLAYER_BOOT_CONFIG="${test_root}/config.txt" \
    STREAMPLAYER_MEMINFO="${test_root}/meminfo" \
    STREAMPLAYER_RUNTIME_DIR="${test_root}/run" \
        "${test_project_dir}/packaging/prepare-system" "$@"
}

mkdir -p "${test_root}/run"
printf '%s\n' 'another-package-needs-reboot' > "${test_root}/run/reboot-required"
printf '%s\n' 'another-package' > "${test_root}/run/reboot-required.pkgs"
printf '%s\n' \
    'MemTotal:         231000 kB' \
    'CmaTotal:           8192 kB' \
    'CmaFree:            7000 kB' > "${test_root}/meminfo"
printf '%s\n' \
    'arm_64bit=1' \
    'dtoverlay=vc4-kms-v3d,cma-256' \
    'dtparam=audio=on' > "${test_root}/config.txt"

run_prepare_system apply
grep -Fqx 'dtoverlay=vc4-kms-v3d,cma-64' "${test_root}/config.txt"
grep -Fqx 'dtoverlay=vc4-kms-v3d,cma-256' "${test_root}/state/cma-original-line"
grep -Fqx streamplayer "${test_root}/run/reboot-required.pkgs"
grep -Fqx 'another-package-needs-reboot' "${test_root}/run/reboot-required"
grep -Fqx 'another-package' "${test_root}/run/reboot-required.pkgs"

first_checksum="$(sha256sum "${test_root}/config.txt" | awk '{ print $1 }')"
run_prepare_system apply
second_checksum="$(sha256sum "${test_root}/config.txt" | awk '{ print $1 }')"
test "${first_checksum}" = "${second_checksum}"

run_prepare_system restore
grep -Fqx 'dtoverlay=vc4-kms-v3d,cma-256' "${test_root}/config.txt"

rm -rf -- "${test_root}/state"
printf '%s\n' \
    'arm_64bit=1' \
    'dtoverlay=vc4-kms-v3d,cma-64' > "${test_root}/config.txt"
run_prepare_system apply
test ! -e "${test_root}/state/cma-original-line"

rm -rf -- "${test_root}/state"
printf '%s\n' \
    'arm_64bit=1' \
    'dtoverlay=vc4-kms-v3d,cma-256' > "${test_root}/config.txt"
run_prepare_system apply
sed -i.bak 's/cma-64/cma-96/' "${test_root}/config.txt"
run_prepare_system restore
grep -Fqx 'dtoverlay=vc4-kms-v3d,cma-96' "${test_root}/config.txt"
test -e "${test_root}/state/restore-conflict"

rm -rf -- "${test_root}/state"
printf '%s\n' \
    'arm_64bit=1' \
    'dtoverlay=vc4-kms-v3d, cma-256' > "${test_root}/config.txt"
run_prepare_system apply
grep -Fqx 'dtoverlay=vc4-kms-v3d,cma-64' "${test_root}/config.txt"
run_prepare_system restore
grep -Fqx 'dtoverlay=vc4-kms-v3d, cma-256' "${test_root}/config.txt"

rm -rf -- "${test_root}/state"
printf '%s\n' \
    'arm_64bit=1' \
    'dtoverlay=vc4-kms-v3d,cma-640' > "${test_root}/config.txt"
run_prepare_system apply
grep -Fqx 'dtoverlay=vc4-kms-v3d,cma-64' "${test_root}/config.txt"
run_prepare_system restore
grep -Fqx 'dtoverlay=vc4-kms-v3d,cma-640' "${test_root}/config.txt"

rm -rf -- "${test_root}/state"
printf '%s\n' \
    'arm_64bit=1' \
    'dtoverlay=vc4-kms-v3d,cma-256' > "${test_root}/config.txt"
run_prepare_system apply
chmod 0555 "${test_root}"
if run_prepare_system restore; then
    echo "restore unexpectedly succeeded in a read-only directory" >&2
    exit 1
fi
chmod 0755 "${test_root}"
test -e "${test_root}/state/restore-failed"
grep -Fqx 'dtoverlay=vc4-kms-v3d,cma-64' "${test_root}/config.txt"

wifi_root="${test_root}/wifi"
wifi_state_dir="${wifi_root}/state"
wifi_sys_class="${wifi_root}/sys/class/net"
wifi_device_state="${wifi_root}/device-state"
wifi_iw="${wifi_root}/iw"
mkdir -p "${wifi_sys_class}/wlan0/wireless"
printf '%s\n' on > "${wifi_device_state}"
cat > "${wifi_iw}" <<'EOF'
#!/bin/sh
set -eu
if [ "$1" != dev ] || [ "$2" != wlan0 ]; then
    exit 2
fi
case "$3" in
    get)
        [ "${FAKE_IW_GET_FAIL:-0}" != 1 ] || exit 1
        printf 'Power save: %s\n' "$(sed -n '1p' "${FAKE_IW_STATE}")"
        ;;
    set)
        [ "$4" = power_save ]
        sleep 0.05
        printf '%s\n' "$5" > "${FAKE_IW_STATE}"
        ;;
    *)
        exit 2
        ;;
esac
EOF
chmod 0755 "${wifi_iw}"

run_wifi_tuning() {
    STREAMPLAYER_STATE_DIR="${wifi_state_dir}" \
    STREAMPLAYER_SYS_CLASS_NET="${wifi_sys_class}" \
    STREAMPLAYER_TUNING_LOCK="${wifi_root}/tuning.lock" \
    STREAMPLAYER_IW_COMMAND="${wifi_iw}" \
    FAKE_IW_STATE="${wifi_device_state}" \
    FAKE_IW_GET_FAIL="${FAKE_IW_GET_FAIL:-0}" \
        "${test_project_dir}/packaging/apply-runtime-tuning" "$@"
}

run_wifi_tuning apply &
wifi_apply_one=$!
run_wifi_tuning apply &
wifi_apply_two=$!
wait "${wifi_apply_one}"
wait "${wifi_apply_two}"
grep -Fqx on "${wifi_state_dir}/wifi-power-save.wlan0"
grep -Fqx off "${wifi_device_state}"
run_wifi_tuning restore
grep -Fqx on "${wifi_device_state}"
test ! -e "${wifi_state_dir}/wifi-power-save.wlan0"

run_wifi_tuning apply
rm -rf -- "${wifi_sys_class}/wlan0/wireless"
if run_wifi_tuning restore; then
    echo "Wi-Fi restore unexpectedly succeeded without the interface" >&2
    exit 1
fi
test -e "${wifi_state_dir}/wifi-power-save.wlan0"
mkdir -p "${wifi_sys_class}/wlan0/wireless"
run_wifi_tuning restore
grep -Fqx on "${wifi_device_state}"

if FAKE_IW_GET_FAIL=1 run_wifi_tuning apply; then
    echo "Wi-Fi tuning unexpectedly succeeded without readable state" >&2
    exit 1
fi
grep -Fqx on "${wifi_device_state}"
test ! -e "${wifi_state_dir}/wifi-power-save.wlan0"

resource_guard_root="${test_root}/resource-guard"
resource_guard_systemctl_state="${test_root}/fake-systemctl-state"
resource_guard_systemctl="${test_root}/fake-systemctl"
install -m 0755 \
    "${test_project_dir}/tests/fake_resource_guard_systemctl.sh" \
    "${resource_guard_systemctl}"

run_resource_guard_restore() {
    FAKE_SYSTEMCTL_STATE_DIR="${resource_guard_systemctl_state}" \
    FAKE_SYSTEMCTL_START_DELAY="${FAKE_SYSTEMCTL_START_DELAY:-0.1}" \
    FAKE_SYSTEMCTL_FAIL_START="${FAKE_SYSTEMCTL_FAIL_START:-}" \
    STREAMPLAYER_SYSTEMCTL_COMMAND="${resource_guard_systemctl}" \
    STREAMPLAYER_RESOURCE_GUARD_STATE_DIR="${resource_guard_root}" \
    STREAMPLAYER_RESTORE_WAIT_SECONDS=3 \
        "${test_project_dir}/packaging/resource-guard" "$1"
}

# These state directories represent a guard whose main process was terminated
# normally, by SIGKILL, or by the OOM killer before ExecStopPost runs.
mkdir -p "${resource_guard_root}/background" "${resource_guard_root}/display"
: > "${resource_guard_root}/background/was-active"
: > "${resource_guard_root}/background/masked-by-streamplayer"
: > "${resource_guard_root}/display/was-active"
run_resource_guard_restore restore-background
run_resource_guard_restore restore-display
test ! -e "${resource_guard_root}"
grep -Fq -- '--no-block start packagekit.service' \
    "${resource_guard_systemctl_state}/calls.log"
grep -Fq -- '--no-block start lightdm.service' \
    "${resource_guard_systemctl_state}/calls.log"

# A failed restore must retain state so a later stop/prerm can retry it.
rm -f -- "${resource_guard_systemctl_state}/active.lightdm_service"
mkdir -p "${resource_guard_root}/display"
: > "${resource_guard_root}/display/was-active"
if FAKE_SYSTEMCTL_FAIL_START=lightdm.service \
   run_resource_guard_restore restore-display; then
    echo "display restore unexpectedly succeeded after a failed start" >&2
    exit 1
fi
test -d "${resource_guard_root}/display"
run_resource_guard_restore restore-display
test ! -e "${resource_guard_root}"

resource_guard_proc="${test_root}/fake-proc"
streamplayer_test_executable="/opt/streamplayer/M5CardputerZero-StreamPlayer"

make_fake_streamplayer_process() {
    process_id=$1
    process_starttime=$2
    process_group=$3
    process_executable=$4
    mkdir -p "${resource_guard_proc}/${process_id}"
    rm -f -- "${resource_guard_proc}/${process_id}/exe"
    ln -s "${process_executable}" "${resource_guard_proc}/${process_id}/exe"
    printf '%s\n' \
        "${process_id} (M5CardputerZero) S 1 ${process_group} 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 ${process_starttime}" \
        > "${resource_guard_proc}/${process_id}/stat"
}

run_resource_guard() {
    FAKE_SYSTEMCTL_STATE_DIR="${resource_guard_systemctl_state}" \
    STREAMPLAYER_SYSTEMCTL_COMMAND="${resource_guard_systemctl}" \
    STREAMPLAYER_RESOURCE_GUARD_STATE_DIR="${resource_guard_root}" \
    STREAMPLAYER_PROC_ROOT="${resource_guard_proc}" \
    STREAMPLAYER_EXECUTABLE="${streamplayer_test_executable}" \
    STREAMPLAYER_GUARD_POLL_SECONDS=1 \
        "${test_project_dir}/packaging/resource-guard" "$1"
}

wait_for_guard_ready() {
    ready_path=$1
    for _guard_wait in $(seq 1 100); do
        [ -f "${ready_path}" ] && return 0
        sleep 0.05
    done
    printf '%s\n' "resource guard did not become ready: ${ready_path}" >&2
    return 1
}

wait_for_guard_exit() {
    guard_pid=$1
    for _guard_wait in $(seq 1 100); do
        if ! kill -0 "${guard_pid}" 2>/dev/null; then
            wait "${guard_pid}"
            return 0
        fi
        sleep 0.05
    done
    kill "${guard_pid}" 2>/dev/null || true
    wait "${guard_pid}" 2>/dev/null || true
    printf '%s\n' "resource guard did not release its tracked process" >&2
    return 1
}

# Capture exactly one process identity at startup. A newer matching process is
# selected once; changing its kernel start time simulates PID reuse. The older
# matching process remains present to prove the wait loop does not rescan /proc.
rm -rf -- \
    "${resource_guard_root}" \
    "${resource_guard_proc}" \
    "${resource_guard_systemctl_state}"
mkdir -p "${resource_guard_systemctl_state}"
: > "${resource_guard_systemctl_state}/active.packagekit_service"
make_fake_streamplayer_process 310 1000 300 \
    "${streamplayer_test_executable}"
make_fake_streamplayer_process 320 2000 300 \
    "${streamplayer_test_executable}"
run_resource_guard background \
    > "${test_root}/background-guard.log" 2>&1 &
background_guard_pid=$!
wait_for_guard_ready "${resource_guard_root}/background/ready"
grep -Fqx 320 "${resource_guard_root}/background/tracked-pid"
grep -Fqx 2000 "${resource_guard_root}/background/tracked-starttime"
grep -Fqx 300 "${resource_guard_root}/background/tracked-pgrp"
test -f "${resource_guard_systemctl_state}/masked.packagekit"
test ! -e "${resource_guard_systemctl_state}/active.packagekit_service"
make_fake_streamplayer_process 320 3000 301 \
    "${streamplayer_test_executable}"
wait_for_guard_exit "${background_guard_pid}"
run_resource_guard_restore restore-background
test ! -e "${resource_guard_root}"
test ! -e "${resource_guard_systemctl_state}/masked.packagekit"
test -f "${resource_guard_systemctl_state}/active.packagekit_service"

# The display guard spans the foreground process lifetime, stops only LightDM,
# and restores its captured baseline after the tracked process disappears.
rm -rf -- \
    "${resource_guard_root}" \
    "${resource_guard_proc}" \
    "${resource_guard_systemctl_state}"
mkdir -p "${resource_guard_systemctl_state}"
: > "${resource_guard_systemctl_state}/active.lightdm_service"
make_fake_streamplayer_process 410 4000 400 \
    "${streamplayer_test_executable}"
run_resource_guard display \
    > "${test_root}/display-guard.log" 2>&1 &
display_guard_pid=$!
wait_for_guard_ready "${resource_guard_root}/display/ready"
test ! -e "${resource_guard_systemctl_state}/active.lightdm_service"
grep -Fqx 410 "${resource_guard_root}/display/tracked-pid"
rm -rf -- "${resource_guard_proc}/410"
wait_for_guard_exit "${display_guard_pid}"
run_resource_guard_restore restore-display
test ! -e "${resource_guard_root}"
test -f "${resource_guard_systemctl_state}/active.lightdm_service"
if grep -Fiq APPLaunch "${resource_guard_systemctl_state}/calls.log"; then
    echo "display guard must not stop or alter APPLaunch" >&2
    exit 1
fi

# A similarly named executable is not sufficient: matching remains exact.
rm -rf -- \
    "${resource_guard_root}" \
    "${resource_guard_proc}" \
    "${resource_guard_systemctl_state}"
mkdir -p "${resource_guard_systemctl_state}"
make_fake_streamplayer_process 510 5000 500 \
    "${streamplayer_test_executable}-other"
if run_resource_guard background \
        > "${test_root}/mismatched-guard.log" 2>&1; then
    echo "resource guard accepted a non-exact executable" >&2
    exit 1
fi
test ! -e "${resource_guard_root}"
test ! -e "${resource_guard_systemctl_state}/calls.log"

redacted_output="$(printf '%s\n' \
    'api_key=secret-one AccessToken%3Dsecret-two X-Emby-Token: secret-three {"AccessToken":"secret-four","api_key":"secret-five"}' |
    "${test_project_dir}/debug_cardputerzero_logs.sh" redact)"
case "${redacted_output}" in
    *secret-one*|*secret-two*|*secret-three*|*secret-four*|*secret-five*)
        echo "debug log redaction leaked a synthetic token" >&2
        exit 1
        ;;
esac

for package_script in \
    prepare-system \
    apply-runtime-tuning \
    resource-guard \
    validate-runtime \
    networkmanager-dispatcher \
    postinst \
    prerm \
    postrm; do
    sh -n "${test_project_dir}/packaging/${package_script}"
done

media_source="${test_project_dir}/main/src/media_controller.cpp"
package_version="$(awk -F': ' '$1 == "Version" { print $2; exit }' \
    "${test_project_dir}/packaging/control")"
grep -Fq "constexpr const char *kAppVersion = \"${package_version}\";" "${media_source}"

http_body_helper="$(
    sed -n '/^static bool write_private_http_body(/,/^}/p' "${media_source}"
)"
printf '%s\n' "${http_body_helper}" |
    grep -Fq 'char path_template[] = "/tmp/m5streamplayer-body-XXXXXX.json";'
printf '%s\n' "${http_body_helper}" |
    grep -Fq 'int fd = mkstemps(path_template, 5);'
printf '%s\n' "${http_body_helper}" |
    grep -Fq 'if (fchmod(fd, 0600) != 0)'
printf '%s\n' "${http_body_helper}" |
    grep -Fq 'descriptor_flags | FD_CLOEXEC'
printf '%s\n' "${http_body_helper}" |
    grep -Fq 'while (remaining > 0)'

scoped_temp_file="$(
    sed -n '/^class ScopedTemporaryFile {/,/^};/p' "${media_source}"
)"
printf '%s\n' "${scoped_temp_file}" |
    grep -Fq '~ScopedTemporaryFile()'
printf '%s\n' "${scoped_temp_file}" |
    grep -Fq 'if (unlink(path_.c_str()) != 0 && errno != ENOENT)'
grep -Fq 'ScopedTemporaryFile body_file;' "${media_source}"

http_body_staging_log="$(
    sed -n '/HTTP body staging failed method=/,/return response;/p' "${media_source}"
)"
printf '%s\n' "${http_body_staging_log}" |
    grep -Fq '" url=" + redact_query_secret(url) +'
if printf '%s\n' "${http_body_staging_log}" |
        grep -Fq '" url=" + url +'; then
    echo "HTTP body staging failure log contains an unredacted URL" >&2
    exit 1
fi

if grep -Fq '"/tmp/m5streamplayer-body-" + std::to_string(getpid())' \
        "${media_source}"; then
    echo "HTTP body staging regressed to a predictable PID-based path" >&2
    exit 1
fi

grep -Fq 'constexpr int kTranscodeMaxWidth = 1920;' "${media_source}"
grep -Fq 'constexpr int kTranscodeMaxHeight = 1080;' "${media_source}"
grep -Fq 'constexpr int kTranscodeMaxFramerate = 24;' "${media_source}"
grep -Fq 'constexpr int kTranscodeVideoBitrate = 3800000;' "${media_source}"
grep -Fq 'constexpr int kTranscodeAudioBitrate = 128000;' "${media_source}"
grep -Fq 'constexpr int kTranscodeMaxBitrate = kTranscodeVideoBitrate + kTranscodeAudioBitrate;' "${media_source}"
grep -Fq 'constexpr int kSmallScreenTranscodeMaxWidth = 320;' "${media_source}"
grep -Fq 'constexpr int kSmallScreenTranscodeMaxHeight = 170;' "${media_source}"
grep -Fq 'constexpr int kSmallScreenTranscodeVideoBitrate = 500000;' "${media_source}"
grep -Fq 'constexpr int kSmallScreenTranscodeMaxBitrate = kSmallScreenTranscodeVideoBitrate + kTranscodeAudioBitrate;' "${media_source}"
grep -Fq 'constexpr uint64_t kPlayerStartupTimeoutMs = 60000;' "${media_source}"

control_source="${test_project_dir}/packaging/control"
runtime_validator="${test_project_dir}/packaging/validate-runtime"
grep -Fqx 'Package: streamplayer' "${control_source}"
grep -Eq '^Depends: (.*, )?fonts-dejavu-core(, .*)?$' "${control_source}"
grep -Fq '/usr/share/APPLaunch/share/font/NotoSansSC-Regular.ttf' "${runtime_validator}"
grep -Fq '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf' "${runtime_validator}"
grep -Fqx 'CONFIG_V9_5_LV_FS_POSIX_PATH="/"' \
    "${test_project_dir}/config_defaults.mk"
grep -Fq 'g_runtime_font_14->fallback = g_runtime_fallback_font_14;' \
    "${media_source}"

for guard_unit in \
    "${test_project_dir}/packaging/streamplayer-background-guard.service" \
    "${test_project_dir}/packaging/streamplayer-display-guard.service"; do
    grep -Fqx 'OOMPolicy=stop' "${guard_unit}"
    grep -Fqx 'TimeoutStopSec=150s' "${guard_unit}"
    grep -Fq 'ExecStopPost=/usr/lib/streamplayer/resource-guard restore-' \
        "${guard_unit}"
done
if grep -Fqx 'After=lightdm.service' \
   "${test_project_dir}/packaging/streamplayer-display-guard.service"; then
    echo "display guard must not wait on LightDM while restoring it" >&2
    exit 1
fi
grep -Fq '"${systemctl_command}" --no-block start "${service_name}"' \
    "${test_project_dir}/packaging/resource-guard"
grep -Fq 'while tracked_process_is_running; do' \
    "${test_project_dir}/packaging/resource-guard"
grep -Fq '[ "${current_starttime}" = "${tracked_starttime}" ]' \
    "${test_project_dir}/packaging/resource-guard"
if grep -Fq 'for executable_link in /proc/[0-9]*/exe' \
        "${test_project_dir}/packaging/resource-guard"; then
    echo "resource guard regressed to repeatedly scanning all of /proc" >&2
    exit 1
fi

controller_init="$(
    sed -n '/^    void init()/,/^    void attach_keyboard/p' "${media_source}"
)"
printf '%s\n' "${controller_init}" |
    grep -Fq '(void)acquire_display_performance_mode();'
printf '%s\n' "${controller_init}" |
    grep -Fq '(void)acquire_background_performance_mode();'
printf '%s\n' "${controller_init}" |
    grep -Fq 'prepare_hdmi_output_mode();'
hdmi_mode_line="$(printf '%s\n' "${controller_init}" |
    grep -n 'prepare_hdmi_output_mode();' | cut -d: -f1)"
display_guard_line="$(printf '%s\n' "${controller_init}" |
    grep -n '(void)acquire_display_performance_mode();' | cut -d: -f1)"
test "${hdmi_mode_line}" -lt "${display_guard_line}"
controller_shutdown="$(
    sed -n '/^    void shutdown()/,/^private:/p' "${media_source}"
)"
printf '%s\n' "${controller_shutdown}" |
    grep -Fq 'restore_display_performance_mode();'
display_release="$(
    sed -n '/^    void release_kms_exclusive()/,/^    }/p' "${media_source}"
)"
printf '%s\n' "${display_release}" |
    grep -Fq 'display guard retained until app exit'
if printf '%s\n' "${display_release}" | grep -Fq 'systemctl'; then
    echo "player stop must not end the foreground display guard" >&2
    exit 1
fi
if grep -Fq 'stop streamplayer-display-guard.service' \
        "${test_project_dir}/packaging/streamplayer.sudoers" \
        "${runtime_validator}"; then
    echo "StreamPlayer no longer needs permission to stop its display guard" >&2
    exit 1
fi
grep -Fq 'M5_STREAMPLAYER_KEEP_LIGHTDM=1' \
    "${test_project_dir}/README.md"
grep -Fq "const char *keep_lightdm = getenv(\"M5_STREAMPLAYER_KEEP_LIGHTDM\");" \
    "${media_source}"
grep -Fq 'LightDM 将在退出应用后恢复' "${media_source}"
grep -Fq "StreamPlayer's entire foreground lifetime" \
    "${test_project_dir}/app-builder.json"
grep -Fq 'StreamPlayer 整个前台生命周期' \
    "${test_project_dir}/app-builder.json"
sh -n "${test_project_dir}/tests/fake_resource_guard_systemctl.sh"

echo "StreamPlayer packaging tests passed."
