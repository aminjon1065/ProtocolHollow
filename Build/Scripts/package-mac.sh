#!/usr/bin/env bash
# Packages Protocol: Hollow for macOS (S4-06, ADR-0024): RunUAT BuildCookRun with -package, so the archive holds a
# self-contained ProtocolHollow.app (binary and cooked content), signed with Build/Mac/Resources/Hollow.entitlements
# (no App Sandbox). Fails if the build reports a warning in project sources (the same gate as package-win64.ps1), and
# if the packaged game does not survive its start: it runs once, off screen, for START_SECONDS. An incremental cook
# can ship stale engine materials or assets cooked with an older class layout, and the game dies at once (S4-08,
# KnownIssues WD-6, RL-5); --clean fixes it, so when the start check fails after an incremental cook the script cooks
# again with --clean, once, by itself.
#
# Usage:  Build/Scripts/package-mac.sh [--config Development|Shipping] [--clean] [--output DIR] [--no-start-check] [--no-retry]
#   --config          client configuration (default Development: console, logs and dev commands for the live tests)
#   --clean           full rebuild and recook instead of an incremental cook
#   --output          archive folder (default Packages/Mac/<Config>)
#   --no-start-check  skip the start of the packaged game
#   --no-retry        a failed start check after an incremental cook fails at once (no clean cook): to look at it
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
RETRY=1
START_SECONDS=30   # the main menu is up in about 10 s on an M4
while [[ $# -gt 0 ]]; do
	case "$1" in
		--config) CONFIG="${2:?--config needs a value}"; shift 2 ;;
		--clean)  CLEAN=1; shift ;;
		--output) OUTPUT="${2:?--output needs a value}"; shift 2 ;;
		--no-start-check) START_CHECK=0; shift ;;
		--no-retry) RETRY=0; shift ;;
		-h|--help) sed -n '2,15p' "$0"; exit 0 ;;
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
APP="${OUTPUT}/ProtocolHollow.app"
# UBT names a Development bundle after the target only; other configurations carry platform and configuration.
if [[ "${CONFIG}" != Development ]]; then APP="${OUTPUT}/ProtocolHollow-Mac-${CONFIG}.app"; fi

# Builds, cooks and packages (clean when CLEAN is 1) into OUTPUT; the zero-warning gate; the app has its content.
# Sets LOG_FILE. Exits the script on a failure.
package() {
	LOG_FILE="${LOG_DIR}/package-mac-${CONFIG}-$(date +%Y%m%d-%H%M%S).log"
	local args=(BuildCookRun "-project=${HOLLOW_UPROJECT}" -target=ProtocolHollow -platform=Mac "-clientconfig=${CONFIG}"
		-build -cook -stage -pak -iostore -package -archive "-archivedirectory=${OUTPUT}" -utf8output -nop4 -unattended)
	if [[ "${CLEAN}" == 1 ]]; then args+=(-clean); else args+=(-iterate); fi
	echo "Protocol: Hollow -> Mac ${CONFIG}$([[ "${CLEAN}" == 1 ]] && echo ' (clean)')"
	echo "  Output: ${OUTPUT}"
	echo "  Log:    ${LOG_FILE}"
	if ! "${RUNUAT}" "${args[@]}" > "${LOG_FILE}" 2>&1; then
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
	if [[ ! -d "${APP}/Contents/UE" ]]; then
		echo "FAILED: ${APP} has no cooked content (Contents/UE)" >&2
		exit 1
	fi
}

# Starts the packaged game off screen, no sound, the Null online subsystem and a save slot of its own (the player's
# profile stays untouched). 0 if it is alive after START_SECONDS; 1 (and why) if it died before.
start_check() {
	local slot="PackageStartCheck"
	local saves="${HOME}/Library/Application Support/Epic/ProtocolHollow/Saved/SaveGames"
	START_LOG="${LOG_FILE%.log}-start.log"
	"${APP}/Contents/MacOS/$(basename "${APP}" .app)" -RenderOffScreen -windowed -ResX=640 -ResY=360 -nosound -nosplash -unattended \
		"-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=Null" "-ini:Engine:[OnlineSubsystemSteam]:bEnabled=false" \
		"-ini:Game:[/Script/HollowEconomy.HollowEconomySettings]:SaveSlotName=${slot}" -abslog="${START_LOG}" > /dev/null 2>&1 &
	local game_pid=$!
	local i code
	for ((i = 0; i < START_SECONDS; i++)); do kill -0 "${game_pid}" 2> /dev/null || break; sleep 1; done
	if ! kill -0 "${game_pid}" 2> /dev/null; then
		wait "${game_pid}" && code=0 || code=$?
		echo "The packaged game exited after ${i} s (code ${code}); log ${START_LOG}." >&2
		grep -E "Fatal error|appError|Failed to find|InvalidPosition" "${START_LOG}" 2> /dev/null | head -5 >&2 || true
		rm -f "${saves}/${slot}.sav"
		return 1
	fi
	kill "${game_pid}" 2> /dev/null || true
	wait "${game_pid}" 2> /dev/null || true
	rm -f "${saves}/${slot}.sav"
	echo "  Start:  alive after ${START_SECONDS} s (${START_LOG})"
}

package
if [[ "${START_CHECK}" == 1 ]] && ! start_check; then
	if [[ "${CLEAN}" == 1 || "${RETRY}" == 0 ]]; then
		echo "FAILED: the packaged game does not start$([[ "${CLEAN}" == 0 ]] && echo ' (try --clean)')." >&2
		exit 1
	fi
	echo "The incremental cook's game died at its start (KnownIssues WD-6, RL-5): cooking again with --clean." >&2
	CLEAN=1
	package
	if ! start_check; then
		echo "FAILED: the packaged game does not start, even after a clean cook." >&2
		exit 1
	fi
fi
echo "OK: packaged to ${APP} ($(du -sh "${APP}" | cut -f1))"
