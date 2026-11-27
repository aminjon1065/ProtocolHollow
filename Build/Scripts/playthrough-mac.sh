#!/usr/bin/env bash
# Bug-bash playthroughs of a packaged build on this Mac (S4-06, ADR-0024), Null online subsystem, local IP. The menus
# are driven with keys (Hollow.UI.PressKey), the way a player does: Esc opens the system menu, the d-pad moves the focus,
# A (Gamepad_FaceButton_Bottom) presses the button (a synthetic keyboard Enter does not reach CommonUI's click action).
#   solo   E2: main menu "Play solo" -> hub -> system menu "Start raid" -> lift the core -> extract -> debrief -> hub ->
#          "Leave session" -> main menu. DRONE-01 (ADR-0054): the solo drone is out (dump), takes the lifted core
#          (Hollow.Dev.FaceDrone, Interact; dump: Drone mode, the drone pace; shot Drone_Carry) and the extraction counts it.
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
#   cathedral  S7-01 Reactor Cathedral (ADR-0038): host + 1 headless client behind --net; the host picks the Cathedral's
#          contract in the hub (it picks the sector, ADR-0041). Both machines dump the sector
#          (same layout hash, the plugin Active), the host its generated map and the Standard and Heavy paths to the
#          extraction; the AI lives for 20 s (both dump it); both dump the rooms' light against DA_LightBudget and the
#          sounds heard (S7-03: rigs, reactor hum, reverbs); the host walks 4 rooms with real input taking lit pictures,
#          then carries the core out and both extract.
#   altar  S7-02 Cathedral mechanics (ADR-0039): host + 1 headless client behind --net, the host picks the Cathedral's
#          contract in the hub. The client carries the core to the Purification Altar and lays it there (Interact on a carry
#          receiver); both dump the altar through the 20 s ritual, the AI and the heat (the bell draws them); the core
#          comes off purified. The client stands in an electric arc, vulnerable: struck; the host cuts its breaker: no more
#          strikes. The client carries the purified core out: both extract for 1250 credits.
#   biomes  S7-04 both sectors in one session (ADR-0041), the Phase 2 co-op regression: host (Engineer) + 1 headless
#          client behind --net. Hub: the client turns Techno-Occultist, crafts and equips the Ring of Silence and watches
#          the contracts screen (the Docks, their sector and reward); both machines dump both sectors (Registered, no
#          reference). Raid 1 in the Docks: same layout hash, the Docks Active and the Cathedral not; the host carries the
#          core out. Hub: after a garbage collection nothing of either sector is loaded on either machine, the contract is
#          still the Docks; the host picks the Cathedral at its terminal with the gamepad while the client's open contracts
#          screen follows live, and launches from it. Raid 2 in the Cathedral: same hash, the Cathedral Active and the Docks
#          released; the Occultist lays the core on the altar (the ritual, purified), the host cuts an arc's breaker, the
#          client carries the purified core out. Hub: both sectors released and unloaded again, the contract kept; profiles.
#   guide  UX-01 objectives, waypoints and controls (ADR-0042): host + 1 headless client behind --net. Hub: both dump the
#          objective (the workbench or the locker first; the host launches, the guest waits) and the three terminals'
#          waypoints, which step aside under an open screen; the host folds and unfolds the cheat sheet with H, the client
#          with the gamepad's View (its keys turn gamepad glyphs). Raid in the Cathedral: find the core (vault and
#          extraction marked) -> the client lifts it: purify it or carry it (the prices) -> the ritual -> deliver the
#          purified core -> hold the zone (the countdown); the host takes a picture of its visor at every step.
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
#   weapons  GP-01 weapons (ADR-0044): host + 1 headless client behind --net. In the hub the host trades its nailgun for a
#          shotgun and shells; in the raid the client takes its nailgun in hand (the hands' key), shoots a Pilgrim the host
#          spawns six times, reloads with R and strikes; the host blasts it twice with the shotgun (staggered). Both dump
#          the hands, magazines and rounds (the host: the client's hits it took and refused) and the Pilgrim; the client
#          puts the weapon away and cuts. Both extract. GP-01c (ADR-0046): both dump the game feel they felt (their own
#          shots, swaps and blow, the Pilgrim's steps) and the hit markers they saw.
#   loot   GP-01b loot crates (ADR-0045): host + 1 headless client behind --net. In the Docks both machines dump the sector's
#          crates (the host: their seeds and contents); the client shoots one open with its nailgun and picks its loot up
#          (Interact on each pickup), the host breaks another with blows and picks it up, then dies (its found loot is left
#          behind); the client extracts: its found loot is in its stash, the debrief says so.
#   swarm  GP-02 Whisperers (ADR-0047): host + 1 headless client behind --net. Both dump the sector's own nests (the host:
#          the swarm), then the host puts a nest (seed 15: four Whisperers, two of whom drop a shard) in front of itself and
#          the client among them, its lamp on them: they flinch (lit, shrieks on both machines) and back off. Both put their
#          lamps out: the frenzy (both machines). Lamps on again, the client shoots them down with its nailgun; both dump the
#          swarm, the enemies, the shots (the host: the hits it took), the feel and the drops. Both extract.
#   loc    LOC-01 English + Russian (ADR-0048): host (-culture=ru, this run only) + 1 headless client in English. The host
#          shoots every screen in Russian (Hollow.UI.Shot Loc_*): main menu, hub with its objective, waypoints and cheat
#          sheet, the workbench's three tabs, the locker, the system menu (switched to English and back at once with
#          Hollow.UI.Language, no restart), the contracts; in the raid the visor, the core's prompt, the visor in English
#          again for a moment, the extraction and the debrief. Both machines dump their UI: each in its own language.
#   story  NAR-01a handler's radio (ADR-0049): host (-culture=ru, this run only) + 1 headless client in English, both with
#          the radio forgotten (Hollow.Radio.Reset). The hub briefing plays on arrival on each machine in its own language
#          (shots in Russian, in English for a moment, Russian again); the first sortie into the Docks, the core seen and
#          lifted, the Echo-Stalker's mimicry and a critical Resonance (forced: the AI is killed), the client's death (the
#          host hears the loss), the extraction's countdown. Both machines dump their radio (Story_* shots on the host).
#          NAR-01b (ADR-0050): both archives forgotten too (Hollow.Lore.Reset); the host opens the empty crew archive in the
#          hub, downloads both datapads of the Docks before the core (the visor's notice; the client gets each log too),
#          extracts with the bonus while the client dies (its logs stay), then reads the Codex back in the hub, in Russian
#          and in English. Both machines dump their archive (Hollow.Lore.Dump) and their profile.
#   fab    FAB-01 Fab props (ADR-0051): host + 1 headless client. The host views the hub's Fab props from fixed spots
#          (Hollow.Dev.GoToRoom World), then in a Docks raid (AI removed) the CoreVault's pump station, the plain core's cyan
#          aura, the airlock, the hangar, the two corridors and a junction (Fab_* shots; the raid's seed decides which rooms
#          it has: a missing one is a warning); both dump the lights (DA_LightBudget kept).
#   fabaltar  FAB-01 Niagara (ADR-0051): host + 1 headless client, Cathedral. The client carries the core (its cyan aura
#          seen by the host), the client lays it on the altar, the host stands beside it: the violet ritual, the gold finale,
#          the purified core's gold glow (FabAltar_* shots); both dump the altar.
#   ui     UI-02 menus and settings (ADR-0052): host (-culture=ru, this run only) + 1 headless client. The host shoots every
#          screen in Russian, then in English at once (Hollow.UI.Language, no restart): the live main menu, the settings'
#          four tabs (E: the next one), in the hub the visor, the workbench, the locker, the contracts, the Codex, the system
#          menu and the settings from it; in the raid the visor (UI_* shots). Both dump their UI and their profile.
#   alert  POLISH-01 threat readability (ADR-0053): host (-culture=ru, this run only) + 1 headless client. In a Docks raid
#          (the sector's AI removed, everyone invulnerable) a Pilgrim behind a closed door wakes to the host's shout: it
#          calls out (both machines log its acoustic alert), the host's visor says ACOUSTIC ALERT (shot), the client's not;
#          a fresh one wakes to the client's shout: the client's visor only; in English the host's shotgun wakes another,
#          and one more at once calls out with the host's visor quiet (its cooldown); the Echo-Stalker the director brings
#          hears a shotgun blast. Then immersive waypoints: hidden in the raid, shown 5 s on H (Alert_* shots, RU and EN).
#   chars  CHAR-01 characters (ADR-0055): host + 1 headless client, Docks raid of a fixed sector (--seed, default
#          1624260639: the one the A/B baseline was measured in; the sector's AI removed, everyone invulnerable). The host is shown each body without its AI (Hollow.Art.Showcase): the ExoFrame in the airlock, the
#          Echo-Stalker in the core vault, its own first-person hands and then the Iron Pilgrim in the hangar, then all three
#          there (CHAR01_* shots); with the three in view it records 20 s of frames at Low (Hollow.Perf.Capture CHAR01_Group:
#          the same scene before and after an art change is its FPS A/B), then everyone extracts.
# Usage: Build/Scripts/playthrough-mac.sh <solo|coop|soak|carry|carrysolo|loadout|economy|hub|screens|classes|lantern|ward|relics|cathedral|altar|biomes|guide|weapons|loot|swarm|loc|story|fab|fabaltar|ui|alert|chars> [--app PATH] [--minutes N] [--seed N] [--net 150|bad]
#   --app  the packaged game (default Packages/Mac/Development/ProtocolHollow.app, Build/Scripts/package-mac.sh)
#   --net  the clients' network: 0 = none, 150 = 150 ms (-PktLag=150, the carry and loadout modes' default), bad = Network Emulation
#          Bad (100-200 ms each way, 5 % loss)
#   --client-fps  frame limit of the headless clients (default 30)
set -euo pipefail
source "$(dirname "$0")/hollow-common.sh"

MODE="${1:-}"
[[ "$MODE" == solo || "$MODE" == coop || "$MODE" == soak || "$MODE" == carry || "$MODE" == carrysolo || "$MODE" == loadout || "$MODE" == economy || "$MODE" == hub || "$MODE" == screens || "$MODE" == classes || "$MODE" == lantern || "$MODE" == ward || "$MODE" == relics || "$MODE" == cathedral || "$MODE" == altar || "$MODE" == biomes || "$MODE" == guide || "$MODE" == weapons || "$MODE" == loot || "$MODE" == swarm || "$MODE" == loc || "$MODE" == story || "$MODE" == fab || "$MODE" == fabaltar || "$MODE" == ui || "$MODE" == alert || "$MODE" == chars ]] || { sed -n '2,140p' "$0"; exit 2; }
shift
APP="$HOLLOW_PROJECT_ROOT/Packages/Mac/Development/ProtocolHollow.app"
MINUTES=20
SEED=        # the raid's sector seed where a mode pins it (chars); empty: the mode's own
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
if [[ ("$MODE" == carry* || "$MODE" == loadout || "$MODE" == hub || "$MODE" == classes || "$MODE" == lantern || "$MODE" == ward || "$MODE" == relics || "$MODE" == cathedral || "$MODE" == altar || "$MODE" == biomes || "$MODE" == weapons || "$MODE" == loot || "$MODE" == swarm) && "$NET" == none ]]; then
	NET_ARGS=(-PktLag=150)   # the co-op smokes but coop and soak always run behind a slow network
fi
GAME="$APP/Contents/MacOS/ProtocolHollow"
[[ -x "$GAME" ]] || { echo "No packaged game at $APP (Build/Scripts/package-mac.sh)" >&2; exit 2; }

NET_SUFFIX=""
[[ "$NET" != none ]] && NET_SUFFIX="-$NET"
OUT="$HOLLOW_PROJECT_ROOT/Saved/Playthrough/$(date +%Y%m%d-%H%M%S)-$MODE$NET_SUFFIX"
mkdir -p "$OUT"
# -forcelogflush: every log line reaches the file at once, so an engine check's text survives the crash (KnownIssues H-7).
COMMON=(-nosplash -unattended -HollowPreset=Low -forcelogflush
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
UP=Gamepad_DPad_Up
PRESS=Gamepad_FaceButton_Bottom

H=()   # host commands
HOST_ARGS=()   # extra arguments of the host
C=()   # commands of every client
C1=()  # commands of client 1 only
CLIENT_OPEN="open 127.0.0.1"   # how the clients join (a mode whose host hosts late waits first)
h() { H+=("$1"); }
case "$MODE" in
	solo)
		h "$(key 6 $UP)"; h "$(key 7 $PRESS)"                                         # main menu: Host co-op -> Play solo (above it, UI-02)
		h "$(key 16 Escape)"; h "$(key 17 $DOWN)"; h "$(key 18 $PRESS)"               # system menu: Resume -> Start raid
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 2.5 Hollow.Drone.Dump"                               # DRONE-01: out for a solo raid, lit, humming
		h "Hollow.Dev.RunInRaid 3 Hollow.Dev.GoToNearestCore"
		h "Hollow.Dev.RunInRaid 4 Hollow.Interact"
		h "Hollow.Dev.RunInRaid 4.6 Hollow.Dev.FaceDrone"
		h "Hollow.Dev.RunInRaid 5.2 Hollow.Interact"                                 # ...hand the core to the drone
		h "Hollow.Dev.RunInRaid 5.8 Hollow.Drone.Dump"                               # Drone mode, the drone pace
		h "Hollow.Dev.RunInRaid 6 Hollow.UI.Shot Drone_Carry"
		h "Hollow.Dev.RunInRaid 6.5 Hollow.Dev.GoToExtraction"
		h "Hollow.Dev.RunInRaid 45 Hollow.Profile.Dump"                              # back in the hub after the debrief
		h "Hollow.Dev.RunInRaid 46 Hollow.UI.PressKey Escape"                        # the debrief, unread, shows again in the hub
		h "Hollow.Dev.RunInRaid 47 Hollow.UI.PressKey Escape"                        # system menu
		h "Hollow.Dev.RunInRaid 48 Hollow.UI.PressKey $DOWN"
		h "Hollow.Dev.RunInRaid 49 Hollow.UI.PressKey $DOWN"
		h "Hollow.Dev.RunInRaid 49.5 Hollow.UI.PressKey $DOWN"
		h "Hollow.Dev.RunInRaid 50 Hollow.UI.PressKey $PRESS"                        # Resume, Start raid, Settings -> Leave session
		h "Hollow.Dev.RunInRaid 57 Hollow.Session.Status"
		h "Hollow.Dev.RunInRaid 58 Hollow.UI.Dump"                                   # the main menu again
		END='Hollow[.]UI[.]Dump: layer'   # an output line (the command line in the log names the command too)
		;;
	coop)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op
		h "$(key 30 Escape)"; h "$(key 31 $DOWN)"; h "$(key 32 $DOWN)"; h "$(key 33 $PRESS)"   # Resume, Invite -> Start raid
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 2.5 Hollow.Drone.Dump"                               # DRONE-01: no drone in co-op
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
		# Host time; the client starts some 10 s later (after the host's menu) and counts from its own start.
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
		# The launch waits for the client's last hub step (46 on its clock, up to some 15 s behind the host's; S7-04 run: 12).
		h "Hollow.Dev.RunAfter 62 Hollow.Hub.Dump"
		h "Hollow.Dev.RunAfter 65 Hollow.UI.Dump"
		h "$(key 66 $PRESS)"                                                          # A on Launch: the raid
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
	weapons)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		# The host (an Engineer: welder 0, cutter 1, cells 2, mines 3, nailgun 4, nails 5) trades the nailgun for a shotgun.
		h "Hollow.Dev.RunAfter 12 Hollow.Profile.Credits 2000"
		h "Hollow.Dev.RunAfter 12.5 Hollow.Shop.Buy DA_Item_Weapon_Shotgun 1"; h "Hollow.Dev.RunAfter 13 Hollow.Shop.Buy DA_Item_Ammo_Shells 10"
		h "Hollow.Dev.RunAfter 13.5 Hollow.Profile.Unequip 5"; h "Hollow.Dev.RunAfter 14 Hollow.Profile.Unequip 4"
		h "Hollow.Dev.RunAfter 14.5 Hollow.Profile.Equip DA_Item_Weapon_Shotgun 1"; h "Hollow.Dev.RunAfter 15 Hollow.Profile.Equip DA_Item_Ammo_Shells 10"
		h "Hollow.Dev.RunAfter 15.5 Hollow.Shop.Dump"
		h "Hollow.Dev.RunAfter 40 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"; h "Hollow.Dev.RunInRaid 2.2 Hollow.Dev.Invulnerable All"
		h "Hollow.Dev.RunInRaid 2.5 Hollow.Inventory.Dump"; C+=("Hollow.Dev.RunInRaid 2.5 Hollow.Inventory.Dump")
		# 1. The client's nailgun: six shots at a Pilgrim the host spawns, a reload (R), a blow.
		h "Hollow.Dev.RunInRaid 3 Hollow.AI.Spawn 600"
		h "Hollow.Dev.RunInRaid 4 Hollow.Dev.GoToNearestEnemy Others"
		h "Hollow.Dev.RunInRaid 5 Hollow.AI.Dump"                                    # its health before
		C1+=("Hollow.Dev.RunInRaid 6 Hollow.Tool.Hold Hollow.Input.Hands.Weapon 0.05")
		C1+=("Hollow.Dev.RunInRaid 6.6 Hollow.Weapon.Dump")
		for ((s = 0; s < 6; s++)); do
			C1+=("Hollow.Dev.RunInRaid $((7 + s)).0 Hollow.Dev.FaceNearestEnemy"); C1+=("Hollow.Dev.RunInRaid $((7 + s)).3 Hollow.Tool.Hold Hollow.Input.PrimaryUse 0.05")
		done
		C1+=("Hollow.Dev.RunInRaid 13.5 Hollow.Weapon.Dump"); h "Hollow.Dev.RunInRaid 14 Hollow.Weapon.Dump"; h "Hollow.Dev.RunInRaid 14 Hollow.AI.Dump"
		C1+=("Hollow.Dev.RunInRaid 14.5 Hollow.Tool.Hold Hollow.Input.Reload 0.05")
		C1+=("Hollow.Dev.RunInRaid 15 Hollow.Weapon.Dump")                         # reloading: its rounds in at once
		C1+=("Hollow.Dev.RunInRaid 16.5 Hollow.Dev.FaceNearestEnemy"); C1+=("Hollow.Dev.RunInRaid 17 Hollow.Tool.Hold Hollow.Input.Melee 0.05")
		h "Hollow.Dev.RunInRaid 17.8 Hollow.AI.Dump"
		# 2. The host's shotgun: two blasts up close (each one staggers the Pilgrim).
		h "Hollow.Dev.RunInRaid 18 Hollow.Dev.GoToNearestEnemy"
		h "Hollow.Dev.RunInRaid 19 Hollow.Tool.Hold Hollow.Input.Hands.Weapon 0.05"
		h "Hollow.Dev.RunInRaid 19.6 Hollow.Dev.FaceNearestEnemy"; h "Hollow.Dev.RunInRaid 19.8 Hollow.Tool.Hold Hollow.Input.PrimaryUse 0.05"
		h "Hollow.Dev.RunInRaid 20 Hollow.AI.Dump"
		h "Hollow.Dev.RunInRaid 21.2 Hollow.Dev.FaceNearestEnemy"; h "Hollow.Dev.RunInRaid 21.4 Hollow.Tool.Hold Hollow.Input.PrimaryUse 0.05"
		h "Hollow.Dev.RunInRaid 21.6 Hollow.AI.Dump"; h "Hollow.Dev.RunInRaid 22 Hollow.Weapon.Dump"; C+=("Hollow.Dev.RunInRaid 22.5 Hollow.Weapon.Dump")
		h "Hollow.Dev.RunInRaid 22.2 Hollow.Feel.Dump"; C+=("Hollow.Dev.RunInRaid 22.7 Hollow.Feel.Dump")   # GP-01c
		# 3. The client puts the weapon away and cuts again.
		C1+=("Hollow.Dev.RunInRaid 23 Hollow.Tool.Hold Hollow.Input.Hands.Tools 0.05")
		C1+=("Hollow.Dev.RunInRaid 23.6 Hollow.Tool.Hold Hollow.Input.PrimaryUse 1"); C1+=("Hollow.Dev.RunInRaid 24 Hollow.Tool.Info")
		C1+=("Hollow.Dev.RunInRaid 24.8 Hollow.Weapon.Dump")
		h "Hollow.Dev.RunInRaid 25 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 26 Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid 70 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 71 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	loot)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		h "Hollow.Dev.RunAfter 40 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"; h "Hollow.Dev.RunInRaid 2.2 Hollow.Dev.Invulnerable All"
		h "Hollow.Dev.RunInRaid 3 Hollow.Loot.Dump"; C+=("Hollow.Dev.RunInRaid 3.5 Hollow.Loot.Dump")
		# 1. The client shoots a crate open and picks its loot up.
		h "Hollow.Dev.RunInRaid 4 Hollow.Dev.GoToNearestCrate Others"
		C1+=("Hollow.Dev.RunInRaid 5 Hollow.Tool.Hold Hollow.Input.Hands.Weapon 0.05")
		for ((s = 0; s < 5; s++)); do
			C1+=("Hollow.Dev.RunInRaid $((6 + s)).0 Hollow.Dev.FaceNearestCrate"); C1+=("Hollow.Dev.RunInRaid $((6 + s)).3 Hollow.Tool.Hold Hollow.Input.PrimaryUse 0.05")
		done
		h "Hollow.Dev.RunInRaid 11.5 Hollow.Loot.Dump"; C+=("Hollow.Dev.RunInRaid 12 Hollow.Loot.Dump")
		for ((p = 0; p < 3; p++)); do
			C1+=("Hollow.Dev.RunInRaid $((13 + 2 * p)).0 Hollow.Dev.FaceNearestPickup"); C1+=("Hollow.Dev.RunInRaid $((13 + 2 * p)).6 Hollow.Interact")
		done
		C1+=("Hollow.Dev.RunInRaid 19.5 Hollow.Inventory.Dump")
		# 2. The host breaks another with blows, takes its loot, dies.
		h "Hollow.Dev.RunInRaid 20 Hollow.Dev.GoToNearestCrate"
		for ((b = 0; b < 4; b++)); do
			h "Hollow.Dev.RunInRaid $((21 + b)).0 Hollow.Dev.FaceNearestCrate"; h "Hollow.Dev.RunInRaid $((21 + b)).3 Hollow.Tool.Hold Hollow.Input.Melee 0.05"
		done
		for ((p = 0; p < 3; p++)); do
			h "Hollow.Dev.RunInRaid $((26 + 2 * p)).0 Hollow.Dev.FaceNearestPickup"; h "Hollow.Dev.RunInRaid $((26 + 2 * p)).6 Hollow.Interact"
		done
		h "Hollow.Dev.RunInRaid 32 Hollow.Inventory.Dump"; h "Hollow.Dev.RunInRaid 32.5 Hollow.Loot.Dump"
		h "Hollow.Dev.RunInRaid 33 Hollow.Dev.Kill"                                   # the host's found loot is left behind
		h "Hollow.Dev.RunInRaid 35 Hollow.Dev.GoToExtraction Others"
		h "Hollow.Dev.RunInRaid 80 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 81 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	swarm)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		h "Hollow.Dev.RunAfter 40 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Swarm"; h "Hollow.Dev.RunInRaid 2.1 Hollow.AI.Dump"; C+=("Hollow.Dev.RunInRaid 2.2 Hollow.AI.Dump")   # the sector's own nests
		h "Hollow.Dev.RunInRaid 2.5 Hollow.AI.Kill"; h "Hollow.Dev.RunInRaid 2.7 Hollow.Dev.Invulnerable All"
		# 1. A nest in front of the host, the client among its brood, its lamp on them: they flinch and back off.
		h "Hollow.Dev.RunInRaid 3 Hollow.AI.Nest 700 15"
		h "Hollow.Dev.RunInRaid 4 Hollow.Dev.GoToNearestEnemy Others"
		C1+=("Hollow.Dev.RunInRaid 4.6 Hollow.Dev.FaceNearestEnemy")
		h "Hollow.Dev.RunInRaid 5.2 Hollow.AI.Swarm"; h "Hollow.Dev.RunInRaid 5.4 Hollow.AI.Dump"; C+=("Hollow.Dev.RunInRaid 5.6 Hollow.AI.Dump")
		# 2. Both lamps out: the dark, a frenzy.
		C1+=("Hollow.Dev.RunInRaid 6 Hollow.Lamp.Toggle"); h "Hollow.Dev.RunInRaid 6 Hollow.Lamp.Toggle"
		h "Hollow.Dev.RunInRaid 7.4 Hollow.AI.Swarm"; h "Hollow.Dev.RunInRaid 7.6 Hollow.AI.Dump"; C+=("Hollow.Dev.RunInRaid 7.8 Hollow.AI.Dump")
		# 3. The lamps on again, the client's nailgun: it shoots them down.
		C1+=("Hollow.Dev.RunInRaid 8.5 Hollow.Lamp.Toggle"); h "Hollow.Dev.RunInRaid 8.5 Hollow.Lamp.Toggle"
		C1+=("Hollow.Dev.RunInRaid 9 Hollow.Tool.Hold Hollow.Input.Hands.Weapon 0.05")
		for ((s = 0; s < 10; s++)); do
			C1+=("Hollow.Dev.RunInRaid $((10 + s)).0 Hollow.Dev.FaceNearestEnemy"); C1+=("Hollow.Dev.RunInRaid $((10 + s)).3 Hollow.Tool.Hold Hollow.Input.PrimaryUse 0.05")
		done
		h "Hollow.Dev.RunInRaid 20.5 Hollow.AI.Swarm"; h "Hollow.Dev.RunInRaid 20.7 Hollow.AI.Dump"; C+=("Hollow.Dev.RunInRaid 20.9 Hollow.AI.Dump")
		C1+=("Hollow.Dev.RunInRaid 21.2 Hollow.Weapon.Dump"); h "Hollow.Dev.RunInRaid 21.2 Hollow.Weapon.Dump"
		C+=("Hollow.Dev.RunInRaid 21.4 Hollow.Feel.Dump"); h "Hollow.Dev.RunInRaid 21.6 Hollow.Feel.Dump"
		h "Hollow.Dev.RunInRaid 22 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 22.6 Hollow.Loot.Dump"; C+=("Hollow.Dev.RunInRaid 22.8 Hollow.Loot.Dump")   # what they dropped
		h "Hollow.Dev.RunInRaid 23.5 Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid 70 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 71 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	cathedral)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		h "Hollow.Dev.RunAfter 36 Hollow.Hub.Select DA_Contract_Cathedral"           # the contract picks the sector; the client learns it from the manifest
		h "Hollow.Dev.RunAfter 37 Hollow.Hub.Dump"
		h "Hollow.Dev.RunAfter 40 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 1 Hollow.Dev.Invulnerable All"
		h "Hollow.Dev.RunInRaid 2 Hollow.Sector.Info"; C+=("Hollow.Dev.RunInRaid 2.5 Hollow.Sector.Info")
		h "Hollow.Dev.RunInRaid 2.2 Hollow.Sector.Status HollowSector_Cathedral"; C+=("Hollow.Dev.RunInRaid 2.7 Hollow.Sector.Status HollowSector_Cathedral")
		h "Hollow.Dev.RunInRaid 2.4 Hollow.Sector.Status HollowSector_Docks"
		h "Hollow.Dev.RunInRaid 3 Hollow.Nav.Path Standard"; h "Hollow.Dev.RunInRaid 3.5 Hollow.Nav.Path Heavy"
		h "Hollow.Dev.RunInRaid 4 Hollow.AI.Dump"; C+=("Hollow.Dev.RunInRaid 4.5 Hollow.AI.Dump")
		h "Hollow.Dev.RunInRaid 22 Hollow.AI.Dump"; C+=("Hollow.Dev.RunInRaid 22.5 Hollow.AI.Dump")   # 20 s on: the AI in the halls
		h "Hollow.Dev.RunInRaid 24 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 5 Hollow.Light.Dump"; C+=("Hollow.Dev.RunInRaid 5.5 Hollow.Light.Dump")   # S7-03: every rig within budget
		# The host walks 4 rooms with real input (piers, plinths, openings) and takes lit pictures on the way (S7-03).
		h "Hollow.Dev.RunInRaid 24.5 Hollow.Dev.OpenDoors"
		h "Hollow.Dev.RunInRaid 25.5 Hollow.Dev.WalkRoute 4"
		h "Hollow.Dev.RunInRaid 27 Hollow.UI.Shot Cathedral_Walk1"; h "Hollow.Dev.RunInRaid 28.5 Hollow.UI.Shot Cathedral_Walk2"
		h "Hollow.Dev.RunInRaid 30 Hollow.UI.Shot Cathedral_Walk3"; h "Hollow.Dev.RunInRaid 34 Hollow.UI.Shot Cathedral_Walk4"   # 4 rooms take ~6 s
		h "Hollow.Dev.RunInRaid 46 Hollow.Dev.GoToNearestCore"; h "Hollow.Dev.RunInRaid 46.5 Hollow.UI.Shot Cathedral_Crypt"
		h "Hollow.Dev.RunInRaid 47 Hollow.Interact"
		h "Hollow.Dev.RunInRaid 49 Hollow.Dev.GoToExtraction All"; h "Hollow.Dev.RunInRaid 50 Hollow.UI.Shot Cathedral_Belfry"
		h "Hollow.Dev.RunInRaid 52 Hollow.Audio.Dump"; C+=("Hollow.Dev.RunInRaid 52.5 Hollow.Audio.Dump")   # the room tones and the sounds heard
		h "Hollow.Dev.RunInRaid 90 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 91 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	altar)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		h "Hollow.Dev.RunAfter 36 Hollow.Hub.Select DA_Contract_Cathedral"           # the contract picks the sector; the client learns it from the manifest
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
		h "Hollow.Dev.RunInRaid 30.2 Hollow.Audio.Dump"; C+=("Hollow.Dev.RunInRaid 30.7 Hollow.Audio.Dump")   # S7-03: purified heard once, a bell a toll
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
	guide)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		# Hub, host time: the objective and the terminals' waypoints; the workbench's screen hides them, then the console.
		h "Hollow.Dev.RunAfter 14 Hollow.UI.Dump"
		h "Hollow.Dev.RunAfter 15 Hollow.UI.Shot Guide_Hub"
		h "Hollow.Dev.RunAfter 17 Hollow.Dev.GoToTerminal Workbench"
		h "Hollow.Dev.RunAfter 18 Hollow.Interact"                                  # the workbench's screen
		h "Hollow.Dev.RunAfter 19.5 Hollow.UI.Dump"                                 # under a screen: no waypoints
		h "$(key 20.5 Escape)"                                                        # back to the visor
		h "Hollow.Dev.RunAfter 22 Hollow.UI.Dump"                                   # 2/2: launch at the console
		h "$(key 23 H)"; h "Hollow.Dev.RunAfter 24 Hollow.UI.Dump"; h "$(key 25 H)"  # the cheat sheet folds and unfolds
		h "Hollow.Dev.RunAfter 26 Hollow.Dev.GoToTerminal Contracts"
		h "Hollow.Dev.RunAfter 27.5 Hollow.UI.Shot Guide_Hub_Console"
		# The gamepad's View on the rendered host (CommonUI routes no UI action on a headless client): folds, unfolds.
		h "$(key 29 Gamepad_Special_Left)"; h "Hollow.Dev.RunAfter 30 Hollow.UI.Dump"
		h "$(key 31 Gamepad_Special_Left)"; h "Hollow.Dev.RunAfter 32 Hollow.UI.Dump"
		# Client time (it starts some 10 s after the host): a guest's objective, its locker, the gamepad's View.
		C1+=("Hollow.Dev.RunAfter 16 Hollow.UI.Dump")
		C1+=("Hollow.Dev.RunAfter 18 Hollow.Hub.Open Locker"); C1+=("Hollow.Dev.RunAfter 19 Hollow.UI.Dump")
		C1+=("Hollow.Dev.RunAfter 20 Hollow.Hub.Close"); C1+=("Hollow.Dev.RunAfter 21 Hollow.UI.Dump")   # 2/2: the host launches
		C1+=("Hollow.Dev.RunAfter 22 Hollow.UI.PressKey Gamepad_Special_Left"); C1+=("Hollow.Dev.RunAfter 23 Hollow.UI.Dump")
		C1+=("Hollow.Dev.RunAfter 24 Hollow.UI.PressKey Gamepad_Special_Left"); C1+=("Hollow.Dev.RunAfter 25 Hollow.UI.Dump")
		h "Hollow.Dev.RunAfter 44 Hollow.Hub.Select DA_Contract_Cathedral"
		h "Hollow.Dev.RunAfter 48 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 1 Hollow.Dev.Invulnerable All"
		# Raid: find the core (after the few seconds of deploying).
		h "Hollow.Dev.RunInRaid 4 Hollow.UI.Dump"; C+=("Hollow.Dev.RunInRaid 4 Hollow.UI.Dump")
		h "Hollow.Dev.RunInRaid 4.2 Hollow.UI.Shot Guide_FindCore"
		h "Hollow.Dev.RunInRaid 4.5 Hollow.Dev.GoToNearestCore Others"
		C1+=("Hollow.Dev.RunInRaid 5 Hollow.Dev.FaceCore"); C1+=("Hollow.Dev.RunInRaid 5.5 Hollow.Interact")
		# The client carries it: purify it or carry it out, each at its own prices.
		h "Hollow.Dev.RunInRaid 7 Hollow.UI.Dump"; C+=("Hollow.Dev.RunInRaid 7.5 Hollow.UI.Dump")
		h "Hollow.Dev.RunInRaid 7.2 Hollow.UI.Shot Guide_Purify"
		h "Hollow.Dev.RunInRaid 8 Hollow.Dev.GoToAltar Others"
		C1+=("Hollow.Dev.RunInRaid 9.5 Hollow.Interact")                            # the altar takes it: the ritual
		h "Hollow.Dev.RunInRaid 11 Hollow.UI.Dump"; C+=("Hollow.Dev.RunInRaid 11.5 Hollow.UI.Dump")
		h "Hollow.Dev.RunInRaid 11.2 Hollow.UI.Shot Guide_Ritual"
		# Art-01 (ADR-0043): during the ritual the host cuts the nearest door: the held cutter glows, sparks fly, the seam heats.
		h "Hollow.Dev.RunInRaid 13 Hollow.Dev.GoToNearestDoor"; h "Hollow.Dev.RunInRaid 13.5 Hollow.Art.View -8"
		h "Hollow.Dev.RunInRaid 14 Hollow.Tool.Hold Hollow.Input.PrimaryUse 3"
		h "Hollow.Dev.RunInRaid 15.5 Hollow.UI.Shot Guide_Sparks"; h "Hollow.Dev.RunInRaid 15.7 Hollow.Art.Dump"
		h "Hollow.Dev.RunInRaid 32 Hollow.Altar.Dump"
		h "Hollow.Dev.RunInRaid 32.5 Hollow.UI.Dump"; C+=("Hollow.Dev.RunInRaid 33 Hollow.UI.Dump")   # purified: deliver it
		h "Hollow.Dev.RunInRaid 32.7 Hollow.UI.Shot Guide_Deliver"
		h "Hollow.Dev.RunInRaid 33.5 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 34 Hollow.Dev.GoToAltar Others"
		C1+=("Hollow.Dev.RunInRaid 35.5 Hollow.Dev.FaceCore"); C1+=("Hollow.Dev.RunInRaid 36 Hollow.Interact")
		h "Hollow.Dev.RunInRaid 38 Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid 42 Hollow.UI.Dump"; C+=("Hollow.Dev.RunInRaid 42.5 Hollow.UI.Dump")   # hold the zone
		h "Hollow.Dev.RunInRaid 42.2 Hollow.UI.Shot Guide_Hold"
		h "Hollow.Dev.RunInRaid 45 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 100 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 101 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	fab)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		# The hub's Fab props from fixed spots (cabin: X ramp 0 .. cockpit 1600, Y port 0 .. starboard 800).
		h "Hollow.Dev.RunAfter 16 Hollow.Dev.GoToRoom World 250 400 0 -4"; h "Hollow.Dev.RunAfter 17 Hollow.UI.Shot Fab_Hub_Fore"
		h "Hollow.Dev.RunAfter 18 Hollow.Dev.GoToRoom World 1350 400 180 4"; h "Hollow.Dev.RunAfter 19 Hollow.UI.Shot Fab_Hub_Aft"
		h "Hollow.Dev.RunAfter 20 Hollow.Dev.GoToRoom World 560 330 -90 -6"; h "Hollow.Dev.RunAfter 21 Hollow.UI.Shot Fab_Hub_Port"
		h "Hollow.Dev.RunAfter 22 Hollow.Dev.GoToRoom World 520 470 90 -6"; h "Hollow.Dev.RunAfter 23 Hollow.UI.Shot Fab_Hub_Starboard"
		h "Hollow.Dev.RunAfter 24 Hollow.Dev.GoToRoom World 1300 400 0 8"; h "Hollow.Dev.RunAfter 25 Hollow.UI.Shot Fab_Hub_Cockpit"
		h "Hollow.Dev.RunAfter 36 Hollow.Hub.Select DA_Contract_Docks"
		h "Hollow.Dev.RunAfter 40 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 1 Hollow.Dev.Invulnerable All"; h "Hollow.Dev.RunInRaid 1.5 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 2 Hollow.Light.Dump"; C+=("Hollow.Dev.RunInRaid 2.5 Hollow.Light.Dump")
		# The Docks rooms (room-local spots, hollow_fab_layout.py): the hero CoreVault twice, the core, then the others.
		h "Hollow.Dev.RunInRaid 3 Hollow.Dev.GoToRoom CoreVault 640 560 238 -12"; h "Hollow.Dev.RunInRaid 4 Hollow.UI.Shot Fab_CoreVault_Pump"
		h "Hollow.Dev.RunInRaid 5 Hollow.Dev.GoToRoom CoreVault 160 420 0 -4"; h "Hollow.Dev.RunInRaid 6 Hollow.UI.Shot Fab_CoreVault_North"
		h "Hollow.Dev.RunInRaid 7 Hollow.Dev.GoToNearestCore"; h "Hollow.Dev.RunInRaid 7.5 Hollow.Dev.FaceCore"
		h "Hollow.Dev.RunInRaid 8.5 Hollow.UI.Shot Fab_Core"
		h "Hollow.Dev.RunInRaid 9.5 Hollow.Dev.GoToRoom Spawn 230 300 40 -6"; h "Hollow.Dev.RunInRaid 10.5 Hollow.UI.Shot Fab_Spawn"
		h "Hollow.Dev.RunInRaid 11.5 Hollow.Dev.GoToRoom Hangar 900 820 215 -6"; h "Hollow.Dev.RunInRaid 12.5 Hollow.UI.Shot Fab_Hangar"
		h "Hollow.Dev.RunInRaid 13.5 Hollow.Dev.GoToRoom Corridor_Straight 300 60 90 0"; h "Hollow.Dev.RunInRaid 14.5 Hollow.UI.Shot Fab_Corridor"
		h "Hollow.Dev.RunInRaid 15.5 Hollow.Dev.GoToRoom Corridor_Corner 110 330 -55 -4"; h "Hollow.Dev.RunInRaid 16.5 Hollow.UI.Shot Fab_Corner"
		h "Hollow.Dev.RunInRaid 17 Hollow.Dev.GoToRoom Junction 90 200 0 14"; h "Hollow.Dev.RunInRaid 17.8 Hollow.UI.Shot Fab_Junction"
		h "Hollow.Dev.RunInRaid 18.5 Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid 60 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 61 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	fabaltar)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		h "Hollow.Dev.RunAfter 36 Hollow.Hub.Select DA_Contract_Cathedral"
		h "Hollow.Dev.RunAfter 40 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 1 Hollow.Dev.Invulnerable All"; h "Hollow.Dev.RunInRaid 1.5 Hollow.AI.Kill"
		# The client lifts the core; the host before it sees its cyan aura on the carrier's hands.
		h "Hollow.Dev.RunInRaid 3 Hollow.Dev.GoToNearestCore Others"
		C1+=("Hollow.Dev.RunInRaid 3.5 Hollow.Dev.FaceCore"); C1+=("Hollow.Dev.RunInRaid 4 Hollow.Interact")
		h "Hollow.Dev.RunInRaid 5 Hollow.Dev.GoToNearestCore"; h "Hollow.Dev.RunInRaid 5.5 Hollow.Dev.FaceCore"
		h "Hollow.Dev.RunInRaid 6 Hollow.UI.Shot FabAltar_Carried"
		# Both before the altar; the client lays the core there: the ritual (20 s), its finale, the purified glow.
		h "Hollow.Dev.RunInRaid 7 Hollow.Dev.GoToAltar Others"
		C1+=("Hollow.Dev.RunInRaid 8.5 Hollow.Interact")
		h "Hollow.Dev.RunInRaid 9.2 Hollow.Dev.GoToAltar"                           # the host beside it, facing the altar
		h "Hollow.Dev.RunInRaid 9.5 Hollow.Altar.Dump"; C+=("Hollow.Dev.RunInRaid 10 Hollow.Altar.Dump")
		h "Hollow.Dev.RunInRaid 11 Hollow.UI.Shot FabAltar_Ritual"; h "Hollow.Dev.RunInRaid 19 Hollow.UI.Shot FabAltar_Ritual2"
		h "Hollow.Dev.RunInRaid 29.4 Hollow.UI.Shot FabAltar_Finale"
		h "Hollow.Dev.RunInRaid 31 Hollow.Altar.Dump"; C+=("Hollow.Dev.RunInRaid 31.5 Hollow.Altar.Dump")
		h "Hollow.Dev.RunInRaid 32 Hollow.UI.Shot FabAltar_Purified"
		h "Hollow.Dev.RunInRaid 33 Hollow.Dev.FaceCore"; h "Hollow.Dev.RunInRaid 33.5 Hollow.UI.Shot FabAltar_PurifiedCore"
		h "Hollow.Dev.RunInRaid 35 Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid 80 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 81 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	loc)
		HOST_ARGS=(-culture=ru)                                                       # the host in Russian, this run only
		h "Hollow.Dev.RunAfter 4 Hollow.UI.Language"; h "Hollow.Dev.RunAfter 5 Hollow.UI.Shot Loc_MainMenu"
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		h "Hollow.Dev.RunAfter 15 Hollow.UI.Dump"; h "Hollow.Dev.RunAfter 15.5 Hollow.UI.Shot Loc_Hub"
		h "Hollow.Dev.RunAfter 17 Hollow.Dev.GoToTerminal Workbench"; h "Hollow.Dev.RunAfter 18 Hollow.Interact"
		h "Hollow.Dev.RunAfter 19.5 Hollow.UI.Shot Loc_Craft"
		h "$(key 20 E)"; h "Hollow.Dev.RunAfter 21 Hollow.UI.Shot Loc_Shop"           # E: the next tab
		h "$(key 22 E)"; h "Hollow.Dev.RunAfter 23 Hollow.UI.Shot Loc_Upgrades"
		h "$(key 24 Escape)"
		h "Hollow.Dev.RunAfter 25 Hollow.Dev.GoToTerminal Locker"; h "Hollow.Dev.RunAfter 26 Hollow.Interact"
		h "Hollow.Dev.RunAfter 27.5 Hollow.UI.Shot Loc_Locker"; h "$(key 28 Escape)"
		h "$(key 29.5 Escape)"; h "Hollow.Dev.RunAfter 31 Hollow.UI.Shot Loc_SystemMenu"   # the system menu, its language button
		h "Hollow.Dev.RunAfter 32 Hollow.UI.Language en"; h "Hollow.Dev.RunAfter 33 Hollow.UI.Shot Loc_SystemMenu_EN"   # at once
		h "Hollow.Dev.RunAfter 34 Hollow.UI.Language ru"; h "Hollow.Dev.RunAfter 35 Hollow.UI.Shot Loc_SystemMenu_RU"
		h "$(key 36 Escape)"
		h "Hollow.Dev.RunAfter 37 Hollow.Dev.GoToTerminal Contracts"; h "Hollow.Dev.RunAfter 38 Hollow.Interact"
		h "Hollow.Dev.RunAfter 39.5 Hollow.UI.Shot Loc_Contracts"; h "$(key 40.5 Escape)"
		C1+=("Hollow.Dev.RunAfter 18 Hollow.UI.Language"); C1+=("Hollow.Dev.RunAfter 18.5 Hollow.UI.Dump")   # the guest: English
		C1+=("Hollow.Dev.RunAfter 20 Hollow.Hub.Open Locker"); C1+=("Hollow.Dev.RunAfter 21 Hollow.UI.Dump"); C1+=("Hollow.Dev.RunAfter 22 Hollow.Hub.Close")
		h "Hollow.Dev.RunAfter 45 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 1 Hollow.Dev.Invulnerable All"; h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 4 Hollow.UI.Dump"; C+=("Hollow.Dev.RunInRaid 4.5 Hollow.UI.Dump")
		h "Hollow.Dev.RunInRaid 4.2 Hollow.UI.Shot Loc_Raid"
		h "Hollow.Dev.RunInRaid 5 Hollow.Dev.GoToNearestCore"; h "Hollow.Dev.RunInRaid 6 Hollow.Dev.FaceCore"
		h "Hollow.Dev.RunInRaid 6.8 Hollow.UI.Shot Loc_Prompt"; h "Hollow.Dev.RunInRaid 7 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 7.5 Hollow.Interact"                                  # the host lifts the core
		h "Hollow.Dev.RunInRaid 9 Hollow.UI.Language en"; h "Hollow.Dev.RunInRaid 10 Hollow.UI.Shot Loc_Raid_EN"
		h "Hollow.Dev.RunInRaid 11 Hollow.UI.Language ru"; h "Hollow.Dev.RunInRaid 12 Hollow.UI.Shot Loc_Raid_RU"
		h "Hollow.Dev.RunInRaid 13 Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid 17 Hollow.UI.Shot Loc_Extraction"; h "Hollow.Dev.RunInRaid 17.5 Hollow.UI.Dump"
		for t in 55 70 85; do h "Hollow.Dev.RunInRaid $t Hollow.UI.Shot Loc_After_$t"; done   # the debrief (raid over, then the hub)
		h "Hollow.Dev.RunInRaid 95 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 100 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 101 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	ui)
		HOST_ARGS=(-culture=ru)                                                       # the host in Russian, this run only
		# One pass per language over the same screens: the live main menu and the settings' four tabs (E: the next).
		shots() {   # $1: start (s after launch), $2: language suffix
			local s=$1 l=$2
			h "Hollow.Dev.RunAfter $s Hollow.UI.Shot UI_MainMenu_$l"
			h "$(key $((s + 1)) $DOWN)"; h "$(key $((s + 2)) $PRESS)"                   # Host co-op -> Settings
			h "Hollow.Dev.RunAfter $((s + 3)) Hollow.UI.Shot UI_Settings_Graphics_$l"; h "Hollow.Dev.RunAfter $((s + 3)) Hollow.UI.Dump"
			h "$(key $((s + 4)) E)"; h "Hollow.Dev.RunAfter $((s + 5)) Hollow.UI.Shot UI_Settings_Audio_$l"
			h "$(key $((s + 6)) E)"; h "Hollow.Dev.RunAfter $((s + 7)) Hollow.UI.Shot UI_Settings_Controls_$l"
			h "$(key $((s + 8)) E)"; h "Hollow.Dev.RunAfter $((s + 9)) Hollow.UI.Shot UI_Settings_Accessibility_$l"
			h "$(key $((s + 10)) Escape)"                                             # back to the main menu
		}
		shots 5 RU
		h "Hollow.Dev.RunAfter 17 Hollow.UI.Language en"; shots 18 EN
		h "Hollow.Dev.RunAfter 30 Hollow.UI.Language ru"
		h "$(key 31 $PRESS)"                                                          # main menu: Host co-op -> the Skiff
		hub() {   # $1: start, $2: language suffix
			local s=$1 l=$2
			h "Hollow.Dev.RunAfter $s Hollow.UI.Shot UI_Hub_$l"; h "Hollow.Dev.RunAfter $s Hollow.UI.Dump"
			local t=$((s + 1)) terminal
			for terminal in Workbench Locker Contracts Codex; do
				h "Hollow.Dev.RunAfter $t Hollow.Dev.GoToTerminal $terminal"; h "Hollow.Dev.RunAfter $((t + 1)) Hollow.Interact"
				h "Hollow.Dev.RunAfter $((t + 2)) Hollow.UI.Shot UI_${terminal}_$l"; h "Hollow.Dev.RunAfter $((t + 2)) Hollow.UI.Dump"   # UI-H7: no prompt
				h "$(key $((t + 3)) Escape)"; t=$((t + 4))
			done
			h "$(key $t Escape)"; h "Hollow.Dev.RunAfter $((t + 1)) Hollow.UI.Shot UI_SystemMenu_$l"   # the system menu
			h "$(key $((t + 2)) $DOWN)"; h "$(key $((t + 3)) $DOWN)"; h "$(key $((t + 4)) $DOWN)"; h "$(key $((t + 5)) $PRESS)"   # Resume, Invite, Start raid -> Settings
			h "Hollow.Dev.RunAfter $((t + 6)) Hollow.UI.Shot UI_SystemSettings_$l"
			h "$(key $((t + 7)) Escape)"; h "Hollow.Dev.RunAfter $((t + 8)) Hollow.UI.Dump"; h "$(key $((t + 9)) Escape)"
		}
		hub 43 RU
		h "Hollow.Dev.RunAfter 70 Hollow.UI.Language en"; hub 71 EN
		h "Hollow.Dev.RunAfter 98 Hollow.UI.Language ru"
		h "Hollow.Dev.RunAfter 100 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 4 Hollow.UI.Shot UI_Raid_RU"; h "Hollow.Dev.RunInRaid 4.5 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 6 Hollow.UI.Language en"; h "Hollow.Dev.RunInRaid 7 Hollow.UI.Shot UI_Raid_EN"
		h "Hollow.Dev.RunInRaid 8 Hollow.UI.Language ru"
		h "Hollow.Dev.RunInRaid 10 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 6 Hollow.UI.Dump"); C+=("Hollow.Dev.RunInRaid 11 Hollow.Profile.Dump")
		CLIENT_OPEN="Hollow.Dev.RunAfter 40 open 127.0.0.1"   # the host hosts after its menu shots (31 s): an open at once times out (UI2-1)
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	chars)
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		h "Hollow.Dev.RunAfter 30 Hollow.Raid.Start ${SEED:-1624260639}"              # the selected contract, a fixed sector (A/B)
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"; h "Hollow.Dev.RunInRaid 2.2 Hollow.Dev.Invulnerable All"
		h "Hollow.Dev.RunInRaid 2.5 Hollow.Graphics.Dump"
		EXO=/Game/Hollow/Characters/ExoFrame/BP_ExoFrame.BP_ExoFrame_C
		PILGRIM=/Game/Hollow/Characters/Enemies/IronPilgrim/BP_IronPilgrim.BP_IronPilgrim_C
		STALKER=/Game/Hollow/Characters/Enemies/EchoStalker/BP_EchoStalker.BP_EchoStalker_C
		# Each body alone, without its AI (room-local spots of the fab mode, hollow_fab_layout.py).
		h "Hollow.Dev.RunInRaid 3 Hollow.Dev.GoToRoom Spawn 230 300 40 -6"; h "Hollow.Dev.RunInRaid 4 Hollow.Art.Showcase $EXO 240 0"
		h "Hollow.Dev.RunInRaid 6.5 Hollow.Art.Dump"; h "Hollow.Dev.RunInRaid 7 Hollow.UI.Shot CHAR01_ExoFrame"
		h "Hollow.Dev.RunInRaid 8 Hollow.Dev.GoToRoom CoreVault 160 420 0 -4"; h "Hollow.Dev.RunInRaid 9 Hollow.Art.Showcase $STALKER 340 0"
		h "Hollow.Dev.RunInRaid 11.5 Hollow.UI.Shot CHAR01_Stalker"
		h "Hollow.Dev.RunInRaid 13 Hollow.Dev.GoToRoom Hangar 900 820 215 -6"; h "Hollow.Dev.RunInRaid 14.5 Hollow.UI.Shot CHAR01_FirstPerson"
		h "Hollow.Dev.RunInRaid 15 Hollow.Art.Showcase $PILGRIM 520 0"; h "Hollow.Dev.RunInRaid 17.5 Hollow.UI.Shot CHAR01_Pilgrim"
		# All three in view: the shot, then the frames (the FPS A/B scene).
		h "Hollow.Dev.RunInRaid 18 Hollow.Art.Showcase $STALKER 470 -260"; h "Hollow.Dev.RunInRaid 18.5 Hollow.Art.Showcase $EXO 400 240"
		h "Hollow.Dev.RunInRaid 21 Hollow.UI.Shot CHAR01_Group"; h "Hollow.Dev.RunInRaid 21.5 Hollow.Art.Dump"
		h "Hollow.Dev.RunInRaid 22 Hollow.Perf.Capture CHAR01_Group 20 2"
		h "Hollow.Dev.RunInRaid 48 Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid 90 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 91 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	alert)
		HOST_ARGS=(-culture=ru)                                                       # the host in Russian, this run only
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		h "Hollow.Dev.RunAfter 30 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"; h "Hollow.Dev.RunInRaid 2.2 Hollow.Dev.Invulnerable All"
		# 1. The host's shout wakes a Pilgrim behind a door: it calls out; ACOUSTIC ALERT on the host's visor only.
		h "Hollow.Dev.RunInRaid 3 Hollow.AI.Spawn Behind"
		h "Hollow.Dev.RunInRaid 5 Hollow.AI.Noise Hollow.Noise.Voice.Shout"
		h "Hollow.Dev.RunInRaid 5.5 Hollow.UI.Shot Alert_Visor_RU"; h "Hollow.Dev.RunInRaid 5.7 Hollow.UI.Dump"; C1+=("Hollow.Dev.RunInRaid 5.7 Hollow.UI.Dump")
		# 2. A fresh Pilgrim; the client shouts into its microphone: the client's visor only.
		h "Hollow.Dev.RunInRaid 7 Hollow.AI.Kill"; h "Hollow.Dev.RunInRaid 8 Hollow.AI.Spawn Behind"
		C1+=("Hollow.Dev.RunInRaid 10 Hollow.Voice.Simulate 230 3")
		h "Hollow.Dev.RunInRaid 11.5 Hollow.UI.Dump"; C1+=("Hollow.Dev.RunInRaid 11.5 Hollow.UI.Dump")
		# 3. In English: the host's shotgun wakes another (its cooldown over); one more at once calls out, the visor quiet.
		# (A shotgun, not the nailgun: the door with room behind it may be farther than a nailgun carries through walls.)
		h "Hollow.Dev.RunInRaid 13 Hollow.UI.Language en"; h "Hollow.Dev.RunInRaid 13.5 Hollow.AI.Kill"; h "Hollow.Dev.RunInRaid 14 Hollow.AI.Spawn Behind"
		h "Hollow.Dev.RunInRaid 16 Hollow.AI.Noise Hollow.Noise.Weapon.Shotgun"; h "Hollow.Dev.RunInRaid 16.5 Hollow.UI.Shot Alert_Visor_EN"
		h "Hollow.Dev.RunInRaid 17 Hollow.AI.Kill"; h "Hollow.Dev.RunInRaid 17.3 Hollow.AI.Spawn Behind"
		h "Hollow.Dev.RunInRaid 19.3 Hollow.AI.Noise Hollow.Noise.Weapon.Shotgun"; h "Hollow.Dev.RunInRaid 19.8 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 20 Hollow.UI.Language ru"
		# 4. The Echo-Stalker the director brings hears a shotgun blast.
		h "Hollow.Dev.RunInRaid 21 Hollow.AI.Kill"; h "Hollow.Dev.RunInRaid 22 Hollow.AI.Director Spawn"
		h "Hollow.Dev.RunInRaid 27 Hollow.AI.Noise Hollow.Noise.Weapon.Shotgun"; h "Hollow.Dev.RunInRaid 27.5 Hollow.AI.Dump"
		# 5. Immersive waypoints: hidden in the raid, shown 5 s on H (the cheat sheet folds with it), in Russian and English.
		h "Hollow.Dev.RunInRaid 29 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 30 Hollow.UI.Waypoints Immersive"; h "Hollow.Dev.RunInRaid 31 Hollow.UI.Shot Alert_Immersive_Hidden_RU"; h "Hollow.Dev.RunInRaid 31.2 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 32 Hollow.UI.PressKey H"; h "Hollow.Dev.RunInRaid 32.6 Hollow.UI.Shot Alert_Immersive_Shown_RU"; h "Hollow.Dev.RunInRaid 32.8 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 38.5 Hollow.UI.Dump"                                 # 5 s later: hidden again
		h "Hollow.Dev.RunInRaid 39 Hollow.UI.Language en"; h "Hollow.Dev.RunInRaid 40 Hollow.UI.PressKey H"; h "Hollow.Dev.RunInRaid 40.6 Hollow.UI.Shot Alert_Immersive_Shown_EN"
		h "Hollow.Dev.RunInRaid 41 Hollow.UI.Language ru"; h "Hollow.Dev.RunInRaid 42 Hollow.UI.Waypoints Full"
		h "Hollow.Dev.RunInRaid 43 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 44 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	story)
		HOST_ARGS=(-culture=ru)                                                       # the host in Russian, this run only
		h "Hollow.Dev.RunAfter 3 Hollow.Radio.Reset"; C1+=("Hollow.Dev.RunAfter 3 Hollow.Radio.Reset")   # the briefing unheard
		h "Hollow.Dev.RunAfter 3.5 Hollow.Lore.Reset"; C1+=("Hollow.Dev.RunAfter 3.5 Hollow.Lore.Reset")  # NAR-01b: no crew log yet
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		h "Hollow.Dev.RunAfter 16 Hollow.UI.Dump"; h "Hollow.Dev.RunAfter 16.5 Hollow.UI.Shot Story_Briefing_RU"
		h "Hollow.Dev.RunAfter 24 Hollow.UI.Language en"; h "Hollow.Dev.RunAfter 25 Hollow.UI.Shot Story_Briefing_EN"
		h "Hollow.Dev.RunAfter 25.5 Hollow.UI.Dump"
		h "Hollow.Dev.RunAfter 26 Hollow.UI.Language ru"; h "Hollow.Dev.RunAfter 27 Hollow.UI.Shot Story_Briefing_RU2"
		C1+=("Hollow.Dev.RunAfter 22 Hollow.UI.Dump")                                 # the guest's own briefing, in English
		h "Hollow.Dev.RunAfter 47 Hollow.UI.Dump"; C1+=("Hollow.Dev.RunAfter 47 Hollow.UI.Dump")   # over: quiet, heard for good
		h "Hollow.Dev.RunAfter 48 Hollow.Dev.GoToTerminal Codex"; h "Hollow.Dev.RunAfter 49 Hollow.Interact"   # NAR-01b: the crew archive
		h "Hollow.Dev.RunAfter 50.5 Hollow.UI.Shot Story_Codex_Empty"; h "Hollow.Dev.RunAfter 51 Hollow.UI.Dump"; h "$(key 52 Escape)"
		h "Hollow.Dev.RunAfter 54 Hollow.Hub.Launch"
		h "Hollow.Dev.RunInRaid 1 Hollow.Dev.Invulnerable All"; h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 3 Hollow.UI.Dump"; h "Hollow.Dev.RunInRaid 3.5 Hollow.UI.Shot Story_Sortie"; C+=("Hollow.Dev.RunInRaid 3 Hollow.UI.Dump")
		h "Hollow.Dev.RunInRaid 5 Hollow.Lore.Dump"                                   # NAR-01b: the sector's two datapads
		h "Hollow.Dev.RunInRaid 6 Hollow.Dev.GoToNearestDatapad"; h "Hollow.Dev.RunInRaid 7 Hollow.Dev.FaceNearestDatapad"
		h "Hollow.Dev.RunInRaid 7.5 Hollow.Interact"; h "Hollow.Dev.RunInRaid 8.5 Hollow.UI.Shot Story_Datapad"; h "Hollow.Dev.RunInRaid 9 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 10 Hollow.Dev.GoToNearestDatapad"; h "Hollow.Dev.RunInRaid 11 Hollow.Dev.FaceNearestDatapad"
		h "Hollow.Dev.RunInRaid 11.5 Hollow.Interact"; h "Hollow.Dev.RunInRaid 12.5 Hollow.UI.Shot Story_Datapad2"
		C+=("Hollow.Dev.RunInRaid 14 Hollow.Lore.Dump")                               # the guest got both logs, in its own archive
		h "Hollow.Dev.RunInRaid 16 Hollow.Dev.GoToNearestCore"; h "Hollow.Dev.RunInRaid 17 Hollow.Dev.FaceCore"
		h "Hollow.Dev.RunInRaid 19 Hollow.UI.Dump"; h "Hollow.Dev.RunInRaid 19.5 Hollow.UI.Shot Story_CoreSighted"
		h "Hollow.Dev.RunInRaid 25 Hollow.Interact"                                   # the host lifts the core
		h "Hollow.Dev.RunInRaid 28 Hollow.UI.Dump"; h "Hollow.Dev.RunInRaid 28.5 Hollow.UI.Shot Story_CoreLifted"
		C+=("Hollow.Dev.RunInRaid 28 Hollow.UI.Dump")                                 # the guest heard the lift too, in English
		h "Hollow.Dev.RunInRaid 35 Hollow.Radio.Fire Stalker.Contact Force"           # the stalker was killed: its mimicry forced
		h "Hollow.Dev.RunInRaid 36 Hollow.UI.Shot Story_Echo"; h "Hollow.Dev.RunInRaid 36.5 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 46 Hollow.Radio.Fire Resonance.Critical Force"; h "Hollow.Dev.RunInRaid 47 Hollow.UI.Shot Story_Resonance"
		h "Hollow.Dev.RunInRaid 54 Hollow.Dev.Kill Others"                            # the client dies: the host hears the loss
		h "Hollow.Dev.RunInRaid 55 Hollow.UI.Shot Story_SquadLoss"; h "Hollow.Dev.RunInRaid 55.5 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 62 Hollow.Dev.GoToExtraction All"
		h "Hollow.Dev.RunInRaid 65 Hollow.UI.Shot Story_Extraction"; h "Hollow.Dev.RunInRaid 65.5 Hollow.UI.Dump"
		# NAR-01b: back in the hub, the crew archive with the two logs, in Russian, then in English.
		h "Hollow.Dev.RunInRaid 140 Hollow.Dev.GoToTerminal Codex"; h "Hollow.Dev.RunInRaid 141 Hollow.Interact"
		h "Hollow.Dev.RunInRaid 142.5 Hollow.UI.Shot Story_Codex_RU"; h "Hollow.Dev.RunInRaid 143 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 144 Hollow.UI.Language en"; h "Hollow.Dev.RunInRaid 145 Hollow.UI.Shot Story_Codex_EN"
		h "Hollow.Dev.RunInRaid 146 Hollow.UI.Language ru"; h "Hollow.Dev.RunInRaid 147 Hollow.UI.PressKey Escape"
		h "Hollow.Dev.RunInRaid 150 Hollow.Lore.Dump"; C+=("Hollow.Dev.RunInRaid 150 Hollow.Lore.Dump")
		h "Hollow.Dev.RunInRaid 152 Hollow.Profile.Dump"; C+=("Hollow.Dev.RunInRaid 153 Hollow.Profile.Dump")
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	biomes)
		D=HollowSector_Docks; K=HollowSector_Cathedral
		h "$(key 6 $PRESS)"                                                           # main menu: Host co-op -> the Skiff
		# Hub 1. The client (its own clock): Techno-Occultist, the Ring of Silence crafted (1 shard bought) and equipped.
		C1+=("Hollow.Dev.RunAfter 17 Hollow.Class.Select DA_Class_Occultist")
		C1+=("Hollow.Dev.RunAfter 18 Hollow.Shop.Buy DA_Item_AetherShard 1")
		C1+=("Hollow.Dev.RunAfter 18.5 Hollow.Craft.Make DA_Recipe_Relic_RingOfSilence 1")
		C1+=("Hollow.Dev.RunAfter 19 Hollow.Profile.Equip DA_Item_Relic_RingOfSilence 1")
		C1+=("Hollow.Dev.RunAfter 20 Hollow.Class.Dump"); C1+=("Hollow.Dev.RunAfter 20.5 Hollow.Shop.Dump")
		C1+=("Hollow.Dev.RunAfter 21 Hollow.Sector.Status $D"); C1+=("Hollow.Dev.RunAfter 21.2 Hollow.Sector.Status $K")
		C1+=("Hollow.Dev.RunAfter 22 Hollow.Hub.Open Contracts"); C1+=("Hollow.Dev.RunAfter 23 Hollow.UI.Dump")   # a guest's screen: the Docks
		C1+=("Hollow.Dev.RunAfter 24 Hollow.Hub.Close")
		h "Hollow.Dev.RunAfter 14 Hollow.Hub.Dump"
		h "Hollow.Dev.RunAfter 14.5 Hollow.Sector.Status $D"; h "Hollow.Dev.RunAfter 14.7 Hollow.Sector.Status $K"
		h "Hollow.Dev.RunAfter 45 Hollow.Hub.Launch"                                # the default contract: the Docks
		# Raid 1, the Docks.
		h "Hollow.Dev.RunInRaid 1 Hollow.Dev.Invulnerable All"; h "Hollow.Dev.RunInRaid 2 Hollow.AI.Kill"
		h "Hollow.Dev.RunInRaid 2.5 Hollow.Sector.Info"; C+=("Hollow.Dev.RunInRaid 3 Hollow.Sector.Info")
		h "Hollow.Dev.RunInRaid 3.2 Hollow.Sector.Status $D"; h "Hollow.Dev.RunInRaid 3.4 Hollow.Sector.Status $K"
		C+=("Hollow.Dev.RunInRaid 3.5 Hollow.Sector.Status $D"); C+=("Hollow.Dev.RunInRaid 3.7 Hollow.Sector.Status $K")
		h "Hollow.Dev.RunInRaid 4 Hollow.Class.Dump"; C+=("Hollow.Dev.RunInRaid 4 Hollow.Class.Dump"); C+=("Hollow.Dev.RunInRaid 4.5 Hollow.Gear.Dump")
		h "Hollow.Dev.RunInRaid 5 Hollow.Dev.GoToNearestCore All"; h "Hollow.Dev.RunInRaid 6 Hollow.Interact"
		h "Hollow.Dev.RunInRaid 7 Hollow.Dev.GoToExtraction All"
		# Hub 2 (the raid's clock goes on): nothing of the Docks left after a collection, the contract kept.
		h "Hollow.Dev.RunInRaid 46 obj gc"; C+=("Hollow.Dev.RunInRaid 46 obj gc")
		h "Hollow.Dev.RunInRaid 47 Hollow.Sector.Status $D"; h "Hollow.Dev.RunInRaid 47.2 Hollow.Sector.Status $K"
		C+=("Hollow.Dev.RunInRaid 47.5 Hollow.Sector.Status $D"); C+=("Hollow.Dev.RunInRaid 47.7 Hollow.Sector.Status $K")
		h "Hollow.Dev.RunInRaid 48 Hollow.Hub.Dump"; C+=("Hollow.Dev.RunInRaid 48.5 Hollow.Hub.Dump")
		h "Hollow.Dev.RunInRaid 49 Hollow.UI.PressKey Escape"                        # the unread debrief
		C1+=("Hollow.Dev.RunInRaid 50 Hollow.Hub.Open Contracts"); C1+=("Hollow.Dev.RunInRaid 51 Hollow.UI.Dump")
		# The host at its terminal: focus on Launch, up to the Cathedral's Choose (rows in id order), A.
		h "Hollow.Dev.RunInRaid 51 Hollow.Dev.GoToTerminal Contracts"; h "Hollow.Dev.RunInRaid 52 Hollow.Interact"
		h "Hollow.Dev.RunInRaid 53 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 53.5 Hollow.UI.PressKey Gamepad_DPad_Up"; h "Hollow.Dev.RunInRaid 54 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 54.5 Hollow.UI.PressKey Gamepad_DPad_Up"; h "Hollow.Dev.RunInRaid 55 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 55.5 Hollow.UI.PressKey $PRESS"; h "Hollow.Dev.RunInRaid 56.5 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 57 Hollow.UI.Shot Biomes_Contracts"
		C1+=("Hollow.Dev.RunInRaid 58 Hollow.UI.Dump"); C1+=("Hollow.Dev.RunInRaid 59 Hollow.Hub.Close")   # live: the host's Cathedral
		C+=("Hollow.Dev.RunInRaid 59.5 Hollow.Hub.Dump")
		h "Hollow.Dev.RunInRaid 59 Hollow.UI.PressKey Gamepad_FaceButton_Right"     # B: closed; again: the focus on Launch
		h "Hollow.Dev.RunInRaid 59.5 Hollow.Dev.GoToTerminal Contracts"              # (the view turns when the screen closes)
		h "Hollow.Dev.RunInRaid 60.5 Hollow.Interact"; h "Hollow.Dev.RunInRaid 61.5 Hollow.UI.Dump"
		h "Hollow.Dev.RunInRaid 62.5 Hollow.UI.PressKey $PRESS"                      # A on Launch: the Cathedral
		# Raid 2, the Cathedral: registered in the hub, so these wait for the next ready raid.
		N="Hollow.Dev.RunInRaid 50 Hollow.Dev.RunInRaid"
		h "$N 1 Hollow.Dev.Invulnerable All"
		h "$N 2 Hollow.Sector.Info"; C+=("$N 2.5 Hollow.Sector.Info")
		h "$N 2.2 Hollow.Sector.Status $K"; h "$N 2.4 Hollow.Sector.Status $D"
		C+=("$N 2.7 Hollow.Sector.Status $K"); C+=("$N 2.9 Hollow.Sector.Status $D")
		h "$N 3 Hollow.Class.Dump"; C+=("$N 3 Hollow.Class.Dump"); C+=("$N 3.5 Hollow.Gear.Dump"); h "$N 3.7 Hollow.Altar.Dump"
		# The client faces the core 1.5 s after the host moved it there: the move reaches it through the 150 ms link first.
		h "$N 4 Hollow.Dev.GoToNearestCore Others"                                   # the Occultist lifts the core...
		C1+=("$N 5.5 Hollow.Dev.FaceCore"); C1+=("$N 6 Hollow.Interact")
		h "$N 7.5 Hollow.Dev.GoToAltar Others"; C1+=("$N 9 Hollow.Interact")         # ...and lays it on the altar
		h "$N 10 Hollow.Altar.Dump"; C+=("$N 10.5 Hollow.Altar.Dump"); h "$N 10.2 Hollow.AI.Heat"
		h "$N 31 Hollow.Altar.Dump"; C+=("$N 31.5 Hollow.Altar.Dump")               # done: purified
		h "$N 31.2 Hollow.Audio.Dump"; C+=("$N 31.7 Hollow.Audio.Dump")
		h "$N 32 Hollow.AI.Kill"
		h "$N 33 Hollow.Dev.GoToArc Breaker"; h "$N 34 Hollow.Interact"             # the Engineer cuts an arc's breaker
		h "$N 35 Hollow.Arc.Dump"; C+=("$N 35.5 Hollow.Arc.Dump")
		# The purified core sits on the altar's bowl: from in front of the altar (GoToNearestCore may stand diagonally,
		# where the altar's candles hide the core from the focus probe, KnownIssues RG-3).
		h "$N 37 Hollow.Dev.GoToAltar Others"
		C1+=("$N 38.5 Hollow.Dev.FaceCore"); C1+=("$N 39 Hollow.Interact")          # the purified core
		h "$N 41 Hollow.Dev.GoToExtraction All"
		# Hub 3 (raid 2's clock): both sectors released and unloaded, the contract kept, the profiles.
		h "$N 82 obj gc"; C+=("$N 82 obj gc")
		h "$N 83 Hollow.Sector.Status $D"; h "$N 83.2 Hollow.Sector.Status $K"
		C+=("$N 83.5 Hollow.Sector.Status $D"); C+=("$N 83.7 Hollow.Sector.Status $K")
		h "$N 84 Hollow.Hub.Dump"; C+=("$N 84.5 Hollow.Hub.Dump")
		h "$N 86 Hollow.Profile.Dump"; C+=("$N 89 Hollow.Profile.Dump")              # the client's ends the run: the host's first
		END='Hollow[.]Profile[.]Dump: [0-9]'
		;;
	screens)
		h "$(key 6 $UP)"; h "$(key 7 $PRESS)"                                         # main menu: Host co-op -> Play solo (above it, UI-02)
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
		h "$(key 6 $UP)"; h "$(key 7 $PRESS)"                                         # main menu: Host co-op -> Play solo (above it, UI-02)
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
	# The clients start once the host's main menu is up (their "open" waits for the host to listen): the scenarios'
	# client clocks are tuned to that. Until S7-04 this waited for "listening on port", which the trace server's
	# "Control listening on port 1985" matched at the same moment; the game's driver listens a menu press later (RG-1).
	wait_for "$OUT/Host.log" 'UI up for .*[(]main menu[)]' 240 || { echo "the host's menu never came up"; exit 1; }
	CLIENTS=3
	[[ "$MODE" == carry* || "$MODE" == loadout || "$MODE" == hub || "$MODE" == classes || "$MODE" == lantern || "$MODE" == ward || "$MODE" == relics || "$MODE" == cathedral || "$MODE" == altar || "$MODE" == biomes || "$MODE" == guide || "$MODE" == weapons || "$MODE" == loot || "$MODE" == swarm || "$MODE" == loc || "$MODE" == story || "$MODE" == fab || "$MODE" == fabaltar || "$MODE" == ui || "$MODE" == alert || "$MODE" == chars ]] && CLIENTS=1
	for ((i = 1; i <= CLIENTS; i++)); do
		CMDS=("t.MaxFPS $CLIENT_FPS" "p.NetShowCorrections 1" "$CLIENT_OPEN" ${C[@]+"${C[@]}"})
		[[ "$i" == 1 ]] && CMDS+=(${C1[@]+"${C1[@]}"})
		launch "Client$i" "${HEADLESS[@]}" ${NET_ARGS[@]+"${NET_ARGS[@]}"} -ExecCmds="$(join "${CMDS[@]}")"
		[[ "$i" == 1 ]] && CLIENT1="${PIDS[${#PIDS[@]}-1]}"
		sleep "$CLIENT_STAGGER"
	done
	LIMIT=500
	[[ "$MODE" == soak ]] && LIMIT=$((MINUTES * 60 + 600))
	# Client 1 runs the scenario's client side and says when it is over; if it dies, say so now (KnownIssues V-6).
	HOST_WAIT=90   # seconds for the host to host after its menu (one press, a map load)
	for ((t = 0; t < LIMIT; t++)); do
		grep -qE "$END" "$OUT/Client1.log" 2> /dev/null && break
		kill -0 "$CLIENT1" 2> /dev/null || { echo "client 1 died after ${t} s (exit: see ~/Library/Logs/DiagnosticReports)"; break; }
		((t == HOST_WAIT)) && ! grep -q 'IpNetDriver listening on port' "$OUT/Host.log" && { echo "the host never hosted (its menu press was lost?)"; break; }
		sleep 1
	done
	((t >= LIMIT)) && echo "the $MODE playthrough did not finish"
fi
sleep 3
# The clients first: a host gone before them makes each log "Host closed the connection" as errors (teardown only).
((${#PIDS[@]} > 1)) && { kill "${PIDS[@]:1}" 2>/dev/null || true; sleep 2; }
kill ${PIDS[@]+"${PIDS[@]}"} 2>/dev/null || true
wait ${PIDS[@]+"${PIDS[@]}"} 2>/dev/null || true

SUMMARY_LINES=160
[[ "$MODE" == biomes ]] && SUMMARY_LINES=320   # two raids and three hubs
[[ "$MODE" == guide || "$MODE" == loc || "$MODE" == story ]] && SUMMARY_LINES=260
[[ "$MODE" == ui ]] && SUMMARY_LINES=420   # two passes over every screen
[[ "$MODE" == alert ]] && SUMMARY_LINES=260    # every step's objective on both machines
strip() { sed 's/^\[[^]]*\]\[[^]]*\]//' | sed -E 's/Log[A-Za-z]+: (Display: )?//' | cut -c1-260; }
for log in "$OUT"/*.log; do
	echo "===== $(basename "$log" .log)"
	grep -hE "UI up for|Session: State|Raid .*: .* -> |result (Extracted|KilledInAction|MissingInAction|SignalLost|Abandoned)|every hash matches|debrief|Hollow.Profile.Dump: [0-9]|Carry.Dump: |Hollow.Perf: Soak|Hollow.UI.Dump: layer|Hollow.Dev.RunAfter: Hollow.UI|Voice: .*(Shout|Speech|Whisper)|Hollow.Voice.Dump|Hollow.Lamp.Dump|helmet lamp|Hollow.UI.Dump: (glass|resonance|voice|lamp)|WalkRoute: |FollowCarry: |OpenDoors: |carrier corrected|Client: Error for|client-authoritative|server-authoritative again|lets go|joins .* \\(Coop|tether breaks|sanity check|its moves show|lifted by|dropped by|ForcePositionUpdate|Inventory.Dump: |takes |uses [0-9]|lays |refuels|asks .* for the loadout|loadout of|Profile: loadout|Hollow.UI.Dump: supplies|Profile.SetSlot|Hollow.Shop.(Buy|Sell|Insure|Dump): |Hollow.Craft.(Make|Dump): |Hollow.Profile.(Equip|Unequip): |resolved as|Hub: |Hollow.Hub.Dump: |GoToTerminal: |Hollow.UI.Shot: |Shop.BuyUpgrade: |Raid: travelling|contract |Hollow.UI.Dump: (hub|input|class)|_C_[0-9]+: |shows W_|Hollow.Class.(Dump|Select): | plays DA_Class|Class: |Aether light|Lantern light|Exposed glow|speed x[0-9]|Hollow.AI.Dump: (enemy|brain)|Hollow.AI.Spawn: |Hollow.Resonance.Dump: player|Hollow.Tool.Info: |FaceNearestEnemy: |GoToNearestEnemy| burns at | goes out |: inside [0-9]|Hollow.Gadget.Dump: |lays BP_|takes back|Hollow.Gear.Dump: |Hollow.Profile.Credits: |cut down|Hollow.Sector.(Info|Status): |Hollow.Nav.Path [A-Za-z]+: |Hollow.AI.Dump: |Hollow.(Altar|Arc).Dump: |lays .* on the altar|ritual is over|: toll [0-9]|strikes |power (on|off)|turned (on|off) by|counts as |held by a fixture|free to lift|Hollow.AI.Heat: |Raid contract |Sector (activated|deactivated): |hub role |Hollow.UI.Dump: (objective|tracker|waypoints|controls)|Objective of |Controls cheat sheet |Hollow.Art.Dump: |Hollow.Weapon.Dump: |Hollow.Feel.Dump: |in hand|carries DA_Weapon|reloads [0-9]|strikes |staggered|Hollow.Loot.Dump: |broken open|picks up|finds [0-9]|FaceNearest(Crate|Pickup): |GoToNearestCrate|found |Hollow.AI.(Swarm|Nest): |flinches in|a brood of|leaves [0-9]+ x|Swarm: |Hollow.UI.Language: |Language (English|Russian)|Hollow.UI.Dump: (radio|subtitle|lore notice)|Radio of |Radio: cue|Hollow.Radio.(Fire|Reset)|Hollow.Lore.(Dump|Reset)|crew log|Crew log|datapad|Datapad|acoustic alert|Acoustic alert|answers Hollow.Noise|Hollow.AI.Noise: |Hollow.UI.Waypoints: |Hollow.AI.Director: |hears a noise|Solo drone: |Hollow.Drone.Dump: |FaceDrone: |hands it to |takes it back from |drone goes dark|Hollow.Perf: |Hollow.Art.Showcase: |Hollow.Graphics.Dump: " "$log" | grep -v "Command Line\|commandline\|metalmap" | strip | awk -v n="$SUMMARY_LINES" 'NR <= n'
done
if [[ "$MODE" == lantern ]]; then
	# Every player's Resonance in the host's four dumps (by name and pawn: local processes share a name): the carrier
	# gains, without the aura (1 -> 2) and in it (3 -> 4).
	echo "===== Resonance gained, without the lantern's aura and in it (host's dumps)"
	{ grep -hE "Hollow.Resonance.Dump: player" "$OUT/Host.log" || true; } | strip | awk '{ for (i = 1; i < NF; i++) if ($i == "player") { v[$(i + 1) " " $(i + 2)] = v[$(i + 1) " " $(i + 2)] " " $(i + 3) } }
		END { for (p in v) { n = split(v[p], a, " "); if (n >= 4) printf "%s: without +%.1f, in the aura +%.1f (x%.2f)\n", p, a[2] - a[1], a[4] - a[3], (a[2] > a[1]) ? (a[4] - a[3]) / (a[2] - a[1]) : 0 } }'
fi
# AK-14 (ADR-0053): a material drawn without its usage flag is the engine's default material in a cooked game.
echo "===== errors, ensures, crashes"
{ grep -hE "(LogHollow[A-Za-z]*|LogNet|LogAbilitySystem|LogOnline|LogGameMode|LogLoad|LogStreaming|LogScript): (Error|Warning)|Ensure condition failed|Assertion failed|Fatal error|appError|missing usage flag" "$OUT"/*.log || true; } | strip | sort | uniq -c | sort -rn | head -30   # none is no failure
echo "Logs: $OUT"
