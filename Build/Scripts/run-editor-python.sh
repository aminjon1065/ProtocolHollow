#!/usr/bin/env bash
# Runs an editor-only Python content script headless (ADR-0010) and fails unless the script reported success.
# The script must log the marker "HOLLOW_PYTHON_OK" when done and call unreal.SystemLibrary.quit_editor().
#
# Usage:  Build/Scripts/run-editor-python.sh <script.py> [--no-build] [--timeout 1800]
#   --no-build  skip build-editor-mac.sh (default: incremental build + zero-warning gate first)
#   --timeout   seconds before the editor is killed (default: 1800)
# Env:    UE_ROOT  engine root (default: /Users/Shared/Epic Games/UE_5.8)
#
# Exit codes: 0 success | 1 script failed (no success marker) | 2 usage or environment | 6 timeout | 7 build failed
set -euo pipefail
# shellcheck source=hollow-common.sh
source "$(dirname "${BASH_SOURCE[0]}")/hollow-common.sh"

SUCCESS_MARKER="HOLLOW_PYTHON_OK"
SCRIPT=""
DO_BUILD=1
TIMEOUT_SEC=1800

while [[ $# -gt 0 ]]; do
	case "$1" in
		--no-build) DO_BUILD=0; shift ;;
		--timeout)  TIMEOUT_SEC="${2:?--timeout needs a value}"; shift 2 ;;
		-h|--help)  sed -n '2,10p' "$0"; exit 0 ;;
		-*)         echo "Unknown argument: $1" >&2; exit 2 ;;
		*)          SCRIPT="$1"; shift ;;
	esac
done

if [[ -z "${SCRIPT}" || ! -f "${SCRIPT}" ]]; then
	echo "Python script not found: '${SCRIPT}'" >&2
	exit 2
fi
SCRIPT="$(cd "$(dirname "${SCRIPT}")" && pwd)/$(basename "${SCRIPT}")"
hollow_require_tools "${HOLLOW_EDITOR_CMD}"

if [[ ${DO_BUILD} -eq 1 ]] && ! hollow_build_editor; then
	echo "FAILED: editor build (or zero-warning gate) failed, script not run." >&2
	exit 7
fi

RUN_DIR="${HOLLOW_PROJECT_ROOT}/Saved/EditorPython/$(date +%Y%m%d-%H%M%S)-$(basename "${SCRIPT}" .py)"
EDITOR_LOG="${RUN_DIR}/Editor.log"
mkdir -p "${RUN_DIR}"

echo "Running ${SCRIPT} (timeout ${TIMEOUT_SEC}s) -> ${RUN_DIR}"
EDITOR_EXIT=0
hollow_run_with_timeout "${TIMEOUT_SEC}" "${RUN_DIR}/Console.txt" \
	"${HOLLOW_EDITOR_CMD}" "${HOLLOW_UPROJECT}" \
	-ExecutePythonScript="${SCRIPT}" \
	-unattended -nullrhi -nosplash -nosound -nop4 \
	-abslog="${EDITOR_LOG}" || EDITOR_EXIT=$?
if [[ ${EDITOR_EXIT} -eq ${HOLLOW_TIMEOUT_EXIT} ]]; then
	echo "FAILED: timeout after ${TIMEOUT_SEC}s (did the script call quit_editor?). Log: ${EDITOR_LOG}" >&2
	exit 6
fi

grep -E 'LogPython: (Error|Warning)' "${EDITOR_LOG}" || true
if ! grep -qF "${SUCCESS_MARKER}" "${EDITOR_LOG}"; then
	echo "FAILED: script did not report ${SUCCESS_MARKER} (editor exit ${EDITOR_EXIT}). Log: ${EDITOR_LOG}" >&2
	exit 1
fi
echo "OK: $(basename "${SCRIPT}") finished."
