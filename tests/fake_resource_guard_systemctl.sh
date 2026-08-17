#!/bin/sh
set -eu

state_dir=${FAKE_SYSTEMCTL_STATE_DIR:?}
log_file="${state_dir}/calls.log"
mkdir -p "${state_dir}"
printf '%s\n' "$*" >> "${log_file}"

service_key()
{
    printf '%s' "$1" | tr '.-' '__'
}

case "${1:-}" in
    is-system-running)
        printf '%s\n' running
        ;;
    show)
        [ "${2:-}" = packagekit.service ]
        [ "${3:-}" = --property=LoadState ]
        [ "${4:-}" = --value ]
        if [ -f "${state_dir}/masked.packagekit" ]; then
            printf '%s\n' masked
        else
            printf '%s\n' loaded
        fi
        ;;
    mask)
        [ "${2:-}" = --runtime ]
        [ "${3:-}" = packagekit.service ]
        : > "${state_dir}/masked.packagekit"
        ;;
    unmask)
        [ "${2:-}" = --runtime ]
        [ "${3:-}" = packagekit.service ]
        rm -f -- "${state_dir}/masked.packagekit"
        ;;
    stop)
        service_name=${2:-}
        [ -n "${service_name}" ]
        rm -f -- "${state_dir}/active.$(service_key "${service_name}")"
        ;;
    --no-block)
        [ "${2:-}" = start ]
        service_name=${3:-}
        [ -n "${service_name}" ]
        if [ "${FAKE_SYSTEMCTL_FAIL_START:-}" = "${service_name}" ]; then
            : > "${state_dir}/failed.$(service_key "${service_name}")"
            exit 1
        fi
        service_state="${state_dir}/active.$(service_key "${service_name}")"
        failed_state="${state_dir}/failed.$(service_key "${service_name}")"
        rm -f -- "${failed_state}"
        (
            sleep "${FAKE_SYSTEMCTL_START_DELAY:-0}"
            : > "${service_state}"
        ) </dev/null >/dev/null 2>&1 &
        ;;
    is-active)
        [ "${2:-}" = --quiet ]
        service_name=${3:-}
        test -f "${state_dir}/active.$(service_key "${service_name}")"
        ;;
    is-failed)
        [ "${2:-}" = --quiet ]
        service_name=${3:-}
        test -f "${state_dir}/failed.$(service_key "${service_name}")"
        ;;
    *)
        printf '%s\n' "unexpected fake systemctl call: $*" >&2
        exit 2
        ;;
esac
