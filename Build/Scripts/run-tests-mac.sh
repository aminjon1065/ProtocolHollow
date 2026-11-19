#!/usr/bin/env bash
# Runs Protocol: Hollow automation tests headless on macOS and returns a non-zero exit code
# if anything is wrong (CLAUDE.md sec. 7). Pair script: run-tests-win64.ps1 (same flags and exit codes).
#
# Usage:  Build/Scripts/run-tests-mac.sh [--filter Hollow.] [--no-build] [--strict] [--timeout 1800]
#   --filter    test name filter passed to "Automation RunTests" (default: Hollow.)
#   --no-build  skip build-editor-mac.sh (default: incremental build + zero-warning gate first)
#   --strict    tests that pass with warnings count as failures
#   --timeout   seconds before the editor is killed (default: 1800)
# Env:    UE_ROOT  engine root (default: /Users/Shared/Epic Games/UE_5.8)
#
# Exit codes: 0 all passed | 1 test failed / not run | 2 usage or environment | 3 no report produced
#             4 no test matched the filter | 5 editor exited non-zero | 6 timeout | 7 build failed
set -euo pipefail
# shellcheck source=hollow-common.sh
source "$(dirname "${BASH_SOURCE[0]}")/hollow-common.sh"

FILTER="Hollow."
DO_BUILD=1
STRICT=0
TIMEOUT_SEC=1800

while [[ $# -gt 0 ]]; do
	case "$1" in
		--filter)   FILTER="${2:?--filter needs a value}"; shift 2 ;;
		--no-build) DO_BUILD=0; shift ;;
		--strict)   STRICT=1; shift ;;
		--timeout)  TIMEOUT_SEC="${2:?--timeout needs a value}"; shift 2 ;;
		-h|--help)  sed -n '2,15p' "$0"; exit 0 ;;
		*)          echo "Unknown argument: $1" >&2; exit 2 ;;
	esac
done

hollow_require_tools "${HOLLOW_EDITOR_CMD}" "${HOLLOW_PYTHON}"

if [[ ${DO_BUILD} -eq 1 ]] && ! hollow_build_editor; then
	echo "FAILED: editor build (or zero-warning gate) failed, tests not run." >&2
	exit 7
fi

RUN_DIR="${HOLLOW_PROJECT_ROOT}/Saved/Automation/$(date +%Y%m%d-%H%M%S)"
REPORT_DIR="${RUN_DIR}/Report"
EDITOR_LOG="${RUN_DIR}/Editor.log"
mkdir -p "${REPORT_DIR}"

echo "Running automation tests '${FILTER}' (timeout ${TIMEOUT_SEC}s) -> ${RUN_DIR}"
EDITOR_EXIT=0
hollow_run_with_timeout "${TIMEOUT_SEC}" "${RUN_DIR}/Console.txt" \
	"${HOLLOW_EDITOR_CMD}" "${HOLLOW_UPROJECT}" \
	-ExecCmds="Automation RunTests ${FILTER}; Quit" \
	-TestExit="Automation Test Queue Empty" \
	-unattended -nullrhi -nosplash -nosound -nop4 \
	-abslog="${EDITOR_LOG}" \
	-ReportExportPath="${REPORT_DIR}" || EDITOR_EXIT=$?
if [[ ${EDITOR_EXIT} -eq ${HOLLOW_TIMEOUT_EXIT} ]]; then
	echo "FAILED: timeout after ${TIMEOUT_SEC}s, editor killed. Log: ${EDITOR_LOG}" >&2
	exit 6
fi

# Summarize the report and map it to an exit code (UE-bundled Python: no external dependencies).
REPORT_EXIT=0
"${HOLLOW_PYTHON}" - "${REPORT_DIR}/index.json" "${STRICT}" "${EDITOR_LOG}" <<'PY' || REPORT_EXIT=$?
import json, os, sys

report_path, strict, editor_log = sys.argv[1], sys.argv[2] == "1", sys.argv[3]
if not os.path.isfile(report_path):
    # UE exports no report at all when the filter matches nothing; it logs this line instead.
    log_text = open(editor_log, encoding="utf-8", errors="replace").read() if os.path.isfile(editor_log) else ""
    if "No automation tests matched" in log_text:
        print("FAILED: no test matched the filter.")
        sys.exit(4)
    print("FAILED: no report produced (editor crashed or failed to start).")
    sys.exit(3)

with open(report_path, encoding="utf-8-sig") as f:
    report = json.load(f)

tests = report.get("tests", [])
passed = [t for t in tests if t.get("state") == "Success" and not t.get("warnings")]
warned = [t for t in tests if t.get("state") == "Success" and t.get("warnings")]
failed = [t for t in tests if t.get("state") == "Fail"]
not_run = [t for t in tests if t.get("state") not in ("Success", "Fail")]

print("")
print("Automation report: %d test(s) | passed %d | passed with warnings %d | failed %d | not run %d"
      % (len(tests), len(passed), len(warned), len(failed), len(not_run)))

def show(title, items, kinds):
    for t in items:
        print("  %s %s" % (title, t.get("fullTestPath", "?")))
        for entry in t.get("entries", []):
            event = entry.get("event", {})
            if event.get("type") in kinds:
                where = entry.get("filename", "")
                line = entry.get("lineNumber", 0)
                location = " (%s:%s)" % (os.path.basename(where), line) if where and where != "Unknown" else ""
                print("      %s: %s%s" % (event.get("type"), event.get("message", ""), location))

show("FAIL   ", failed, ("Error",))
show("WARN   ", warned, ("Warning",))
show("NOT RUN", not_run, ("Error", "Warning"))

if not tests:
    print("FAILED: no test matched the filter.")
    sys.exit(4)
if failed or not_run or (strict and warned):
    sys.exit(1)
PY

echo "Report: ${REPORT_DIR}/index.html | Editor log: ${EDITOR_LOG}"
if [[ ${REPORT_EXIT} -ne 0 ]]; then
	exit "${REPORT_EXIT}"
fi
if [[ ${EDITOR_EXIT} -ne 0 ]]; then
	echo "FAILED: editor exited with code ${EDITOR_EXIT} although all tests passed (see ${EDITOR_LOG})." >&2
	exit 5
fi
echo "OK: all tests passed."
