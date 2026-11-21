#!/usr/bin/env bash
# Packages Protocol: Hollow for macOS (S4-06, ADR-0024): RunUAT BuildCookRun with -package, so the archive holds a
# self-contained ProtocolHollow.app (binary and cooked content), signed with Build/Mac/Resources/Hollow.entitlements
# (no App Sandbox). Fails if the build reports a warning in project sources (the same gate as package-win64.ps1), and
# if the packaged game does not survive its start: it runs once, off screen, for START_SECONDS (S4-08: an incremental
# cook once shipped an engine material without its shaders and the game died at once; --clean fixed it).
#
# Usage:  Build/Scripts/package-mac.sh [--config Development|Shipping] [--clean] [--output DIR] [--no-start-check]
#   --config          client configuration (default Development: console, logs and dev commands for the live tests)
#   --clean           full rebuild and recook instead of an incremental cook
#   --output          archive folder (default Packages/Mac/<Config>)
#   --no-start-check  skip the start of the packaged game
# Output: <output>/ProtocolHollow.app (Shipping: ProtocolHollow-Mac-Shipping.app)
# Log:    Saved/Logs/package-mac-<Config>-<timestamp>.log
# Env:    UE_ROOT  engine root (default: /Users/Shared/Epic Games/UE_5.8)
set -euo pipefail
# shellcheck source=hollow-common.sh
source "$(dirname "${BASH_SOURCE[0]}")/hollow-common.sh"

CONFIG="Development"
CLEAN=0
OUTPUT=""
START_CHECK=1
START_SECONDS=30   # the main menu is up in about 10 s on an M4
while [[ $# -gt 0 ]]; do
	case "$1" in
		--config) CONFIG="${2:?--config needs a value}"; shift 2 ;;
		--clean)  CLEAN=1; shift ;;
		--output) OUTPUT="${2:?--output needs a value}"; shift 2 ;;
		--no-start-check) START_CHECK=0; shift ;;
		-h|--help) sed -n '2,12p' "$0"; exit 0 ;;
		*) echo "Unknown argument: $1" >&2; exit 2 ;;
	esac
done
case "${CONFIG}" in
	Development|Shipping) ;;
	*) echo "--config must be Development or Shipping (the launcher engine builds no Test targets)" >&2; exit 2 ;;
esac

RUNUAT="${UE_ROOT}/Engine/Build/BatchFiles/RunUAT.sh"
if [[ ! -x "${RUNUAT}" ]]; then
	echo "RunUAT.sh not found: ${RUNUAT} (set UE_ROOT)" >&2
	exit 2
fi
# The cook needs the placeholder characters, which are not committed (ADR-0011).
if ! "${HOLLOW_PROJECT_ROOT}/Build/Scripts/import-placeholders.sh" --check > /dev/null 2>&1; then
	echo "Placeholders missing: importing them from the engine templates..."
	"${HOLLOW_PROJECT_ROOT}/Build/Scripts/import-placeholders.sh"
fi

OUTPUT="${OUTPUT:-${HOLLOW_PROJECT_ROOT}/Packages/Mac/${CONFIG}}"
LOG_DIR="${HOLLOW_PROJECT_ROOT}/Saved/Logs"
mkdir -p "${LOG_DIR}" "${OUTPUT}"
LOG_FILE="${LOG_DIR}/package-mac-${CONFIG}-$(date +%Y%m%d-%H%M%S).log"
ARGS=(BuildCookRun "-project=${HOLLOW_UPROJECT}" -target=ProtocolHollow -platform=Mac "-clientconfig=${CONFIG}"
	-build -cook -stage -pak -iostore -package -archive "-archivedirectory=${OUTPUT}" -utf8output -nop4 -unattended)
if [[ "${CLEAN}" == 1 ]]; then ARGS+=(-clean); else ARGS+=(-iterate); fi

echo "Protocol: Hollow -> Mac ${CONFIG}"
echo "  Output: ${OUTPUT}"
echo "  Log:    ${LOG_FILE}"
if ! "${RUNUAT}" "${ARGS[@]}" > "${LOG_FILE}" 2>&1; then
	echo "FAILED: BuildCookRun (see ${LOG_FILE})" >&2
	grep -E "error|Error:" "${LOG_FILE}" | head -10 >&2 || true
	exit 1
fi
if grep -E 'warning[: ]' "${LOG_FILE}" | grep -F '/Source/Hollow' > "${LOG_FILE}.warnings"; then
	echo "FAILED: warnings in project sources:" >&2
	cat "${LOG_FILE}.warnings" >&2
	exit 1
fi
rm -f "${LOG_FILE}.warnings"
# UBT names a Development bundle after the target only; other configurations carry platform and configuration.
APP="${OUTPUT}/ProtocolHollow.app"
if [[ "${CONFIG}" != Development ]]; then APP="${OUTPUT}/ProtocolHollow-Mac-${CONFIG}.app"; fi
if [[ ! -d "${APP}/Contents/UE" ]]; then
	echo "FAILED: ${APP} has no cooked content (Contents/UE)" >&2
	exit 1
fi
if [[ "${START_CHECK}" == 1 ]]; then
	# Off screen, no sound, the Null online subsystem and a save slot of its own (the player's profile stays untouched).
	SLOT="PackageStartCheck"
	SAVES="${HOME}/Library/Application Support/Epic/ProtocolHollow/Saved/SaveGames"
	START_LOG="${LOG_FILE%.log}-start.log"
	"${APP}/Contents/MacOS/$(basename "${APP}" .app)" -RenderOffScreen -windowed -ResX=640 -ResY=360 -nosound -nosplash -unattended \
		"-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=Null" "-ini:Engine:[OnlineSubsystemSteam]:bEnabled=false" \
		"-ini:Game:[/Script/HollowEconomy.HollowEconomySettings]:SaveSlotName=${SLOT}" -abslog="${START_LOG}" > /dev/null 2>&1 &
	GAME_PID=$!
	for ((i = 0; i < START_SECONDS; i++)); do kill -0 "${GAME_PID}" 2> /dev/null || break; sleep 1; done
	if ! kill -0 "${GAME_PID}" 2> /dev/null; then
		wait "${GAME_PID}" && CODE=0 || CODE=$?
		echo "FAILED: the packaged game exited after ${i} s (code ${CODE}); log ${START_LOG}. Try --clean." >&2
		grep -E "Fatal error|appError|Failed to find" "${START_LOG}" 2> /dev/null | head -5 >&2 || true
		exit 1
	fi
	kill "${GAME_PID}" 2> /dev/null || true
	wait "${GAME_PID}" 2> /dev/null || true
	rm -f "${SAVES}/${SLOT}.sav"
	echo "  Start:  alive after ${START_SECONDS} s (${START_LOG})"
fi
echo "OK: packaged to ${APP} ($(du -sh "${APP}" | cut -f1))"
