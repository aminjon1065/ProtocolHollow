#!/usr/bin/env bash
# Bug-bash playthroughs of a packaged build on this Mac (S4-06, ADR-0024), Null online subsystem, local IP. The menus
# are driven with keys (Hollow.UI.PressKey), the way a player does: Esc opens the system menu, the d-pad moves the focus,
# A (Gamepad_FaceButton_Bottom) presses the button (a synthetic keyboard Enter does not reach CommonUI's click action).
#   solo   E2: main menu "Play solo" -> hub -> system menu "Start raid" -> lift the core -> extract -> debrief -> hub ->
#          "Leave session" -> main menu.
#   coop   host + 3 headless clients: "Host co-op", clients join, "Start raid"; the host lifts the core, client 1 takes
#          the other side and lets go again (S3-06), client 1 shouts (a simulated
#          microphone, S4-07: the host hears it as Hollow.Noise.Voice.Shout), client 1 switches its helmet lamp off and on
#          (S4-08: every machine dumps the lamps in between, the host its visor), everyone extracts;
#          back in the hub a second raid: the clients die, the host extracts; results and profiles on every machine.
#   soak   host + 3 headless clients, a raid of --minutes (default 20) with the AI alive (the players take no damage:
#          Hollow.Dev.Invulnerable) and something happening every
#          minute (doors, welding, a breach, the core, mines, Resonance, a client's shout every third minute, its lamp switched
#          every second minute), a CSV capture of the whole raid
#          (Hollow.Perf.Capture: hitches, memory), then everyone extracts.
#   carry  S3-06 co-op carry (ADR-0028): host + 1 headless client behind an emulated network (--net), walking with real
#          input (Hollow.Dev.WalkRoute, Hollow.Dev.FollowCarry): the client lifts the core, the host joins, the client
#          leads through 3 rooms; the client lets go; it joins the host, who leads through 2 rooms; the host walks off
#          (the tether breaks), the client carries alone through 2 rooms; in a second raid the host joins the client,
#          the client dies, the host extracts with the core. Every carrier logs its corrections (p.NetShowCorrections).
#   carrysolo  ADR-0015 investigation: host + 1 headless client behind --net; the client walks 2 rooms carrying the core
#          alone, then 2 rooms without it (the control), logging corrections.
#   loadout  S5-01 raid inventory (ADR-0030): host + 1 headless client behind --net, the client's profile without pulse
#          mines; both dump their inventories; the client presses G (no mine), the host lays one; the client burns fuel
#          with the cutter, refuels (R: one cell) and presses R again on a full tank (refused); both extract; the
#          profiles show what was spent.
# Every log is scanned for errors, ensures and crashes. Output: Saved/Playthrough/<stamp>-<mode>/.
#
# Usage: Build/Scripts/playthrough-mac.sh <solo|coop|soak|carry|carrysolo|loadout> [--app PATH] [--minutes N] [--seed N] [--net 150|bad]
#   --app  the packaged game (default Packages/Mac/Development/ProtocolHollow.app, Build/Scripts/package-mac.sh)
#   --net  the clients' network: 0 = none, 150 = 150 ms (-PktLag=150, the carry and loadout modes' default), bad = Network Emulation
#          Bad (100-200 ms each way, 5 % loss)
#   --client-fps  frame limit of the headless clients (default 30)
set -euo pipefail
source "$(dirname "$0")/hollow-common.sh"

MODE="${1:-}"
[[ "$MODE" == solo || "$MODE" == coop || "$MODE" == soak || "$MODE" == carry || "$MODE" == carrysolo || "$MODE" == loadout ]] || { sed -n '2,31p' "$0"; exit 2; }
shift
APP="$HOLLOW_PROJECT_ROOT/Packages/Mac/Development/ProtocolHollow.app"
MINUTES=20
SEED=7
NET=none
CLIENT_FPS=30
while [[ $# -gt 0 ]]; do
	case "$1" in
		--app) APP="$2"; shift 2 ;;
		--minutes) MINUTES="$2"; shift 2 ;;
		--seed) SEED="$2"; shift 2 ;;
		--net) NET="$2"; shift 2 ;;
		--client-fps) CLIENT_FPS="$2"; shift 2 ;;
		*) echo "Unknown argument: $1" >&2; exit 2 ;;
	esac
done
case "$NET" in
	none|0) NET_ARGS=() ;;
	150) NET_ARGS=(-PktLag=150) ;;
	bad) NET_ARGS=(-PktEmulationProfile=Bad) ;;
	*) echo "Unknown --net: $NET (0, 150 or bad)" >&2; exit 2 ;;
esac
if [[ ("$MODE" == carry* || "$MODE" == loadout) && "$NET" == none ]]; then
	NET_ARGS=(-PktLag=150)   # the carry and loadout smokes always run behind a slow network
fi
GAME="$APP/Contents/MacOS/ProtocolHollow"
[[ -x "$GAME" ]] || { echo "No packaged game at $APP (Build/Scripts/package-mac.sh)" >&2; exit 2; }

NET_SUFFIX=""
[[ "$NET" != none ]] && NET_SUFFIX="-$NET"
OUT="$HOLLOW_PROJECT_ROOT/Saved/Playthrough/$(date +%Y%m%d-%H%M%S)-$MODE$NET_SUFFIX"
mkdir -p "$OUT"
COMMON=(-nosplash -unattended -HollowPreset=Low
	"-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=Null" "-ini:Engine:[OnlineSubsystemSteam]:bEnabled=false")
VIEW=(-RenderOffScreen -windowed -ResX=1280 -ResY=720)
HEADLESS=(-nullrhi -nosound)
CLIENT_STAGGER=3   # seconds between client starts: four copies starting at once crashed in AppKit twice (KnownIssues V-6)
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
		h "Hollow.Dev.RunInRaid 5 Hollow.Dev.GoToNearestCore Others"                 # the clients by the core in the host's hands
		C1+=("Hollow.Dev.RunInRaid 5.5 Hollow.Dev.FaceCore")
		C1+=("Hollow.Dev.RunInRaid 6 Hollow.Interact")                               # S3-06: client 1 takes the other side
		h "Hollow.Dev.RunInRaid 6.8 Hollow.Carry.Dump"
		C1+=("Hollow.Dev.RunInRaid 7 Hollow.Carry.Dump")
		C1+=("Hollow.Dev.RunInRaid 8.5 Hollow.Interact")                             # ...and lets go: the host carries alone
		C1+=("Hollow.Dev.RunInRaid 7 Hollow.Voice.Simulate 230 3")                   # a shout (S4-07): the host logs Voice ... Shout
		C1+=("Hollow.Dev.RunInRaid 8 Hollow.Voice.Dump")
		C1+=("Hollow.Dev.RunInRaid 9 Hollow.Lamp.Toggle")                           # S4-08: off, predicted; the others see it off
		h "Hollow.Dev.RunInRaid 9 Hollow.Carry.Dump"
		h "Hollow.Dev.RunInRaid 10.5 Hollow.Lamp.Dump"
		h "Hollow.Dev.RunInRaid 10.6 Hollow.UI.Dump"                                 # the host's visor: Resonance, voice, lamp
		C+=("Hollow.Dev.RunInRaid 10.5 Hollow.Lamp.Dump")
		C1+=("Hollow.Dev.RunInRaid 11 Hollow.Lamp.Toggle")                          # on again before the extraction
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
	carry)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op
		h "$(key 30 Escape)"; h "$(key 31 $DOWN)"; h "$(key 32 $DOWN)"; h "$(key 33 $PRESS)"   # Resume, Invite -> Start raid
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 2.5 Hollow.Dev.OpenDoors"
		h "Hollow.Dev.RunInRaid 3 Hollow.Dev.GoToNearestCore All"
		# 1. The client lifts, the host joins (Solo -> Coop), the client leads through 3 rooms, the host follows.
		C1+=("Hollow.Dev.RunInRaid 4 Hollow.Interact")
		h "Hollow.Dev.RunInRaid 5 Hollow.Dev.FaceCore"
		h "Hollow.Dev.RunInRaid 5.5 Hollow.Interact"
		h "Hollow.Dev.RunInRaid 6.5 Hollow.Carry.Dump"; C1+=("Hollow.Dev.RunInRaid 6.5 Hollow.Carry.Dump")
		C1+=("Hollow.Dev.RunInRaid 7 Hollow.Dev.WalkRoute 3"); h "Hollow.Dev.RunInRaid 7 Hollow.Dev.FollowCarry"
		h "Hollow.Dev.RunInRaid 48 Hollow.Carry.Dump"; C1+=("Hollow.Dev.RunInRaid 48 Hollow.Carry.Dump")
		# 2. The client lets go (Coop -> Solo on the host); its movement goes back to the server.
		C1+=("Hollow.Dev.RunInRaid 50 Hollow.Interact")
		h "Hollow.Dev.RunInRaid 53 Hollow.Carry.Dump"; C1+=("Hollow.Dev.RunInRaid 53 Hollow.Carry.Dump")
		# 3. The client joins the host, who leads through 2 rooms.
		h "Hollow.Dev.RunInRaid 54 Hollow.Dev.GoToNearestCore Others"
		C1+=("Hollow.Dev.RunInRaid 55 Hollow.Dev.FaceCore"); C1+=("Hollow.Dev.RunInRaid 55.5 Hollow.Interact")
		h "Hollow.Dev.RunInRaid 57 Hollow.Dev.WalkRoute 2"; C1+=("Hollow.Dev.RunInRaid 57 Hollow.Dev.FollowCarry")
		h "Hollow.Dev.RunInRaid 88 Hollow.Carry.Dump"; C1+=("Hollow.Dev.RunInRaid 88 Hollow.Carry.Dump")
		# 4. The host walks off (teleported to the extraction: server-authoritative, the shuttle's countdown starts): the
		#    tether breaks, the client carries alone 2 rooms on and is left behind when the shuttle goes.
		h "Hollow.Dev.RunInRaid 90 Hollow.Dev.GoToExtraction"
		h "Hollow.Dev.RunInRaid 92 Hollow.Carry.Dump"; C1+=("Hollow.Dev.RunInRaid 92 Hollow.Carry.Dump")
		C1+=("Hollow.Dev.RunInRaid 93 Hollow.Dev.WalkRoute 2")
		C1+=("Hollow.Dev.RunInRaid 104 Hollow.Carry.Dump")
		# 5. A second raid (registered once the first is over): the client lifts, the host joins, the client dies
		#    (Coop -> Solo on the host), the host extracts with the core.
		h "Hollow.Dev.RunInRaid 142 Hollow.UI.PressKey Escape"                       # the unread debrief
		h "Hollow.Dev.RunInRaid 143 Hollow.UI.PressKey Escape"                       # system menu
		h "Hollow.Dev.RunInRaid 144 Hollow.UI.PressKey $DOWN"
		h "Hollow.Dev.RunInRaid 145 Hollow.UI.PressKey $DOWN"
		h "Hollow.Dev.RunInRaid 146 Hollow.UI.PressKey $PRESS"                       # Resume, Invite -> Start raid
		h "Hollow.Dev.RunInRaid 140 Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 140 Hollow.Dev.RunInRaid 3 Hollow.Dev.GoToNearestCore All"
		C1+=("Hollow.Dev.RunInRaid 140 Hollow.Dev.RunInRaid 4 Hollow.Interact")
		h "Hollow.Dev.RunInRaid 140 Hollow.Dev.RunInRaid 5 Hollow.Dev.FaceCore"
		h "Hollow.Dev.RunInRaid 140 Hollow.Dev.RunInRaid 5.5 Hollow.Interact"
		h "Hollow.Dev.RunInRaid 140 Hollow.Dev.RunInRaid 7 Hollow.Carry.Dump"
		h "Hollow.Dev.RunInRaid 140 Hollow.Dev.RunInRaid 8 Hollow.Dev.Kill Others"
		h "Hollow.Dev.RunInRaid 140 Hollow.Dev.RunInRaid 10 Hollow.Carry.Dump"
		h "Hollow.Dev.RunInRaid 140 Hollow.Dev.RunInRaid 11 Hollow.Dev.GoToExtraction"
		h "Hollow.Dev.RunInRaid 140 Hollow.Dev.RunInRaid 45 Hollow.Profile.Dump"
		C+=("Hollow.Dev.RunInRaid 140 Hollow.Dev.RunInRaid 46 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	carrysolo)
		h "log LogNetPlayerMovement Log"                                              # the server's ForcePositionUpdate lines
		h "$(key 6 $PRESS)"
		h "$(key 30 Escape)"; h "$(key 31 $DOWN)"; h "$(key 32 $DOWN)"; h "$(key 33 $PRESS)"
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 2.5 Hollow.Dev.OpenDoors"
		h "Hollow.Dev.RunInRaid 3 Hollow.Dev.GoToNearestCore Others"
		C1+=("Hollow.Dev.RunInRaid 4 Hollow.Interact")
		C1+=("Hollow.Dev.RunInRaid 6 Hollow.Carry.Dump")
		C1+=("Hollow.Dev.RunInRaid 7 Hollow.Dev.WalkRoute 2")
		C1+=("Hollow.Dev.RunInRaid 40 Hollow.Carry.Dump")
		C1+=("Hollow.Dev.RunInRaid 41 Hollow.Interact")                              # put it down
		C1+=("Hollow.Dev.RunInRaid 45 Hollow.Dev.WalkRoute 2")                       # the control: the same walk without the core
		C1+=("Hollow.Dev.RunInRaid 78 Hollow.Carry.Dump")
		h "Hollow.Dev.RunInRaid 80 Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid 118 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 119 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	loadout)
		C1+=("Hollow.Profile.SetSlot DA_Item_PulseMine 0")                           # before the raid: client 1 brings no mines
		h "$(key 6 $PRESS)"
		h "$(key 30 Escape)"; h "$(key 31 $DOWN)"; h "$(key 32 $DOWN)"; h "$(key 33 $PRESS)"
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 3 Hollow.Inventory.Dump"; C+=("Hollow.Dev.RunInRaid 3 Hollow.Inventory.Dump")
		C1+=("Hollow.Dev.RunInRaid 4 Hollow.Tool.Hold Hollow.Input.Gadget 0.2")      # no mine in its inventory: nothing laid
		h "Hollow.Dev.RunInRaid 5 Hollow.Tool.Hold Hollow.Input.Gadget 0.2"          # the host lays one of its two
		C1+=("Hollow.Dev.RunInRaid 6 Hollow.Tool.Hold Hollow.Input.PrimaryUse 4")    # the cutter burns 20 fuel
		C1+=("Hollow.Dev.RunInRaid 11 Hollow.Tool.Hold Hollow.Input.Reload 0.2")     # a cell goes in
		C1+=("Hollow.Dev.RunInRaid 13 Hollow.Tool.Hold Hollow.Input.Reload 0.2")     # the tank is full: no second cell
		h "Hollow.Dev.RunInRaid 14 Hollow.Inventory.Dump"; C+=("Hollow.Dev.RunInRaid 14 Hollow.Inventory.Dump")
		h "Hollow.Dev.RunInRaid 14.5 Hollow.UI.Dump"                                 # the host's visor: its supplies
		h "Hollow.Dev.RunInRaid 15 Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid 60 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 61 Hollow.Profile.Dump")
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
			((m % 2 == 0)) && C1+=("Hollow.Dev.RunInRaid $((m * 60 + 40)) Hollow.Lamp.Toggle")            # off, on, off...
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
	CLIENTS=3
	[[ "$MODE" == carry* || "$MODE" == loadout ]] && CLIENTS=1
	for ((i = 1; i <= CLIENTS; i++)); do
		CMDS=("t.MaxFPS $CLIENT_FPS" "p.NetShowCorrections 1" "open 127.0.0.1" ${C[@]+"${C[@]}"})
		[[ "$i" == 1 ]] && CMDS+=(${C1[@]+"${C1[@]}"})
		launch "Client$i" "${HEADLESS[@]}" ${NET_ARGS[@]+"${NET_ARGS[@]}"} -ExecCmds="$(join "${CMDS[@]}")"
		[[ "$i" == 1 ]] && CLIENT1="${PIDS[${#PIDS[@]}-1]}"
		sleep "$CLIENT_STAGGER"
	done
	LIMIT=500
	[[ "$MODE" == soak ]] && LIMIT=$((MINUTES * 60 + 600))
	# Client 1 runs the scenario's client side and says when it is over; if it dies, say so now (KnownIssues V-6).
	for ((t = 0; t < LIMIT; t++)); do
		grep -qE "$END" "$OUT/Client1.log" 2> /dev/null && break
		kill -0 "$CLIENT1" 2> /dev/null || { echo "client 1 died after ${t} s (exit: see ~/Library/Logs/DiagnosticReports)"; break; }
		sleep 1
	done
	((t >= LIMIT)) && echo "the $MODE playthrough did not finish"
fi
sleep 3
kill ${PIDS[@]+"${PIDS[@]}"} 2>/dev/null || true
wait ${PIDS[@]+"${PIDS[@]}"} 2>/dev/null || true

strip() { sed 's/^\[[^]]*\]\[[^]]*\]//' | sed -E 's/Log[A-Za-z]+: (Display: )?//' | cut -c1-260; }
for log in "$OUT"/*.log; do
	echo "===== $(basename "$log" .log)"
	grep -hE "UI up for|Session: State|Raid .*: .* -> |result (Extracted|KilledInAction|MissingInAction|SignalLost|Abandoned)|every hash matches|debrief|Hollow.Profile.Dump: [0-9]|Carry.Dump: |Hollow.Perf: Soak|Hollow.UI.Dump: layer|Hollow.Dev.RunAfter: Hollow.UI|Voice: .*(Shout|Speech|Whisper)|Hollow.Voice.Dump|Hollow.Lamp.Dump|helmet lamp|Hollow.UI.Dump: (glass|resonance|voice|lamp)|WalkRoute: |FollowCarry: |OpenDoors: |carrier corrected|Client: Error for|client-authoritative|server-authoritative again|lets go|joins .* \\(Coop|tether breaks|sanity check|its moves show|lifted by|dropped by|ForcePositionUpdate|Inventory.Dump: |takes |uses [0-9]|lays |refuels|asks .* for the loadout|loadout of|Profile: loadout|Hollow.UI.Dump: supplies|Profile.SetSlot" "$log" | grep -v "Command Line\|commandline" | strip | head -90
done
echo "===== errors, ensures, crashes"
{ grep -hE "(LogHollow[A-Za-z]*|LogNet|LogAbilitySystem|LogOnline|LogGameMode|LogLoad|LogStreaming|LogScript): (Error|Warning)|Ensure condition failed|Assertion failed|Fatal error|appError" "$OUT"/*.log || true; } | strip | sort | uniq -c | sort -rn | head -30   # none is no failure
echo "Logs: $OUT"
