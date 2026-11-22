#!/usr/bin/env bash
# The E9 benchmark scenario on this Mac (ADR-0023; indicative only: E9 itself is measured on Windows, perf-e9-win64.ps1).
# A rendered host (offscreen, 1920x1080, sound on) and 3 headless clients on this machine, Null OSS, seed 7: AI removed,
# everyone at the core in the CoreVault, the host lifts it and the squad Resonance goes Critical (the emergency strobe),
# two clients weld; the host's view turns evenly while Hollow.Perf.Capture records. Prints the verdict line; the CSV and
# JSON land in Saved/Profiling/Hollow, the log (and the Insights trace with --trace) in the output folder.
#
# Usage: Build/Scripts/perf-capture-mac.sh [--preset High|Low] [--seconds 60] [--warmup 10] [--trace] [--epic] [--profilegpu]
#                                          [--exec "cvar value;..."] [--no-build]
#   --epic        the engine's Epic scalability over the preset (the S4-03 baseline)
#   --profilegpu  one GPU profile (per pass) halfway through the capture, printed after the verdict (it costs one hitch)
#   --exec        console commands for the host before the capture (trying a setting), separated by ';'
#   --audio-stress N  every bank sound N times at the host from 1 s into the capture to its end (first plays included)
#   --app PATH    run a packaged build (ProtocolHollow.app, BuildCookRun -platform=Mac) instead of the editor binary
#   --sector NAME raid sector plugin HollowSector_NAME instead of the configured one (S7-03: Cathedral; the host's -ini)
set -euo pipefail
source "$(dirname "$0")/hollow-common.sh"

PRESET=High
SECONDS_TO_CAPTURE=60
WARMUP=10
TRACE=0
EPIC=0
PROFILE=0
EXTRA=""
STRESS=0
APP=""
BUILD=1
SECTOR=
while [[ $# -gt 0 ]]; do
	case "$1" in
		--preset) PRESET="$2"; shift 2 ;;
		--seconds) SECONDS_TO_CAPTURE="$2"; shift 2 ;;
		--warmup) WARMUP="$2"; shift 2 ;;
		--trace) TRACE=1; shift ;;
		--epic) EPIC=1; shift ;;
		--profilegpu) PROFILE=1; shift ;;
		--exec) EXTRA="$2"; shift 2 ;;
		--audio-stress) STRESS="$2"; shift 2 ;;
		--app) APP="$2"; BUILD=0; shift 2 ;;
		--no-build) BUILD=0; shift ;;
		--sector) SECTOR="$2"; shift 2 ;;
		*) echo "Unknown argument: $1" >&2; exit 2 ;;
	esac
done
if [[ "$BUILD" == 1 ]]; then
	"$(dirname "$0")/build-editor-mac.sh"
fi

GAME=("$HOLLOW_EDITOR_CMD" "$HOLLOW_UPROJECT" -game)
LOGARG=abslog
if [[ -n "$APP" ]]; then
	GAME=("$APP/Contents/MacOS/ProtocolHollow")
	LOGARG=stdout   # a packaged Mac build writes no log file (neither -abslog nor its default): its output is the log
fi
# Starts one game process with its log at $1; the rest are its arguments.
launch() {
	local log="$1"; shift
	if [[ "$LOGARG" == stdout ]]; then
		"${GAME[@]}" "$@" -stdout -FullStdOutLogOutput > "$log" 2>&1 &
	else
		"${GAME[@]}" "$@" -abslog="$log" > /dev/null 2>&1 &
	fi
	PIDS+=($!)
}
STAMP="$(date +%Y%m%d-%H%M%S)"
OUT="$HOLLOW_PROJECT_ROOT/Saved/PerfCapture/$STAMP-$PRESET"
mkdir -p "$OUT"
SAVES="$HOLLOW_PROJECT_ROOT/Saved/SaveGames"
COMMON=(-nosplash -unattended -nop4 "-HollowPreset=$PRESET"
	"-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=Null" "-ini:Engine:[OnlineSubsystemSteam]:bEnabled=false")
HEADLESS=(-nullrhi -nosound)
HEADLESS_FPS="t.MaxFPS 30"   # the clients only have to play: keep their CPU away from the host
slot() { echo "-ini:Game:[/Script/HollowEconomy.HollowEconomySettings]:SaveSlotName=$1"; }
wait_for() { local i; for ((i = 0; i < $3; i++)); do grep -qE "$2" "$1" 2>/dev/null && return 0; sleep 1; done; return 1; }

TOTAL=$(awk -v w="$WARMUP" -v s="$SECONDS_TO_CAPTURE" 'BEGIN { print w + s + 5 }')
H="r.VSync 0,t.MaxFPS 0,Hollow.Session.Host InviteOnly,Hollow.Dev.RunAfter 45 Hollow.Raid.Start 7"
r() { H="$H,Hollow.Dev.RunInRaid $1 $2"; }
r 2 "Hollow.AI.Kill"
r 3 "Hollow.AI.Director Spawn"   # brought now and removed: the director sends no other one
r 4 "Hollow.AI.Kill"
r 5 "Hollow.Dev.GoToNearestCore All"
r 6 "Hollow.Interact"
r 7 "Hollow.Resonance.Add 90"
[[ "$EPIC" == 1 ]] && r 7.5 "scalability 3"
if [[ -n "$EXTRA" ]]; then
	IFS=';' read -ra EXTRAS <<< "$EXTRA"
	for e in "${EXTRAS[@]}"; do r 7.7 "$e"; done
fi
[[ "$STRESS" != 0 ]] && r "$(awk -v w="$WARMUP" 'BEGIN { print 9 + w + 1 }')" "Hollow.Audio.Stress $STRESS $(awk -v s="$SECONDS_TO_CAPTURE" 'BEGIN { print s - 2 }')"
[[ "$PROFILE" == 1 ]] && r "$(awk -v w="$WARMUP" -v s="$SECONDS_TO_CAPTURE" 'BEGIN { print 9 + w + s / 2 }')" "ProfileGPU"
r 8 "Hollow.Graphics.Dump"
r 9 "Hollow.Dev.LookAround $TOTAL 2"
r 9 "Hollow.Perf.Capture E9-$PRESET${SECTOR:+-$SECTOR} $SECONDS_TO_CAPTURE $WARMUP"
W="Hollow.Dev.RunInRaid 8 Hollow.Tool.Hold Hollow.Input.SecondaryUse $TOTAL"

SECTORARGS=()
[[ -n "$SECTOR" ]] && SECTORARGS=("-ini:Game:[/Script/HollowGame.HollowSectorSettings]:RaidSector=HollowSector_$SECTOR")   # the clients learn it from the manifest
TRACEARGS=()
[[ "$TRACE" == 1 ]] && TRACEARGS=(-trace=default,gpu,frame,net,physics,audio "-tracefile=$OUT/host.utrace" -statnamedevents)
for s in PerfHost PerfClient1 PerfClient2 PerfClient3; do rm -f "$SAVES/$s.sav"; done
PIDS=()
trap 'kill ${PIDS[@]+"${PIDS[@]}"} 2>/dev/null || true' EXIT   # never leave a game running: it would load the next capture
launch "$OUT/host.log" "${COMMON[@]}" -RenderOffScreen -windowed -ResX=1920 -ResY=1080 "$(slot PerfHost)" ${SECTORARGS[@]+"${SECTORARGS[@]}"} ${TRACEARGS[@]+"${TRACEARGS[@]}"} -ExecCmds="$H"
wait_for "$OUT/host.log" 'listening on port' 240 || { echo "host never listened"; kill "${PIDS[@]}"; exit 1; }
for i in 1 2 3; do
	CMDS="$HEADLESS_FPS,open 127.0.0.1"
	[[ "$i" != 3 ]] && CMDS="$CMDS,$W"   # two of them weld
	launch "$OUT/client$i.log" "${COMMON[@]}" "${HEADLESS[@]}" "$(slot PerfClient$i)" -ExecCmds="$CMDS"
done
wait_for "$OUT/host.log" 'Hollow.Perf: report ' $((240 + ${TOTAL%.*} + 120)) || echo "no capture report"
sleep 2
kill "${PIDS[@]}" 2>/dev/null || true
wait "${PIDS[@]}" 2>/dev/null || true
grep -hE "Graphics preset|Hollow.Graphics.Dump|Squad Resonance|Tool beam on|Hollow.Perf: " "$OUT/host.log" | sed 's/^\[[^]]*\]\[[^]]*\]//' | sed -E 's/Log[A-Za-z]+: (Display: )?//' | cut -c1-600
if [[ "$PROFILE" == 1 ]]; then
	echo "GPU profile (inclusive ms, top passes):"
	awk '/GPU Profile for Frame/{f=1} f' "$OUT/host.log" | grep -E "^\[.*LogRHI: +[0-9.]+%" | sed -E 's/^\[[^]]*\]\[[^]]*\]LogRHI: //' | head -70
fi
echo "Logs: $OUT"
