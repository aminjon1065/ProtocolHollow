#!/usr/bin/env bash
# Bug-bash playthroughs of a packaged build on this Mac (S4-06, ADR-0024), Null online subsystem, local IP. The menus
# are driven with keys (Hollow.UI.PressKey), the way a player does: Esc opens the system menu, the d-pad moves the focus,
# A (Gamepad_FaceButton_Bottom) presses the button (a synthetic keyboard Enter does not reach CommonUI's click action).
#   solo   E2: main menu "Play solo" -> hub -> system menu "Start raid" -> lift the core -> extract -> debrief -> hub ->
#          "Leave session" -> main menu.
#   coop   host + 3 headless clients: "Host co-op", clients join, "Start raid"; the host lifts the core, a second player
#          trying to join the carry is refused (co-op carry was cut, S3-06) without harm, client 1 shouts (a simulated
#          microphone, S4-07: the host hears it as Hollow.Noise.Voice.Shout), everyone extracts;
#          back in the hub a second raid: the clients die, the host extracts; results and profiles on every machine.
#   soak   host + 3 headless clients, a raid of --minutes (default 20) with the AI alive (the players take no damage:
#          Hollow.Dev.Invulnerable) and something happening every
#          minute (doors, welding, a breach, the core, mines, Resonance, a client's shout every third minute), a CSV capture of the whole raid
#          (Hollow.Perf.Capture: hitches, memory), then everyone extracts.
# Every log is scanned for errors, ensures and crashes. Output: Saved/Playthrough/<stamp>-<mode>/.
#
# Usage: Build/Scripts/playthrough-mac.sh <solo|coop|soak> [--app PATH] [--minutes N] [--seed N]
#   --app  the packaged game (default Packages/Mac/Development/ProtocolHollow.app, Build/Scripts/package-mac.sh)
set -euo pipefail
source "$(dirname "$0")/hollow-common.sh"

MODE="${1:-}"
[[ "$MODE" == solo || "$MODE" == coop || "$MODE" == soak ]] || { sed -n '2,15p' "$0"; exit 2; }
shift
APP="$HOLLOW_PROJECT_ROOT/Packages/Mac/Development/ProtocolHollow.app"
MINUTES=20
SEED=7
while [[ $# -gt 0 ]]; do
	case "$1" in
		--app) APP="$2"; shift 2 ;;
		--minutes) MINUTES="$2"; shift 2 ;;
		--seed) SEED="$2"; shift 2 ;;
		*) echo "Unknown argument: $1" >&2; exit 2 ;;
	esac
done
GAME="$APP/Contents/MacOS/ProtocolHollow"
[[ -x "$GAME" ]] || { echo "No packaged game at $APP (Build/Scripts/package-mac.sh)" >&2; exit 2; }

OUT="$HOLLOW_PROJECT_ROOT/Saved/Playthrough/$(date +%Y%m%d-%H%M%S)-$MODE"
mkdir -p "$OUT"
COMMON=(-nosplash -unattended -HollowPreset=Low
	"-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=Null" "-ini:Engine:[OnlineSubsystemSteam]:bEnabled=false")
VIEW=(-RenderOffScreen -windowed -ResX=1280 -ResY=720)
HEADLESS=(-nullrhi -nosound)
PIDS=()
trap 'kill ${PIDS[@]+"${PIDS[@]}"} 2>/dev/null || true' EXIT   # never leave a game running
slot() { echo "-ini:Game:[/Script/HollowEconomy.HollowEconomySettings]:SaveSlotName=Playthrough$1"; }
wait_for() { local i; for ((i = 0; i < $3; i++)); do grep -qE "$2" "$1" 2>/dev/null && return 0; sleep 1; done; return 1; }
# A packaged Mac game keeps its Saved folder here (no sandbox, ADR-0024): every playthrough starts with fresh profiles.
GAME_SAVES="$HOME/Library/Application Support/Epic/ProtocolHollow/Saved/SaveGames"
launch() { local name="$1"; shift; rm -f "$GAME_SAVES/Playthrough$name.sav"
	"$GAME" "${COMMON[@]}" "$@" "$(slot "$name")" -abslog="$OUT/$name.log" > /dev/null 2>&1 & PIDS+=($!); }
key() { echo "Hollow.Dev.RunAfter $1 Hollow.UI.PressKey $2"; }
DOWN=Gamepad_DPad_Down
PRESS=Gamepad_FaceButton_Bottom

H=()   # host commands
C=()   # commands of every client
C1=()  # commands of client 1 only
h() { H+=("$1"); }
case "$MODE" in
	solo)
		h "$(key 6 $DOWN)"; h "$(key 7 $PRESS)"                                       # main menu: Host co-op -> Play solo
		h "$(key 16 Escape)"; h "$(key 17 $DOWN)"; h "$(key 18 $PRESS)"               # system menu: Resume -> Start raid
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 3 Hollow.Dev.GoToNearestCore"
		h "Hollow.Dev.RunInRaid 4 Hollow.Interact"
		h "Hollow.Dev.RunInRaid 6 Hollow.Dev.GoToExtraction"
		h "Hollow.Dev.RunInRaid 45 Hollow.Profile.Dump"                              # back in the hub after the debrief
		h "Hollow.Dev.RunInRaid 46 Hollow.UI.PressKey Escape"                        # the debrief, unread, shows again in the hub
		h "Hollow.Dev.RunInRaid 47 Hollow.UI.PressKey Escape"                        # system menu
		h "Hollow.Dev.RunInRaid 48 Hollow.UI.PressKey $DOWN"
		h "Hollow.Dev.RunInRaid 49 Hollow.UI.PressKey $DOWN"
		h "Hollow.Dev.RunInRaid 50 Hollow.UI.PressKey $PRESS"                        # Resume, Start raid -> Leave session
		h "Hollow.Dev.RunInRaid 57 Hollow.Session.Status"
		h "Hollow.Dev.RunInRaid 58 Hollow.UI.Dump"                                   # the main menu again
		END='Hollow[.]UI[.]Dump: layer'   # an output line (the command line in the log names the command too)
		;;
	coop)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op
		h "$(key 30 Escape)"; h "$(key 31 $DOWN)"; h "$(key 32 $DOWN)"; h "$(key 33 $PRESS)"   # Resume, Invite -> Start raid
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 3 Hollow.Dev.GoToNearestCore All"
		h "Hollow.Dev.RunInRaid 4 Hollow.Interact"
		C1+=("Hollow.Dev.RunInRaid 6 Hollow.Interact")                               # a second carrier: refused (S3-06 cut)
		C1+=("Hollow.Dev.RunInRaid 7 Hollow.Voice.Simulate 230 3")                   # a shout (S4-07): the host logs Voice ... Shout
		C1+=("Hollow.Dev.RunInRaid 8 Hollow.Voice.Dump")
		h "Hollow.Dev.RunInRaid 9 Hollow.Carry.Dump"
		h "Hollow.Dev.RunInRaid 12 Hollow.Dev.GoToExtraction All"
		# The second raid: registered once the first is over, so these wait for the next ready raid.
		h "Hollow.Dev.RunInRaid 50 Hollow.Dev.RunInRaid 4 Hollow.Dev.Kill Others"
		h "Hollow.Dev.RunInRaid 50 Hollow.Dev.RunInRaid 8 Hollow.Dev.GoToExtraction"
		h "Hollow.Dev.RunInRaid 50 Hollow.Dev.RunInRaid 45 Hollow.Profile.Dump"
		h "Hollow.Dev.RunInRaid 52 Hollow.UI.PressKey Escape"                        # the unread debrief
		h "Hollow.Dev.RunInRaid 53 Hollow.UI.PressKey Escape"                        # system menu
		h "Hollow.Dev.RunInRaid 54 Hollow.UI.PressKey $DOWN"
		h "Hollow.Dev.RunInRaid 55 Hollow.UI.PressKey $DOWN"
		h "Hollow.Dev.RunInRaid 56 Hollow.UI.PressKey $PRESS"                        # Resume, Invite -> Start raid
		C+=("Hollow.Dev.RunInRaid 50 Hollow.Dev.RunInRaid 46 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	soak)
		h "$(key 6 $PRESS)"
		h "$(key 30 Escape)"; h "$(key 31 $DOWN)"; h "$(key 32 $DOWN)"; h "$(key 33 $PRESS)"
		SECONDS_TOTAL=$((MINUTES * 60))
		h "Hollow.Dev.RunInRaid 1 Hollow.Dev.Invulnerable All"                       # the AI hunts, nobody dies: the raid lasts
		h "Hollow.Dev.RunInRaid 2 Hollow.Perf.Capture Soak $SECONDS_TOTAL 0"
		ACTIONS=("Hollow.Dev.GoToNearestDoor All" "Hollow.Interact" "Hollow.Dev.GoToNearestCore All" "Hollow.Interact"
			"Hollow.Dev.GoToNearestBulkhead" "Hollow.Dev.Breach 50" "Hollow.Resonance.Add 20" "Hollow.Interact"
			"Hollow.Dev.GoToNearestDoor" "Hollow.Dev.WeldNearestDoor")
		for ((m = 1; m < MINUTES; m++)); do
			h "Hollow.Dev.RunInRaid $((m * 60)) ${ACTIONS[$(((m - 1) % ${#ACTIONS[@]}))]}"
			C+=("Hollow.Dev.RunInRaid $((m * 60 + 5)) Hollow.Tool.Hold Hollow.Input.SecondaryUse 8")
			((m % 4 == 0)) && C1+=("Hollow.Dev.RunInRaid $((m * 60 + 20)) Hollow.Tool.Hold Hollow.Input.Gadget 0.2")
			((m % 3 == 0)) && C1+=("Hollow.Dev.RunInRaid $((m * 60 + 30)) Hollow.Voice.Simulate 230 3")   # a shout the AI hears
		done
		h "Hollow.Dev.RunInRaid $((SECONDS_TOTAL + 5)) Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid $((SECONDS_TOTAL + 50)) Hollow.Profile.Dump"
		C+=("Hollow.Dev.RunInRaid $((SECONDS_TOTAL + 51)) Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
esac
join() { local IFS=','; echo "$*"; }

if [[ "$MODE" == solo ]]; then
	launch Host "${VIEW[@]}" -ExecCmds="$(join "${H[@]}")"
	wait_for "$OUT/Host.log" "$END" 400 || echo "the solo playthrough did not finish"
else
	launch Host "${VIEW[@]}" -ExecCmds="$(join "${H[@]}")"
	wait_for "$OUT/Host.log" 'listening on port' 240 || { echo "the host never listened"; exit 1; }
	for i in 1 2 3; do
		CMDS=("t.MaxFPS 30" "open 127.0.0.1" ${C[@]+"${C[@]}"})
		[[ "$i" == 1 ]] && CMDS+=(${C1[@]+"${C1[@]}"})
		launch "Client$i" "${HEADLESS[@]}" -ExecCmds="$(join "${CMDS[@]}")"
	done
	LIMIT=500
	[[ "$MODE" == soak ]] && LIMIT=$((MINUTES * 60 + 600))
	wait_for "$OUT/Client1.log" "$END" "$LIMIT" || echo "the $MODE playthrough did not finish"
fi
sleep 3
kill ${PIDS[@]+"${PIDS[@]}"} 2>/dev/null || true
wait ${PIDS[@]+"${PIDS[@]}"} 2>/dev/null || true

strip() { sed 's/^\[[^]]*\]\[[^]]*\]//' | sed -E 's/Log[A-Za-z]+: (Display: )?//' | cut -c1-260; }
for log in "$OUT"/*.log; do
	echo "===== $(basename "$log" .log)"
	grep -hE "UI up for|Session: State|Raid .*: .* -> |result (Extracted|KilledInAction|MissingInAction|SignalLost|Abandoned)|every hash matches|debrief|Hollow.Profile.Dump: [0-9]|Carry.Dump: |Hollow.Perf: Soak|Hollow.UI.Dump: layer|Hollow.Dev.RunAfter: Hollow.UI|Voice: .*(Shout|Speech|Whisper)|Hollow.Voice.Dump" "$log" | grep -v "Command Line\|commandline" | strip | head -40
done
echo "===== errors, ensures, crashes"
grep -hE "(LogHollow[A-Za-z]*|LogNet|LogAbilitySystem|LogOnline|LogGameMode|LogLoad|LogStreaming|LogScript): (Error|Warning)|Ensure condition failed|Assertion failed|Fatal error|appError" "$OUT"/*.log | strip | sort | uniq -c | sort -rn | head -30
echo "Logs: $OUT"
