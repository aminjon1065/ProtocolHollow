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
#   --contract NAME  the raid's contract DA_Contract_NAME (Docks by default, Cathedral): the host picks it in the hub
#                 before the raid, the contract picks the sector (ADR-0041); the capture is named E9-PRESET-NAME
#   --altar stand|ritual  FAB-01 (ADR-0051), with --contract Cathedral: the host carries the core to the altar and stands
#                 there (stand), or lays it there (ritual: the 20 s ritual's Niagara plays from 9 s); the capture is
#                 named ...-Altar-stand|ritual. For the two to compare, start the capture early: --warmup 1 --seconds 18
#   --still       the host's view does not turn (with --altar: it faces the altar all along, the worst case; a
#                 --profilegpu frame then sees the effect)
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
CONTRACT=
ALTAR=
STILL=0
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
		--contract) CONTRACT="$2"; shift 2 ;;
		--altar) ALTAR="$2"; shift 2 ;;
		--still) STILL=1; shift ;;
		*) echo "Unknown argument: $1" >&2; exit 2 ;;
	esac
done
[[ -z "$ALTAR" || "$ALTAR" == stand || "$ALTAR" == ritual ]] || { echo "Unknown --altar: $ALTAR (stand or ritual)" >&2; exit 2; }
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
# The editor keeps its saves in the project, a packaged Mac game in Application Support (ADR-0024, as playthrough-mac.sh).
SAVES=("$HOLLOW_PROJECT_ROOT/Saved/SaveGames" "$HOME/Library/Application Support/Epic/ProtocolHollow/Saved/SaveGames")
COMMON=(-nosplash -unattended -nop4 "-HollowPreset=$PRESET"
	"-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=Null" "-ini:Engine:[OnlineSubsystemSteam]:bEnabled=false")
HEADLESS=(-nullrhi -nosound)
HEADLESS_FPS="t.MaxFPS 30"   # the clients only have to play: keep their CPU away from the host
slot() { echo "-ini:Game:[/Script/HollowEconomy.HollowEconomySettings]:SaveSlotName=$1"; }
wait_for() { local i; for ((i = 0; i < $3; i++)); do grep -qE "$2" "$1" 2>/dev/null && return 0; sleep 1; done; return 1; }

TOTAL=$(awk -v w="$WARMUP" -v s="$SECONDS_TO_CAPTURE" 'BEGIN { print w + s + 5 }')
H="r.VSync 0,t.MaxFPS 0,Hollow.Session.Host InviteOnly,Hollow.Dev.RunAfter 45 Hollow.Raid.Start 7"
# The host's choice in the hub, as at the contracts terminal; the clients learn the sector from the manifest.
[[ -n "$CONTRACT" ]] && H="$H,Hollow.Dev.RunAfter 40 Hollow.Hub.Select DA_Contract_$CONTRACT"
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
if [[ -n "$ALTAR" ]]; then   # FAB-01: the same spot and view, with the ritual's effects or without
	r 7.8 "Hollow.Dev.GoToAltar"
	[[ "$ALTAR" == ritual ]] && r 8.6 "Hollow.Interact"
	[[ "$STILL" == 1 ]] || r 9 "Hollow.Dev.LookAround $TOTAL 1"
elif [[ "$STILL" != 1 ]]; then
	r 9 "Hollow.Dev.LookAround $TOTAL 2"
fi
NAME="E9-$PRESET${CONTRACT:+-$CONTRACT}${ALTAR:+-Altar-$ALTAR}"
[[ "$STILL" == 1 ]] && NAME="$NAME-Still"
r 9 "Hollow.Perf.Capture $NAME $SECONDS_TO_CAPTURE $WARMUP"
W="Hollow.Dev.RunInRaid 8 Hollow.Tool.Hold Hollow.Input.SecondaryUse $TOTAL"

TRACEARGS=()
[[ "$TRACE" == 1 ]] && TRACEARGS=(-trace=default,gpu,frame,net,physics,audio "-tracefile=$OUT/host.utrace" -statnamedevents)
# Fresh profiles every capture: the raid a capture cuts off costs the loadout (escrow), so the welders would come back
# empty-handed (KnownIssues AK-13).
for d in "${SAVES[@]}"; do for s in PerfHost PerfClient1 PerfClient2 PerfClient3; do rm -f "$d/$s.sav"; done; done
PIDS=()
trap 'kill ${PIDS[@]+"${PIDS[@]}"} 2>/dev/null || true' EXIT   # never leave a game running: it would load the next capture
launch "$OUT/host.log" "${COMMON[@]}" -RenderOffScreen -windowed -ResX=1920 -ResY=1080 "$(slot PerfHost)" ${TRACEARGS[@]+"${TRACEARGS[@]}"} -ExecCmds="$H"
# The game's driver, not the trace server's "Control listening on port 1985" (KnownIssues RG-1).
wait_for "$OUT/host.log" 'IpNetDriver listening on port' 240 || { echo "host never listened"; kill "${PIDS[@]}"; exit 1; }
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
