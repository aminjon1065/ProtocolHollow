#!/usr/bin/env bash
# Builds the ProtocolHollowEditor target on macOS (Apple Silicon, Xcode / Apple Clang) and fails
# if any compiler warning originates from project sources (CLAUDE.md sec. 7 DoD, ADR-0009).
#
# Usage:  Build/Scripts/build-editor-mac.sh [--config Development|DebugGame] [--no-unity]
#   --no-unity   disable unity batching to catch missing includes hidden by unity files
# Env:    UE_ROOT  engine root (default: /Users/Shared/Epic Games/UE_5.8)
set -euo pipefail

UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
UPROJECT="${PROJECT_ROOT}/ProtocolHollow.uproject"
CONFIG="Development"
EXTRA_ARGS=()

while [[ $# -gt 0 ]]; do
	case "$1" in
		--config)   CONFIG="${2:?--config needs a value}"; shift 2 ;;
		--no-unity) EXTRA_ARGS+=("-DisableUnity"); shift ;;
		-h|--help)  sed -n '2,8p' "$0"; exit 0 ;;
		*)          echo "Unknown argument: $1" >&2; exit 2 ;;
	esac
done

BUILD_SH="${UE_ROOT}/Engine/Build/BatchFiles/Mac/Build.sh"
if [[ ! -x "${BUILD_SH}" ]]; then
	echo "Build.sh not found: ${BUILD_SH} (set UE_ROOT)" >&2
	exit 2
fi

LOG_DIR="${PROJECT_ROOT}/Saved/Logs"
mkdir -p "${LOG_DIR}"
LOG_FILE="${LOG_DIR}/build-editor-mac-$(date +%Y%m%d-%H%M%S).log"

echo "Building ProtocolHollowEditor Mac ${CONFIG} -> ${LOG_FILE}"
"${BUILD_SH}" ProtocolHollowEditor Mac "${CONFIG}" -project="${UPROJECT}" -WaitMutex ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"} 2>&1 | tee "${LOG_FILE}"

# Zero-warning gate: any compiler/UHT warning located in project modules fails the build.
if grep -E 'warning[: ]' "${LOG_FILE}" | grep -F '/Source/Hollow' > "${LOG_FILE}.warnings"; then
	echo "FAILED: warnings in project sources:" >&2
	cat "${LOG_FILE}.warnings" >&2
	exit 1
fi
rm -f "${LOG_FILE}.warnings"
echo "OK: ProtocolHollowEditor built with zero project warnings."
