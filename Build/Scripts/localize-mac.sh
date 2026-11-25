#!/usr/bin/env bash
# LOC-01 (ADR-0048): runs the Game localization target headless (Config/Localization/Game.ini, GatherText commandlet):
# imports Content/Localization/Game/ru/Game.po, gathers the player texts of the code and the data again, compiles
# Game.locres (en, ru) and exports the PO files with any new entry empty. Run it after a change to a player text, then
# translate the new entries in ru/Game.po and run it again. A PO whose entries did not change keeps its file (the
# export stamps the time in its header), so a run with nothing new leaves the repository as it was.
#
# Usage:  Build/Scripts/localize-mac.sh [--no-build] [--check] [--timeout 1800]
#   --no-build  skip build-editor-mac.sh (default: incremental build + zero-warning gate first)
#   --check     fail if an entry of ru/Game.po is still untranslated (after the run)
#   --timeout   seconds before the editor is killed (default: 1800)
# Env:    UE_ROOT  engine root (default: /Users/Shared/Epic Games/UE_5.8)
#
# Exit codes: 0 success | 1 commandlet failed | 2 usage or environment | 3 untranslated entries (--check) | 6 timeout | 7 build failed
set -euo pipefail
# shellcheck source=hollow-common.sh
source "$(dirname "${BASH_SOURCE[0]}")/hollow-common.sh"

DO_BUILD=1
DO_CHECK=0
TIMEOUT_SEC=1800
CONFIG="Config/Localization/Game.ini"
TARGET_DIR="${HOLLOW_PROJECT_ROOT}/Content/Localization/Game"

while [[ $# -gt 0 ]]; do
	case "$1" in
		--no-build) DO_BUILD=0; shift ;;
		--check)    DO_CHECK=1; shift ;;
		--timeout)  TIMEOUT_SEC="${2:?--timeout needs a value}"; shift 2 ;;
		-h|--help)  sed -n '2,15p' "$0"; exit 0 ;;
		*)          echo "Unknown argument: $1" >&2; exit 2 ;;
	esac
done

hollow_require_tools "${HOLLOW_EDITOR_CMD}"
if [[ ${DO_BUILD} -eq 1 ]] && ! hollow_build_editor; then
	echo "FAILED: editor build (or zero-warning gate) failed, localization not run." >&2
	exit 7
fi

RUN_DIR="${HOLLOW_PROJECT_ROOT}/Saved/Localization/$(date +%Y%m%d-%H%M%S)"
EDITOR_LOG="${RUN_DIR}/Editor.log"
mkdir -p "${RUN_DIR}/Before"

# The PO files as they were, to give back the ones whose entries did not change.
PO_FILES=()
if [[ -d "${TARGET_DIR}" ]]; then
	while IFS= read -r po; do
		PO_FILES+=("${po}")
		mkdir -p "${RUN_DIR}/Before/$(dirname "${po#"${TARGET_DIR}"/}")"
		cp "${po}" "${RUN_DIR}/Before/${po#"${TARGET_DIR}"/}"
	done < <(find "${TARGET_DIR}" -name '*.po' | sort)
fi

echo "Localizing (${CONFIG}, timeout ${TIMEOUT_SEC}s) -> ${RUN_DIR}"
EDITOR_EXIT=0
hollow_run_editor_with_timeout "${TIMEOUT_SEC}" "${RUN_DIR}/Console.txt" "${EDITOR_LOG}" \
	"${HOLLOW_EDITOR_CMD}" "${HOLLOW_UPROJECT}" \
	-run=GatherText -config="${CONFIG}" \
	-unattended -nullrhi -nosplash -nosound -nop4 \
	-abslog="${EDITOR_LOG}" || EDITOR_EXIT=$?
if [[ ${EDITOR_EXIT} -eq ${HOLLOW_TIMEOUT_EXIT} ]]; then
	echo "FAILED: timeout after ${TIMEOUT_SEC}s. Log: ${EDITOR_LOG}" >&2
	exit 6
fi
grep -E 'Log(GatherText|LocTextHelper|Localization|PortableObject|TextLocalizationResource)[A-Za-z]*: (Error|Warning)' "${EDITOR_LOG}" || true
if [[ ${EDITOR_EXIT} -ne 0 ]] || grep -qE 'Log(GatherText|LocTextHelper)[A-Za-z]*: Error' "${EDITOR_LOG}"; then
	echo "FAILED: GatherText (editor exit ${EDITOR_EXIT}). Log: ${EDITOR_LOG}" >&2
	exit 1
fi

# The export stamps the time in each PO header: a PO whose entries are the same gets its old file back.
stamped='^"(POT-Creation-Date|PO-Revision-Date): '
for po in "${PO_FILES[@]+"${PO_FILES[@]}"}"; do
	before="${RUN_DIR}/Before/${po#"${TARGET_DIR}"/}"
	if [[ -f "${po}" ]] && cmp -s <(grep -vE "${stamped}" "${before}") <(grep -vE "${stamped}" "${po}"); then
		cp "${before}" "${po}"
	fi
done

RU_PO="${TARGET_DIR}/ru/Game.po"
if [[ ! -f "${RU_PO}" ]]; then
	echo "FAILED: no ${RU_PO} after the run. Log: ${EDITOR_LOG}" >&2
	exit 1
fi
# Entries and the untranslated ones (msgstr "" with no continuation line), the header (msgid "") aside.
# (bytes, not characters: BSD awk compares lines with Cyrillic wrongly in a UTF-8 locale)
read -r ENTRIES UNTRANSLATED < <(LC_ALL=C awk '
	/^msgid / { header = (!seen && $0 == "msgid \"\""); seen = 1 }   # only the first entry is the header
	/^msgstr / { if (!header) { entries++; empty = ($0 == "msgstr \"\""); pending = 1 } ; next }
	pending && /^"/ { empty = 0; next }
	pending { if (empty) untranslated++; pending = 0 }
	END { if (pending && empty) untranslated++; printf "%d %d\n", entries, untranslated }' "${RU_PO}")
echo "ru: ${ENTRIES} entries, ${UNTRANSLATED} untranslated (${RU_PO#"${HOLLOW_PROJECT_ROOT}"/})"
if [[ ${DO_CHECK} -eq 1 && ${UNTRANSLATED} -gt 0 ]]; then
	echo "FAILED: ${UNTRANSLATED} entries of ru/Game.po have no translation." >&2
	exit 3
fi
echo "OK: localization compiled."
