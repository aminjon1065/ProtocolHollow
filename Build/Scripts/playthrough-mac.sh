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
#   hub    S5-03 hub shuttle Skiff (ADR-0032): host + 1 headless client behind --net. The host buys the fuel reserve, the
#          client the core contract; in the hub the client opens the workbench and the locker (only it, their screens on
#          its machine), asks for the contract (refused) and looks at the contracts screen (no Launch: S5-04), the host
#          opens the contracts terminal and launches from its screen with the gamepad (S5-04);
#          in the raid the host carries the core out with both aboard: the host's two free fuel cells, the client's 600.
#   classes  S6-01 player classes (ADR-0034): host + 1 headless client behind --net. In the hub the host picks the
#          Techno-Occultist at its locker (arrows to the class list, A), the client stays an Engineer; both machines
#          dump every player's class, tag and class attributes and their visor colour, in the hub and in the raid
#          (the host's loadout is the Occultist's: the lantern and three cells), and again back in the hub.
#   lantern  S6-02 Aether lantern (ADR-0035): host + 1 headless client behind --net. In the hub the client picks the
#          Techno-Occultist (its starting loadout: the lantern); in the raid it lights a Pilgrim the host spawns (slowed
#          x0.6, exposed, then back to its speed), then stands by the host carrying the core: the carrier's Resonance
#          gain without the aura and in it (the summary computes both).
#   ward   S6-03 ritual ward (ADR-0036): host + 1 headless client behind --net. In the hub the client picks the
#          Techno-Occultist (its starting loadout: two wards); in the raid both stand before a Pilgrim the host spawns,
#          the host with Resonance to spare; the client presses G: the Pilgrim inside is slowed x0.5, the host's
#          Resonance drains; both machines dump the same ward (time left, then pulsing, then gone) and the host shoots it.
#   relics S6-04 occult relics (ADR-0037): host + 1 headless client behind --net. In the hub the client crafts the Ring of
#          Silence and the Kinetic Brace, equips the ring (the brace refused: one relic slot; a bought lantern refused:
#          the cutter holds the primary input), buys the Reliquary, equips the brace, insures the ring; in the raid both
#          dump the gear, the client carries the core faster than the host does, then dies: the insured ring comes home,
#          the brace is lost.
#   cathedral  S7-01 Reactor Cathedral (ADR-0038): host + 1 headless client behind --net, the host's raid sector
#          overridden to HollowSector_Cathedral (dev: the contract picks it from S7-04 on). Both machines dump the sector
#          (same layout hash, the plugin Active), the host its generated map and the Standard and Heavy paths to the
#          extraction; the AI lives for 20 s (both dump it); the host walks 4 rooms with real input taking pictures
#          with the lighting show flag off (no lights before S7-03), then carries the core out and both extract.
#   altar  S7-02 Cathedral mechanics (ADR-0039): host + 1 headless client behind --net, raid sector overridden to the
#          Cathedral. The client carries the core to the Purification Altar and lays it there (Interact on a carry
#          receiver); both dump the altar through the 20 s ritual, the AI and the heat (the bell draws them); the core
#          comes off purified. The client stands in an electric arc, vulnerable: struck; the host cuts its breaker: no more
#          strikes. The client carries the purified core out: both extract for 1250 credits.
#   screens  S5-04 hub screens (ADR-0033), one rendered process: the workbench (craft with A, RB to the shop, D-pad and A
#          to buy, E / Q to the upgrades and back, Esc), the locker (take with the keyboard and A, insure, B), the
#          contracts screen (A on Launch); a screenshot of each; the raid, then the hub again.
#   economy  S5-02 hub economy (ADR-0031), one process: in the hub a refused craft (one shard short), a shard bought, two
#          pulse mines and a fuel cell crafted, the mines equipped, the cutter insured, the cell sold; in the raid a
#          transaction refused, then the player dies: the insured welder and cutter come home, their insurance spent.
#   loadout  S5-01 raid inventory (ADR-0030): host + 1 headless client behind --net, the client's profile without pulse
#          mines; both dump their inventories; the client presses G (no mine), the host lays one; the client burns fuel
#          with the cutter, refuels (R: one cell) and presses R again on a full tank (refused); both extract; the
#          profiles show what was spent.
# Every log is scanned for errors, ensures and crashes. Output: Saved/Playthrough/<stamp>-<mode>/.
#
# Usage: Build/Scripts/playthrough-mac.sh <solo|coop|soak|carry|carrysolo|loadout|economy|hub|screens|classes|lantern|ward|relics|cathedral|altar> [--app PATH] [--minutes N] [--seed N] [--net 150|bad]
#   --app  the packaged game (default Packages/Mac/Development/ProtocolHollow.app, Build/Scripts/package-mac.sh)
#   --net  the clients' network: 0 = none, 150 = 150 ms (-PktLag=150, the carry and loadout modes' default), bad = Network Emulation
#          Bad (100-200 ms each way, 5 % loss)
#   --client-fps  frame limit of the headless clients (default 30)
set -euo pipefail
source "$(dirname "$0")/hollow-common.sh"

MODE="${1:-}"
[[ "$MODE" == solo || "$MODE" == coop || "$MODE" == soak || "$MODE" == carry || "$MODE" == carrysolo || "$MODE" == loadout || "$MODE" == economy || "$MODE" == hub || "$MODE" == screens || "$MODE" == classes || "$MODE" == lantern || "$MODE" == ward || "$MODE" == relics || "$MODE" == cathedral || "$MODE" == altar ]] || { sed -n '2,72p' "$0"; exit 2; }
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
if [[ ("$MODE" == carry* || "$MODE" == loadout || "$MODE" == hub || "$MODE" == classes || "$MODE" == lantern || "$MODE" == ward || "$MODE" == relics || "$MODE" == cathedral || "$MODE" == altar) && "$NET" == none ]]; then
	NET_ARGS=(-PktLag=150)   # the co-op smokes but coop and soak always run behind a slow network
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
HOST_ARGS=()   # extra arguments of the host
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
	hub)
		h "Hollow.Dev.RunAfter 3 Hollow.Shop.BuyUpgrade DA_Upgrade_FuelReserve"     # the host's ship stocks two cells a raid
		C1+=("Hollow.Shop.BuyUpgrade DA_Upgrade_CoreBonus")                          # the client's ship: +20 % for cores
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		h "Hollow.Dev.RunAfter 12 Hollow.Hub.Dump"
		h "Hollow.Dev.RunAfter 13 Hollow.UI.Shot Skiff_Start"
		# Host time; the client starts some 10 s later (it waits for the host to listen) and counts from its own start.
		h "Hollow.Dev.RunAfter 20 Hollow.Dev.GoToTerminal Workbench Others"
		h "Hollow.Dev.RunAfter 26 Hollow.Dev.GoToTerminal Workbench Others"
		C1+=("Hollow.Dev.RunAfter 20 Hollow.Interact")                              # the client's workbench, on the client only
		C1+=("Hollow.Dev.RunAfter 21 Hollow.Hub.Select DA_Contract_Docks")          # refused: only the host picks
		C1+=("Hollow.Dev.RunAfter 22 Hollow.Hub.Dump")
		h "Hollow.Dev.RunAfter 28 Hollow.Dev.GoToTerminal Contracts"
		h "Hollow.Dev.RunAfter 29 Hollow.Interact"                                  # its screen, focus on Launch (S5-04)
		h "Hollow.Dev.RunAfter 31 Hollow.UI.Shot Skiff_Contracts"
		# The client starts 2-12 s after the host: it is placed at the locker before its earliest E and well before launch.
		h "Hollow.Dev.RunAfter 34 Hollow.Dev.GoToTerminal Locker Others"
		h "Hollow.Dev.RunAfter 38 Hollow.Dev.GoToTerminal Locker Others"
		C1+=("Hollow.Dev.RunAfter 38 Hollow.Interact")                              # the locker, on the client only
		C1+=("Hollow.Dev.RunAfter 41 Hollow.Interact")
		C1+=("Hollow.Dev.RunAfter 42 Hollow.Hub.Dump")
		C1+=("Hollow.Dev.RunAfter 42.5 Hollow.UI.Dump")                             # the client's locker screen
		C1+=("Hollow.Dev.RunAfter 44 Hollow.Hub.Open Contracts")                    # a guest's contracts screen: no Launch
		C1+=("Hollow.Dev.RunAfter 45 Hollow.UI.Dump")
		C1+=("Hollow.Dev.RunAfter 46 Hollow.Hub.Close")
		h "Hollow.Dev.RunAfter 52 Hollow.Hub.Dump"
		h "Hollow.Dev.RunAfter 55 Hollow.UI.Dump"
		h "$(key 56 $PRESS)"                                                          # A on Launch: the raid
		C+=("Hollow.Dev.RunInRaid 1 Hollow.UI.Dump")                                 # the screens are gone with the hub
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 3 Hollow.Dev.GoToNearestCore All"
		h "Hollow.Dev.RunInRaid 4 Hollow.Interact"
		h "Hollow.Dev.RunInRaid 5 Hollow.Inventory.Dump"; C+=("Hollow.Dev.RunInRaid 5 Hollow.Inventory.Dump")
		h "Hollow.Dev.RunInRaid 6 Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid 45 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 46 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	classes)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		h "Hollow.Dev.RunAfter 14 Hollow.Dev.GoToTerminal Locker"
		h "Hollow.Dev.RunAfter 15 Hollow.Interact"                                  # its screen; focus on the first button that acts
		h "$(key 16 Right)"; h "$(key 16.5 Right)"; h "$(key 17 Up)"                  # over to the class list: Techno-Occultist
		h "Hollow.Dev.RunAfter 17.5 Hollow.UI.Dump"; h "$(key 18 $PRESS)"             # A: Choose
		h "Hollow.Dev.RunAfter 19 Hollow.UI.Dump"; h "Hollow.Dev.RunAfter 19.5 Hollow.UI.Shot Classes_Locker"
		h "$(key 20 Gamepad_FaceButton_Right)"                                        # B: back to the game
		h "Hollow.Dev.RunAfter 21 Hollow.UI.Shot Classes_Visor"
		h "Hollow.Dev.RunAfter 30 Hollow.Class.Dump"; h "Hollow.Dev.RunAfter 30.5 Hollow.UI.Dump"
		C1+=("Hollow.Dev.RunAfter 26 Hollow.Class.Dump"); C1+=("Hollow.Dev.RunAfter 26.5 Hollow.UI.Dump")   # the client: an Engineer
		h "Hollow.Dev.RunAfter 36 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 3 Hollow.Class.Dump"; C+=("Hollow.Dev.RunInRaid 3 Hollow.Class.Dump")
		h "Hollow.Dev.RunInRaid 3.5 Hollow.Inventory.Dump"; C+=("Hollow.Dev.RunInRaid 3.5 Hollow.Inventory.Dump")
		h "Hollow.Dev.RunInRaid 4 Hollow.UI.Dump"; C+=("Hollow.Dev.RunInRaid 4 Hollow.UI.Dump")
		h "Hollow.Dev.RunInRaid 4.5 Hollow.UI.Shot Classes_Raid"
		h "Hollow.Dev.RunInRaid 5 Hollow.Dev.GoToNearestCore All"
		h "Hollow.Dev.RunInRaid 6 Hollow.Interact"
		h "Hollow.Dev.RunInRaid 7 Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid 44 Hollow.Class.Dump"; C+=("Hollow.Dev.RunInRaid 44 Hollow.Class.Dump")   # back in the hub
		h "Hollow.Dev.RunInRaid 45 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 46 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	lantern)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		C1+=("Hollow.Dev.RunAfter 20 Hollow.Class.Select DA_Class_Occultist")       # the client: its starting loadout, the lantern
		C1+=("Hollow.Dev.RunAfter 23 Hollow.Class.Dump")                            # (no Profile.Dump before the end: it is the END marker)
		h "Hollow.Dev.RunAfter 40 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"; h "Hollow.Dev.RunInRaid 2.2 Hollow.Dev.Invulnerable All"
		C+=("Hollow.Dev.RunInRaid 2.5 Hollow.Inventory.Dump")
		# 1. A Pilgrim in the light: the host spawns it, the client stands before it and lights it 4 s.
		h "Hollow.Dev.RunInRaid 3 Hollow.AI.Spawn 600"
		h "Hollow.Dev.RunInRaid 4 Hollow.Dev.GoToNearestEnemy Others"
		h "Hollow.Dev.RunInRaid 5 Hollow.AI.Dump"                                    # its speed before
		C1+=("Hollow.Dev.RunInRaid 6 Hollow.Dev.FaceNearestEnemy"); C1+=("Hollow.Dev.RunInRaid 6.3 Hollow.Tool.Hold Hollow.Input.PrimaryUse 4")
		h "Hollow.Dev.RunInRaid 8 Hollow.AI.Dump"                                    # exposed, slowed
		C1+=("Hollow.Dev.RunInRaid 8 Hollow.Tool.Info")
		h "Hollow.Dev.RunInRaid 14 Hollow.AI.Dump"                                   # the light gone: its speed back
		h "Hollow.Dev.RunInRaid 15 Hollow.AI.Kill"
		# 2. The host carries the core 5 s alone, then 5 s in the client's aura (its lantern on 8 s beside it).
		h "Hollow.Dev.RunInRaid 16 Hollow.Dev.GoToNearestCore All"
		h "Hollow.Dev.RunInRaid 17 Hollow.Interact"
		h "Hollow.Dev.RunInRaid 18 Hollow.Resonance.Dump"; h "Hollow.Dev.RunInRaid 23 Hollow.Resonance.Dump"
		C1+=("Hollow.Dev.RunInRaid 23 Hollow.Tool.Hold Hollow.Input.PrimaryUse 8")
		h "Hollow.Dev.RunInRaid 25 Hollow.Resonance.Dump"; h "Hollow.Dev.RunInRaid 30 Hollow.Resonance.Dump"
		C1+=("Hollow.Dev.RunInRaid 26 Hollow.Tool.Info"); C1+=("Hollow.Dev.RunInRaid 27 Hollow.Resonance.Dump")
		h "Hollow.Dev.RunInRaid 33 Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid 75 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 76 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	ward)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		C1+=("Hollow.Dev.RunAfter 20 Hollow.Class.Select DA_Class_Occultist")       # the client: its starting loadout, two wards
		C1+=("Hollow.Dev.RunAfter 23 Hollow.Class.Dump")                            # (no Profile.Dump before the end: it is the END marker)
		h "Hollow.Dev.RunAfter 40 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"; h "Hollow.Dev.RunInRaid 2.2 Hollow.Dev.Invulnerable All"
		C+=("Hollow.Dev.RunInRaid 2.5 Hollow.Inventory.Dump")
		h "Hollow.Dev.RunInRaid 3 Hollow.Resonance.Add 30"                           # the host's, for the ward to drain
		h "Hollow.Dev.RunInRaid 3.5 Hollow.AI.Spawn 600"
		h "Hollow.Dev.RunInRaid 4.5 Hollow.Dev.GoToNearestEnemy All"                 # both before the Pilgrim
		h "Hollow.Dev.RunInRaid 5.5 Hollow.AI.Dump"; h "Hollow.Dev.RunInRaid 5.6 Hollow.Resonance.Dump"
		C1+=("Hollow.Dev.RunInRaid 6 Hollow.Tool.Hold Hollow.Input.Gadget 0.2")    # G: the client lays its ward
		h "Hollow.Dev.RunInRaid 8 Hollow.Gadget.Dump"; C1+=("Hollow.Dev.RunInRaid 8 Hollow.Gadget.Dump")
		h "Hollow.Dev.RunInRaid 8.5 Hollow.AI.Dump"; h "Hollow.Dev.RunInRaid 8.6 Hollow.Resonance.Dump"
		h "Hollow.Dev.RunInRaid 9 Hollow.UI.Shot Ward_Circle"
		h "Hollow.Dev.RunInRaid 13.6 Hollow.Resonance.Dump"                          # 5 s later: drained
		C1+=("Hollow.Dev.RunInRaid 14 Hollow.Inventory.Dump")
		h "Hollow.Dev.RunInRaid 19.5 Hollow.Gadget.Dump"; C1+=("Hollow.Dev.RunInRaid 19.5 Hollow.Gadget.Dump")   # pulsing
		h "Hollow.Dev.RunInRaid 19.7 Hollow.UI.Shot Ward_Pulse"
		h "Hollow.Dev.RunInRaid 24 Hollow.Gadget.Dump"; C1+=("Hollow.Dev.RunInRaid 24 Hollow.Gadget.Dump")       # gone
		h "Hollow.Dev.RunInRaid 24.5 Hollow.AI.Dump"
		h "Hollow.Dev.RunInRaid 25 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 26 Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid 65 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 66 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	relics)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		# The client in the hub (an Engineer: welder 0, cutter 1, cells 2, mines 3): its relics.
		C1+=("Hollow.Dev.RunAfter 17 Hollow.Profile.Credits 1000")                 # dev: enough for the Reliquary too
		C1+=("Hollow.Dev.RunAfter 17.5 Hollow.Shop.Buy DA_Item_AetherShard 3"); C1+=("Hollow.Dev.RunAfter 18 Hollow.Shop.Buy DA_Item_Scrap 3")
		C1+=("Hollow.Dev.RunAfter 18.5 Hollow.Craft.Make DA_Recipe_Relic_RingOfSilence 1")
		C1+=("Hollow.Dev.RunAfter 19 Hollow.Craft.Make DA_Recipe_Relic_KineticBrace 1")
		C1+=("Hollow.Dev.RunAfter 19.5 Hollow.Profile.Equip DA_Item_Relic_RingOfSilence 1")
		C1+=("Hollow.Dev.RunAfter 20 Hollow.Profile.Equip DA_Item_Relic_KineticBrace 1")   # refused: one relic slot
		C1+=("Hollow.Dev.RunAfter 20.5 Hollow.Shop.Buy DA_Item_AetherLantern 1")
		C1+=("Hollow.Dev.RunAfter 21 Hollow.Profile.Equip DA_Item_AetherLantern 1")       # refused: the cutter's input
		C1+=("Hollow.Dev.RunAfter 21.5 Hollow.Shop.BuyUpgrade DA_Upgrade_Reliquary")
		C1+=("Hollow.Dev.RunAfter 22 Hollow.Profile.Equip DA_Item_Relic_KineticBrace 1")   # a second relic slot
		C1+=("Hollow.Dev.RunAfter 22.5 Hollow.Shop.Insure 4")                       # the ring (the welder is the other one)
		C1+=("Hollow.Dev.RunAfter 23 Hollow.Shop.Dump")                             # (no Profile.Dump before the end: it is the END marker)
		h "Hollow.Dev.RunAfter 45 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"; h "Hollow.Dev.RunInRaid 2.2 Hollow.Dev.Invulnerable All"
		C+=("Hollow.Dev.RunInRaid 2.5 Hollow.Inventory.Dump"); C+=("Hollow.Dev.RunInRaid 3 Hollow.Gear.Dump"); h "Hollow.Dev.RunInRaid 3 Hollow.Gear.Dump"
		# The client carries the core (braced), lets go; the host carries it (no brace): both say how fast they may go.
		h "Hollow.Dev.RunInRaid 4 Hollow.Dev.GoToNearestCore Others"
		C1+=("Hollow.Dev.RunInRaid 5 Hollow.Dev.FaceCore"); C1+=("Hollow.Dev.RunInRaid 5.5 Hollow.Interact")
		C1+=("Hollow.Dev.RunInRaid 7.5 Hollow.Gear.Dump"); h "Hollow.Dev.RunInRaid 7.5 Hollow.Carry.Dump"
		C1+=("Hollow.Dev.RunInRaid 9 Hollow.Interact")                             # puts it down
		h "Hollow.Dev.RunInRaid 11 Hollow.Dev.GoToNearestCore"
		h "Hollow.Dev.RunInRaid 12 Hollow.Interact"
		h "Hollow.Dev.RunInRaid 14 Hollow.Gear.Dump"
		h "Hollow.Dev.RunInRaid 16 Hollow.Dev.Kill Others"                          # the client dies: only insured slots come home
		h "Hollow.Dev.RunInRaid 18 Hollow.Dev.GoToExtraction"
		h "Hollow.Dev.RunInRaid 60 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 61 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	cathedral)
		HOST_ARGS=("-ini:Game:[/Script/HollowGame.HollowSectorSettings]:RaidSector=HollowSector_Cathedral")   # the client learns it from the manifest
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		h "Hollow.Dev.RunAfter 40 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 1 Hollow.Dev.Invulnerable All"
		h "Hollow.Dev.RunInRaid 2 Hollow.Sector.Info"; C+=("Hollow.Dev.RunInRaid 2.5 Hollow.Sector.Info")
		h "Hollow.Dev.RunInRaid 2.2 Hollow.Sector.Status HollowSector_Cathedral"; C+=("Hollow.Dev.RunInRaid 2.7 Hollow.Sector.Status HollowSector_Cathedral")
		h "Hollow.Dev.RunInRaid 2.4 Hollow.Sector.Status HollowSector_Docks"
		h "Hollow.Dev.RunInRaid 3 Hollow.Nav.Path Standard"; h "Hollow.Dev.RunInRaid 3.5 Hollow.Nav.Path Heavy"
		h "Hollow.Dev.RunInRaid 4 Hollow.AI.Dump"; C+=("Hollow.Dev.RunInRaid 4.5 Hollow.AI.Dump")
		h "Hollow.Dev.RunInRaid 22 Hollow.AI.Dump"; C+=("Hollow.Dev.RunInRaid 22.5 Hollow.AI.Dump")   # 20 s on: the AI in the halls
		h "Hollow.Dev.RunInRaid 24 Hollow.AI.Kill"
		# The host walks 4 rooms with real input (piers, plinths, openings) and takes pictures on the way; no lights
		# before S7-03, so the lighting show flag goes off (a console variable: viewmode does not reach the viewport, H-5).
		h "Hollow.Dev.RunInRaid 24.5 Hollow.Dev.OpenDoors"; h "Hollow.Dev.RunInRaid 25 ShowFlag.Lighting 0"
		h "Hollow.Dev.RunInRaid 25.5 Hollow.Dev.WalkRoute 4"
		h "Hollow.Dev.RunInRaid 27 Hollow.UI.Shot Cathedral_Walk1"; h "Hollow.Dev.RunInRaid 28.5 Hollow.UI.Shot Cathedral_Walk2"
		h "Hollow.Dev.RunInRaid 30 Hollow.UI.Shot Cathedral_Walk3"; h "Hollow.Dev.RunInRaid 34 Hollow.UI.Shot Cathedral_Walk4"   # 4 rooms take ~6 s
		h "Hollow.Dev.RunInRaid 46 Hollow.Dev.GoToNearestCore"; h "Hollow.Dev.RunInRaid 46.5 Hollow.UI.Shot Cathedral_Crypt"
		h "Hollow.Dev.RunInRaid 47 Hollow.Interact"
		h "Hollow.Dev.RunInRaid 49 Hollow.Dev.GoToExtraction All"; h "Hollow.Dev.RunInRaid 50 Hollow.UI.Shot Cathedral_Belfry"
		h "Hollow.Dev.RunInRaid 90 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 91 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	altar)
		HOST_ARGS=("-ini:Game:[/Script/HollowGame.HollowSectorSettings]:RaidSector=HollowSector_Cathedral")   # the client learns it from the manifest
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		h "Hollow.Dev.RunAfter 40 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 1 Hollow.Dev.Invulnerable All"
		h "Hollow.Dev.RunInRaid 2 Hollow.Altar.Dump"; C+=("Hollow.Dev.RunInRaid 2.5 Hollow.Altar.Dump")
		h "Hollow.Dev.RunInRaid 2.2 Hollow.Arc.Dump"
		# 1. The client lifts the core, is put before the altar and lays it there: the ritual, the bell, the AI.
		h "Hollow.Dev.RunInRaid 3 Hollow.Dev.GoToNearestCore Others"
		C1+=("Hollow.Dev.RunInRaid 3.5 Hollow.Dev.FaceCore"); C1+=("Hollow.Dev.RunInRaid 4 Hollow.Interact")
		h "Hollow.Dev.RunInRaid 6 Hollow.Dev.GoToAltar Others"
		C1+=("Hollow.Dev.RunInRaid 7.5 Hollow.Interact")                            # the altar in focus: it takes the core
		h "Hollow.Dev.RunInRaid 9 Hollow.Altar.Dump"; C+=("Hollow.Dev.RunInRaid 9.5 Hollow.Altar.Dump")
		h "Hollow.Dev.RunInRaid 9.2 Hollow.AI.Heat"; h "Hollow.Dev.RunInRaid 9.4 Hollow.AI.Dump"
		h "Hollow.Dev.RunInRaid 18 Hollow.Altar.Dump"; h "Hollow.Dev.RunInRaid 18.2 Hollow.AI.Dump"; h "Hollow.Dev.RunInRaid 18.4 Hollow.AI.Heat"
		h "Hollow.Dev.RunInRaid 30 Hollow.Altar.Dump"; C+=("Hollow.Dev.RunInRaid 30.5 Hollow.Altar.Dump")   # done: purified
		h "Hollow.Dev.RunInRaid 31 Hollow.AI.Kill"
		# 2. The client in an arc, vulnerable: struck; the host cuts its breaker: no more strikes.
		h "Hollow.Dev.RunInRaid 32 Hollow.Dev.GoToArc Others"; h "Hollow.Dev.RunInRaid 32.5 Hollow.Dev.Invulnerable All Off"
		h "Hollow.Dev.RunInRaid 33 Hollow.Arc.Dump"
		h "Hollow.Dev.RunInRaid 40 Hollow.Arc.Dump"; C+=("Hollow.Dev.RunInRaid 40.5 Hollow.Arc.Dump")   # a cycle on: struck
		h "Hollow.Dev.RunInRaid 41 Hollow.Dev.GoToArc Breaker"; h "Hollow.Dev.RunInRaid 42 Hollow.Interact"
		h "Hollow.Dev.RunInRaid 43 Hollow.Arc.Dump"
		h "Hollow.Dev.RunInRaid 56 Hollow.Arc.Dump"; C+=("Hollow.Dev.RunInRaid 56.5 Hollow.Arc.Dump")   # two cycles later: no strike
		# 3. The client carries the purified core out.
		h "Hollow.Dev.RunInRaid 57 Hollow.Dev.Invulnerable All"
		h "Hollow.Dev.RunInRaid 58 Hollow.Dev.GoToNearestCore Others"
		C1+=("Hollow.Dev.RunInRaid 58.5 Hollow.Dev.FaceCore"); C1+=("Hollow.Dev.RunInRaid 59 Hollow.Interact")
		h "Hollow.Dev.RunInRaid 61 Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid 100 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 101 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	screens)
		h "$(key 6 $DOWN)"; h "$(key 7 $PRESS)"                                       # main menu: Host co-op -> Play solo
		h "Hollow.Dev.RunAfter 16 Hollow.Dev.GoToTerminal Workbench"
		h "Hollow.Dev.RunAfter 17 Hollow.Interact"                                  # E: the server opens it, the screen shows
		h "Hollow.Dev.RunAfter 18 Hollow.UI.Dump"; h "Hollow.Dev.RunAfter 18.5 Hollow.UI.Shot Screens_Craft"
		h "$(key 19 $PRESS)"; h "Hollow.Dev.RunAfter 20 Hollow.UI.Dump"               # A: craft a fuel cell
		h "$(key 21 Gamepad_RightShoulder)"; h "Hollow.Dev.RunAfter 22 Hollow.UI.Dump"   # RB: the shop
		h "Hollow.Dev.RunAfter 22.5 Hollow.UI.Shot Screens_Shop"
		h "$(key 23 $DOWN)"; h "$(key 24 $PRESS)"; h "Hollow.Dev.RunAfter 25 Hollow.UI.Dump"   # D-pad, A: buy a plasma cutter
		h "$(key 26 E)"; h "Hollow.Dev.RunAfter 27 Hollow.UI.Dump"                    # E (keyboard): ship upgrades
		h "Hollow.Dev.RunAfter 27.5 Hollow.UI.Shot Screens_Upgrades"
		h "$(key 28 Q)"; h "Hollow.Dev.RunAfter 29 Hollow.UI.Dump"                    # Q: back to the shop
		h "$(key 30 Escape)"; h "Hollow.Dev.RunAfter 31 Hollow.UI.Dump"               # Esc: back to the game
		h "Hollow.Dev.RunAfter 32 Hollow.Dev.GoToTerminal Locker"
		h "Hollow.Dev.RunAfter 33 Hollow.Interact"
		h "Hollow.Dev.RunAfter 34 Hollow.UI.Dump"; h "Hollow.Dev.RunAfter 34.5 Hollow.UI.Shot Screens_Locker"
		h "$(key 35 Down)"; h "$(key 36 $PRESS)"; h "Hollow.Dev.RunAfter 37 Hollow.UI.Dump"   # arrow, A: take the cutter
		h "$(key 38 Up)"; h "$(key 38.5 Right)"; h "$(key 39 Right)"; h "$(key 39.5 Right)"   # over to the loadout's Insure
		h "Hollow.Dev.RunAfter 40 Hollow.UI.Dump"; h "$(key 41 $PRESS)"; h "Hollow.Dev.RunAfter 42 Hollow.UI.Dump"
		h "Hollow.Dev.RunAfter 42.5 Hollow.UI.Shot Screens_Locker_Insured"
		h "$(key 43 Gamepad_FaceButton_Right)"; h "Hollow.Dev.RunAfter 44 Hollow.UI.Dump"   # B: back to the game
		h "Hollow.Dev.RunAfter 45 Hollow.Dev.GoToTerminal Contracts"
		h "Hollow.Dev.RunAfter 46 Hollow.Interact"
		h "Hollow.Dev.RunAfter 47 Hollow.UI.Dump"; h "Hollow.Dev.RunAfter 47.5 Hollow.UI.Shot Screens_Contracts"
		h "Hollow.Dev.RunAfter 48 Hollow.Profile.Dump"
		h "$(key 49 $PRESS)"                                                          # A on Launch
		h "Hollow.Dev.RunInRaid 1 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 3 Hollow.Dev.GoToNearestCore"
		h "Hollow.Dev.RunInRaid 4 Hollow.Interact"
		h "Hollow.Dev.RunInRaid 6 Hollow.Dev.GoToExtraction"
		h "Hollow.Dev.RunInRaid 45 Hollow.Profile.Dump"
		END='Hollow[.]Profile[.]Dump: [0-9].*raids 1,'
		;;
	economy)
		h "$(key 6 $DOWN)"; h "$(key 7 $PRESS)"                                       # main menu: Host co-op -> Play solo
		h "Hollow.Dev.RunAfter 10 Hollow.Profile.Dump"
		h "Hollow.Dev.RunAfter 10.5 Hollow.Shop.Dump"
		h "Hollow.Dev.RunAfter 11 Hollow.Craft.Make DA_Recipe_PulseMine 2"           # one shard in the stash: refused, nothing taken
		h "Hollow.Dev.RunAfter 11.5 Hollow.Shop.Buy DA_Item_AetherShard 1"
		h "Hollow.Dev.RunAfter 12 Hollow.Craft.Make DA_Recipe_PulseMine 2"           # 6 scrap, 2 shards, 20 cr
		h "Hollow.Dev.RunAfter 12.5 Hollow.Shop.Buy DA_Item_Scrap 2"
		h "Hollow.Dev.RunAfter 13 Hollow.Craft.Make DA_Recipe_FuelCell 1"
		h "Hollow.Dev.RunAfter 13.5 Hollow.Profile.Equip DA_Item_PulseMine 2"        # onto the 2 mines: a full slot and a new one
		h "Hollow.Dev.RunAfter 14 Hollow.Shop.Insure 1"                              # the cutter (the welder came insured)
		h "Hollow.Dev.RunAfter 14.5 Hollow.Shop.Sell DA_Item_FuelCell 1"
		h "Hollow.Dev.RunAfter 15 Hollow.Craft.Dump"
		h "Hollow.Dev.RunAfter 15.5 Hollow.Profile.Dump"
		h "$(key 18 Escape)"; h "$(key 19 $DOWN)"; h "$(key 20 $PRESS)"               # system menu: Resume -> Start raid
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 3 Hollow.Inventory.Dump"
		h "Hollow.Dev.RunInRaid 4 Hollow.Shop.Buy DA_Item_Scrap 1"                   # the loadout is in escrow: refused
		h "Hollow.Dev.RunInRaid 5 Hollow.Dev.Kill"
		h "Hollow.Dev.RunInRaid 40 Hollow.Profile.Dump"                              # back in the hub after the debrief
		h "Hollow.Dev.RunInRaid 41 Hollow.Shop.Dump"
		h "Hollow.Dev.RunInRaid 42 Hollow.UI.Dump"
		END='Hollow[.]UI[.]Dump: layer'
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

if [[ "$MODE" == solo || "$MODE" == economy || "$MODE" == screens ]]; then
	launch Host "${VIEW[@]}" -ExecCmds="$(join "${H[@]}")"
	wait_for "$OUT/Host.log" "$END" 400 || echo "the $MODE playthrough did not finish"
else
	launch Host "${VIEW[@]}" ${HOST_ARGS[@]+"${HOST_ARGS[@]}"} -ExecCmds="$(join "${H[@]}")"
	wait_for "$OUT/Host.log" 'listening on port' 240 || { echo "the host never listened"; exit 1; }
	CLIENTS=3
	[[ "$MODE" == carry* || "$MODE" == loadout || "$MODE" == hub || "$MODE" == classes || "$MODE" == lantern || "$MODE" == ward || "$MODE" == relics || "$MODE" == cathedral || "$MODE" == altar ]] && CLIENTS=1
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
	grep -hE "UI up for|Session: State|Raid .*: .* -> |result (Extracted|KilledInAction|MissingInAction|SignalLost|Abandoned)|every hash matches|debrief|Hollow.Profile.Dump: [0-9]|Carry.Dump: |Hollow.Perf: Soak|Hollow.UI.Dump: layer|Hollow.Dev.RunAfter: Hollow.UI|Voice: .*(Shout|Speech|Whisper)|Hollow.Voice.Dump|Hollow.Lamp.Dump|helmet lamp|Hollow.UI.Dump: (glass|resonance|voice|lamp)|WalkRoute: |FollowCarry: |OpenDoors: |carrier corrected|Client: Error for|client-authoritative|server-authoritative again|lets go|joins .* \\(Coop|tether breaks|sanity check|its moves show|lifted by|dropped by|ForcePositionUpdate|Inventory.Dump: |takes |uses [0-9]|lays |refuels|asks .* for the loadout|loadout of|Profile: loadout|Hollow.UI.Dump: supplies|Profile.SetSlot|Hollow.Shop.(Buy|Sell|Insure|Dump): |Hollow.Craft.(Make|Dump): |Hollow.Profile.(Equip|Unequip): |resolved as|Hub: |Hollow.Hub.Dump: |GoToTerminal: |Hollow.UI.Shot: |Shop.BuyUpgrade: |Raid: travelling|contract |Hollow.UI.Dump: (hub|input|class)|_C_[0-9]+: |shows W_|Hollow.Class.(Dump|Select): | plays DA_Class|Class: |Aether light|Lantern light|Exposed glow|speed x[0-9]|Hollow.AI.Dump: (enemy|brain)|Hollow.AI.Spawn: |Hollow.Resonance.Dump: player|Hollow.Tool.Info: |FaceNearestEnemy: |GoToNearestEnemy| burns at | goes out |: inside [0-9]|Hollow.Gadget.Dump: |lays BP_|takes back|Hollow.Gear.Dump: |Hollow.Profile.Credits: |cut down|Hollow.Sector.(Info|Status): |Hollow.Nav.Path [A-Za-z]+: |Hollow.AI.Dump: |Hollow.(Altar|Arc).Dump: |lays .* on the altar|ritual is over|: toll [0-9]|strikes |power (on|off)|turned (on|off) by|counts as |held by a fixture|free to lift|Hollow.AI.Heat: " "$log" | grep -v "Command Line\|commandline" | strip | awk 'NR <= 160'
done
if [[ "$MODE" == lantern ]]; then
	# Every player's Resonance in the host's four dumps (by name and pawn: local processes share a name): the carrier
	# gains, without the aura (1 -> 2) and in it (3 -> 4).
	echo "===== Resonance gained, without the lantern's aura and in it (host's dumps)"
	{ grep -hE "Hollow.Resonance.Dump: player" "$OUT/Host.log" || true; } | strip | awk '{ for (i = 1; i < NF; i++) if ($i == "player") { v[$(i + 1) " " $(i + 2)] = v[$(i + 1) " " $(i + 2)] " " $(i + 3) } }
		END { for (p in v) { n = split(v[p], a, " "); if (n >= 4) printf "%s: without +%.1f, in the aura +%.1f (x%.2f)\n", p, a[2] - a[1], a[4] - a[3], (a[2] > a[1]) ? (a[4] - a[3]) / (a[2] - a[1]) : 0 } }'
fi
echo "===== errors, ensures, crashes"
{ grep -hE "(LogHollow[A-Za-z]*|LogNet|LogAbilitySystem|LogOnline|LogGameMode|LogLoad|LogStreaming|LogScript): (Error|Warning)|Ensure condition failed|Assertion failed|Fatal error|appError" "$OUT"/*.log || true; } | strip | sort | uniq -c | sort -rn | head -30   # none is no failure
echo "Logs: $OUT"
