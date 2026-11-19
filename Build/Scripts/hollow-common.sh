#!/usr/bin/env bash
# Shared helpers for the macOS scripts in Build/Scripts (sourced, not executed). ASCII-only (CLAUDE.md sec. 2).

UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
HOLLOW_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HOLLOW_UPROJECT="${HOLLOW_PROJECT_ROOT}/ProtocolHollow.uproject"
HOLLOW_EDITOR_CMD="${UE_ROOT}/Engine/Binaries/Mac/UnrealEditor-Cmd"
HOLLOW_PYTHON="${UE_ROOT}/Engine/Binaries/ThirdParty/Python3/Mac/bin/python3"
HOLLOW_TIMEOUT_EXIT=124
HOLLOW_POLL_SEC=2
HOLLOW_KILL_GRACE_SEC=10

# Fails with exit code 2 unless every given tool is executable.
hollow_require_tools() {
	local tool
	for tool in "$@"; do
		if [[ ! -x "${tool}" ]]; then
			echo "Not found: ${tool} (set UE_ROOT)" >&2
			exit 2
		fi
	done
}

# Incremental editor build with the zero-warning gate. Returns the build script's exit code.
hollow_build_editor() {
	UE_ROOT="${UE_ROOT}" "${HOLLOW_PROJECT_ROOT}/Build/Scripts/build-editor-mac.sh"
}

# hollow_run_with_timeout <timeout_sec> <output_file> <command...>
# Runs the command in the background with stdout/stderr in <output_file>, kills it after the timeout.
# Returns the command's exit code, or HOLLOW_TIMEOUT_EXIT when it was killed.
hollow_run_with_timeout() {
	local timeout_sec="$1" output_file="$2"
	shift 2
	"$@" > "${output_file}" 2>&1 &
	local pid=$! waited=0 exit_code=0
	while kill -0 "${pid}" 2>/dev/null; do
		if [[ ${waited} -ge ${timeout_sec} ]]; then
			echo "Timeout after ${timeout_sec}s, killing pid ${pid}." >&2
			kill -TERM "${pid}" 2>/dev/null || true
			sleep "${HOLLOW_KILL_GRACE_SEC}"
			kill -KILL "${pid}" 2>/dev/null || true
			wait "${pid}" 2>/dev/null || true
			return "${HOLLOW_TIMEOUT_EXIT}"
		fi
		sleep "${HOLLOW_POLL_SEC}"
		waited=$((waited + HOLLOW_POLL_SEC))
	done
	wait "${pid}" || exit_code=$?
	return "${exit_code}"
}
