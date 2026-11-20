#!/usr/bin/env bash
# Builds the ProtocolHollowEditor target on macOS (Apple Silicon, Xcode / Apple Clang) and fails
# if any compiler warning originates from project sources (CLAUDE.md sec. 7 DoD, ADR-0009).
#
# Usage:  Build/Scripts/build-editor-mac.sh [--config Development|DebugGame] [--no-unity] [--game] [--shipping]
#   --no-unity   disable unity batching to catch missing includes hidden by unity files
#   --game       also build the game target ProtocolHollow (no editor code: what a packaged build compiles, S4-05)
#   --shipping   also build the game target in Shipping (no dev, debugger or test code: what Steam ships, S4-06)
# Env:    UE_ROOT  engine root (default: /Users/Shared/Epic Games/UE_5.8)
set -euo pipefail
# shellcheck source=hollow-common.sh
source "$(dirname "${BASH_SOURCE[0]}")/hollow-common.sh"
PROJECT_ROOT="${HOLLOW_PROJECT_ROOT}"
UPROJECT="${HOLLOW_UPROJECT}"
CONFIG="Development"
EXTRA_ARGS=()
GAME=0
SHIPPING=0

while [[ $# -gt 0 ]]; do
	case "$1" in
		--config)   CONFIG="${2:?--config needs a value}"; shift 2 ;;
		--no-unity) EXTRA_ARGS+=("-DisableUnity"); shift ;;
		--game)     GAME=1; shift ;;
		--shipping) SHIPPING=1; shift ;;
		-h|--help)  sed -n '2,10p' "$0"; exit 0 ;;
		*)          echo "Unknown argument: $1" >&2; exit 2 ;;
	esac
done

BUILD_SH="${UE_ROOT}/Engine/Build/BatchFiles/Mac/Build.sh"
if [[ ! -x "${BUILD_SH}" ]]; then
	echo "Build.sh not found: ${BUILD_SH} (set UE_ROOT)" >&2
	exit 2
fi

BUILDS=("ProtocolHollowEditor ${CONFIG}")
if [[ "${GAME}" == 1 ]]; then BUILDS+=("ProtocolHollow ${CONFIG}"); fi
if [[ "${SHIPPING}" == 1 ]]; then BUILDS+=("ProtocolHollow Shipping"); fi

LOG_DIR="${PROJECT_ROOT}/Saved/Logs"
mkdir -p "${LOG_DIR}"
for BUILD in "${BUILDS[@]}"; do
	read -r TARGET TARGET_CONFIG <<< "${BUILD}"
	LOG_FILE="${LOG_DIR}/build-${TARGET}-mac-${TARGET_CONFIG}-$(date +%Y%m%d-%H%M%S).log"
	echo "Building ${TARGET} Mac ${TARGET_CONFIG} -> ${LOG_FILE}"
	"${BUILD_SH}" "${TARGET}" Mac "${TARGET_CONFIG}" -project="${UPROJECT}" -WaitMutex ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"} 2>&1 | tee "${LOG_FILE}"

	# Zero-warning gate: any compiler/UHT warning located in project modules fails the build.
	if grep -E 'warning[: ]' "${LOG_FILE}" | grep -F '/Source/Hollow' > "${LOG_FILE}.warnings"; then
		echo "FAILED: warnings in project sources:" >&2
		cat "${LOG_FILE}.warnings" >&2
		exit 1
	fi
	rm -f "${LOG_FILE}.warnings"
	echo "OK: ${TARGET} ${TARGET_CONFIG} built with zero project warnings."
done
