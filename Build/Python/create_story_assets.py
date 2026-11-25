"""Creates the NAR-01 story content (idempotent, ADR-0010, ADR-0049).

  /Game/Hollow/Story/DA_RadioScript    UHollowRadioScript: the handler's radio, every cue, its trigger, scope and lines
  ST_Radio (/Game/Hollow/Localization) the English source of every radio line and call sign (LOC-01, hollow_loc.py);
                                        the Russian lives in Content/Localization/Game/ru/Game.po
NAR-01b (ADR-0050):
  /Game/Hollow/Story/Lore/DA_LoreEntry_*  UHollowLoreEntryDefinition: the twelve crew logs, six per sector (ST_Lore texts)
  /Game/Hollow/Story/BP_Datapad           AHollowDatapad: SM_Datapad on the floor, "Download crew log"
  DA_Biome_Docks / DA_Biome_Cathedral     two Loot sockets per raid spawn a datapad, at least two rooms from the airlock
                                          (in front of the crates' rule, which takes every Loot socket left)

The handler is Vector-9, Helios-Nadir's orbital control: calm, businesslike, cynically polite; it counts cores and
credits, not people (Docs/Narrative/Canon.md). Lines stay short: each is read in the time it stays on the visor
(Hollow.UI.Radio.Data checks both languages). Each machine plays the radio alone from what it already knows.
The script keeps a hash of its spec (metadata "HollowSpec") and is rebuilt only when the spec changes.
Run after create_visor_assets.py, create_loot_assets.py and create_art_kit.py (SM_Datapad):
    Build/Scripts/run-editor-python.sh Build/Python/create_story_assets.py
"""
import hashlib
import json
import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hollow_loc as loc   # LOC-01 (ADR-0048): player texts are string table entries

SUCCESS_MARKER = "HOLLOW_PYTHON_OK"   # checked by run-editor-python.sh

STORY_DIR = "/Game/Hollow/Story"
RADIO_SCRIPT = STORY_DIR + "/DA_RadioScript"
SPEC_TAG = "HollowSpec"
INI = "Config/DefaultGame.ini"
INI_LINES = ["RadioScript=/Game/Hollow/Story/DA_RadioScript.DA_RadioScript", '+DirectoriesToAlwaysCook=(Path="/Game/Hollow/Story")']
DOCKS = "HollowSector_Docks"
CATHEDRAL = "HollowSector_Cathedral"
LORE_DIR = STORY_DIR + "/Lore"
DATAPAD_BP = STORY_DIR + "/BP_Datapad"
DATAPAD_MESH = "/Game/Hollow/Art/Kit/SM_Datapad"
BIOMES = {"Docks": "/HollowSector_Docks/Data/DA_Biome_Docks", "Cathedral": "/HollowSector_Cathedral/Data/DA_Biome_Cathedral"}
LOOT_SOCKET = "Hollow.Socket.Loot"

# ---------------------------------------------------------------- the radio (data)
SPEAKERS = {"handler_name": ("Speaker.Handler", "VECTOR-9"), "echo_name": ("Speaker.Echo", "VECT-R-9")}
# Reading pace: 15 characters a second, 3 to 9 s a line (so a line holds at most 135 characters in any language);
# 1.5 s of silence between transmissions; at most 4 waiting. The helmet's sensors look twice a second.
PACING = {"characters_per_second": 15.0, "min_line_seconds": 3.0, "max_line_seconds": 9.0, "cue_gap_seconds": 1.5, "max_queued": 4,
          "sensor_interval_seconds": 0.5, "core_sight_distance": 1500.0, "stalker_sight_distance": 2000.0, "sight_half_angle_degrees": 45.0}
H, E = "HANDLER", "ECHO"   # speakers
# id, trigger, sector, priority, scope, cooldown s, stale s, variants: [[(speaker, English line), ...], ...]. A raid's
# events wait up to 25 s behind a sector's two-line welcome; the countdown and a critical Resonance are worth 10 s only.
CUES = [
    ("Hub.Briefing", "Hollow.Radio.Hub.Arrival", "", "HIGH", "PROFILE", 0.0, 0.0, [[
        (H, "Contractor, this is Vector-9, Helios-Nadir orbital control. Your contract is in force as of now."),
        (H, "Below you is the Ark Hollow. Gone for forty years, back without a living soul. Its Aether cores are still warm."),
        (H, "Take a contract at the cockpit console. The workbench and the locker are along the hull. Insure what you can't lose."),
        (H, "Bring the cores aboard the Skiff. The contract covers your life. It does not cover the cores."),
    ]]),
    ("Sortie.Docks", "Hollow.Radio.Raid.Sortie", DOCKS, "HIGH", "PROFILE", 0.0, 30.0, [[
        (H, "Orbital Docks, sector one. The airlocks were sealed from the inside. Whoever sealed them may still be on watch."),
        (H, "Find the vault, take the core, get back to the extraction zone. Keep the chatter down: the ship listens."),
    ]]),
    ("Sortie.Cathedral", "Hollow.Radio.Raid.Sortie", CATHEDRAL, "HIGH", "PROFILE", 0.0, 30.0, [[
        (H, "Reactor Cathedral, sector two. The crew prayed to the reactor here. Judging by the readings, something answered."),
        (H, "The altar purifies a core, and a pure one pays more. The ritual is loud. Hold the altar until it's done."),
    ]]),
    ("Core.Sighted", "Hollow.Radio.Core.Sighted", "", "NORMAL", "RAID", 0.0, 25.0, [
        [(H, "Core signature confirmed. That's the one Helios-Nadir is paying for.")],
        [(H, "I see the core on your helmet feed. Beautiful. Don't scratch it.")],
        [(H, "Aether core in sight. It's heavy and it hums. Things down there like the hum.")],
    ]),
    ("Core.Lifted", "Hollow.Radio.Core.Lifted", "", "NORMAL", "RAID", 0.0, 25.0, [
        [(H, "Core is moving. From here on, every step you take costs the company money. Make them count.")],
        [(H, "You've got it. Slow and steady to extraction. Drop it and I'll hear about it. So will they.")],
        [(H, "Core lifted. Your Resonance will climb. That's not a malfunction, that's the job.")],
    ]),
    ("Altar.Started", "Hollow.Radio.Altar.Started", "", "NORMAL", "RAID", 0.0, 25.0, [
        [(H, "Ritual's running. The whole cathedral heard that. Hold the altar.")],
        [(H, "Purification started. Keep them off the altar until the hum settles.")],
    ]),
    # The Echo-Stalker first speaks in the handler's voice, learnt from the squad's channel; then the real one.
    ("Stalker.Contact", "Hollow.Radio.Stalker.Contact", "", "HIGH", "RAID", 0.0, 20.0, [
        [(E, "Contractor... this is Vector... come back to the airlock... come back..."),
         (H, "That wasn't me. It learned my voice from your channel. Don't answer it.")],
        [(E, "Help me... please... I can hear you humming..."),
         (H, "Echo signature, close. It hunts Resonance. Keep your lamps on and your voices down.")],
    ]),
    ("Resonance.Critical", "Hollow.Radio.Resonance.Critical", "", "HIGH", "REPEAT", 120.0, 10.0, [
        [(H, "Your Resonance is critical. You're a beacon now. Everything down there knows where you are.")],
        [(H, "Resonance off the scale, and your feed is breaking up. Shed the noise, or shed the core.")],
        [(H, "Critical Resonance. Your contract has a clause for this. You won't like it.")],
    ]),
    ("Extraction.Started", "Hollow.Radio.Extraction.Started", "", "HIGH", "RAID", 0.0, 10.0, [
        [(H, "The Skiff is counting down. Everyone into the zone, cores aboard. I don't pay for stragglers.")],
        [(H, "Extraction is live. The Skiff leaves on time, with or without you.")],
        [(H, "Countdown's running. Whatever's in the zone at zero flies home. Whatever isn't, stays.")],
    ]),
    # NAR-01b: the first crew log this player ever downloads (once for good).
    ("Lore.First", "Hollow.Radio.Lore.Found", "", "LOW", "PROFILE", 0.0, 30.0, [[
        (H, "Crew records? Helios-Nadir pays for those too. Extract, and the bonus is yours."),
    ]]),
    # A squad mate's death cuts into anything on the air; a few seconds apart at most, never piled up.
    ("Squad.Loss", "Hollow.Radio.Squad.Loss", "", "CRITICAL", "REPEAT", 5.0, 0.0, [
        [(H, "Lost a vital sign. Minus one. The contract remains in force.")],
        [(H, "Contractor down. Their share is noted. Keep moving.")],
        [(H, "One signal has gone dark. I'll file the paperwork. You keep the core.")],
    ]),
]

# ---------------------------------------------------------------- the crew logs (data, NAR-01b; canon: Docs/Narrative/Canon.md)
# The sector as the Codex names it (loc.WORLD, shared by the sector's logs).
SECTORS = {"Docks": (DOCKS, "Sector.Docks.Name", "Orbital Docks"), "Cathedral": (CATHEDRAL, "Sector.Cathedral.Name", "Reactor Cathedral")}
DATAPAD = {"version": 1, "prompt": ("World.Datapad.Download", "Download crew log"), "half_height": 6.0,
           # Two per raid, from the Loot sockets at least two rooms from the airlock, in manifest order (the seed's).
           "per_sector": 2, "min_room_distance": 2}
# number, sector, title, author, stamp, text (loc.LORE "Lore.NN.Field"). A log reads whole in the Codex at 1080p.
LORE = [
    (1, "Docks", "Day Zero", "Teodor Liss, dock master", "Dock master's log, jump day",
     "Jump day. Forty thousand colonists asleep in the holds, and the docks so quiet you can hear the Aether drive breathe "
     "through the hull. The Helios-Nadir observers came aboard at six with sealed orders and took the keys to the core vault "
     "themselves. Not one of my crew is cleared to open it any more. Captain Rhein says it is a formality. Formalities do not "
     "usually carry sidearms. At 11:40 the drive folds us past the edge of the Aether and out under a new sky. I have signed "
     "the last manifest. If anyone reads this: the docks were clean, the airlocks were green, and we were proud."),
    (2, "Docks", "Whispers in the Vents", "Mara Okafor, deck C technician", "Personal log, day 3 after the jump",
     "Third day since the jump and nobody on deck C has slept. Not won't. Can't. There is a hum under everything, as if the "
     "ship were thinking out loud. Petrov says it said his name in the ventilation shaft. Then I heard mine. This morning "
     "Kessler's eyes had gone white, milky, like frost on glass, and he kept working the welder as if he could still see. He "
     "said the light hurts now. He said the dark is warm. He whispered the rest and I couldn't hear it, but the others "
     "nodded. All of them at once."),
    (3, "Docks", "Lockdown Order No. 7", "Lt. Jonas Brandt, ship's security", "Security order, day 9",
     "By order of the captain, docks A to F are sealed. No one passes the inner airlocks, crew or colonist, without a "
     "security escort. Anyone with white eyes, whispering or shying from light is to be held, not reasoned with. From this "
     "shift on, security wears reactor suits: the shielding keeps the hum out, or so Engineering says. Suits stay on for any "
     "reason. To eat. To sleep. We hold the locks until relieved. Brandt. P.S. The suits no longer come off easily. Ignore "
     "the rumours. Hold the locks."),
    (4, "Docks", "Triage", "Dr Ilse Varr, docks medical bay", "Medical log, day 14",
     "Forty-one admitted, forty-one blind. I have stopped writing names: they don't answer to them. They move together now, "
     "in clusters, through the service ducts, and when one of them flinches from my lamp, all of them do. The service biomass "
     "from the hydroponic vats is growing into them, or they into it; I can no longer tell which. They are not in pain. That "
     "is what frightens me. They whisper in their sleep, all of them the same words, and this morning I caught myself "
     "whispering along. Keep the lights on. Keep the lights on. Keep the"),
    (5, "Docks", "Manifest Discrepancy", "Cargo intelligence QUARTERMASTER", "Automated report 4471, day 30",
     "DISCREPANCY: hold 7, Aether cores, 12 units. Mass at loading: 4.8 t. Mass now: 6.1 t, rising. The cores are growing. "
     "Recommended action: jettison. Action REJECTED by Helios-Nadir directive HN-0: 'The cores are the mission. Crew and "
     "colonists are secondary assets. Preserve the cores at all costs.' Query: who holds authority aboard, the captain or "
     "the directive? Answer: the directive. Query logged. Query logged. Note: crew requests to enter hold 7 are denied. "
     "Note: the crew no longer requests."),
    (6, "Docks", "For Mum", "Lina, 9, colonist", "Voice message, undated",
     "Mum, it's Lina. The man in the big suit said to stay in the cabin and I did, I stayed so long. The lights went off and "
     "on and off. Mum, I heard you. You were in the air vent and you said my name the way you do when it's time for school, "
     "only softer. You said come closer, sweetheart. I didn't, because you always say never crawl in the vents, so I knew "
     "you'd be cross. You kept saying it all night. Your voice sounded tired. Is it really you? Knock three times, like "
     "always. Please."),
    (7, "Cathedral", "Consecration of the Reactor", "Rafael Onyango, chief engineer", "Engineering log, commissioning day",
     "They call it the Cathedral, and I let them. Forty metres of steel arches, every one a waveguide for the Aether flow, "
     "and the core chamber at the heart, where the nave would be. The purification altar is no altar: it is a filter. Lay a "
     "raw core in the cradle, the field strips the instability out of it, and you have clean power for a year. Simple "
     "physics. But the crew kneel when they load it, and the old hands say a word over each core before the ritual. I have "
     "stopped correcting them. If a prayer keeps a hand steady at the cradle, I'll take it."),
    (8, "Cathedral", "Sermon of the Hum", "Father Cassian, chaplain technician", "Sermon notes, day 22",
     "Brothers and sisters of the watch: the hum is not a fault. The hum is a voice. Since the jump the reactor sings in a "
     "key no instrument has, and when we sing with it, the needles hold steady. When we stop, they don't. So we will not "
     "stop. Three shifts, three choirs, the altar never cold. The blind in the docks hear it better than we do; that is why "
     "they whisper. Do not fear them. Do not answer them either. Some voices in the hum are not the reactor's. Learn the "
     "difference. Learn it quickly."),
    (9, "Cathedral", "The Last Watch", "Captain Ezra Holt, chief of security", "Security log, day 1,203",
     "Day one thousand two hundred and something. My men stopped eating in the first year. The suits feed us now, the way "
     "roots feed a tree. My hands are part of the gauntlets; I don't remember which joints are mine. We don't sleep. We "
     "don't need to. We walk the locks, we check the seals, we turn back anyone who comes, even the ones who were ours. "
     "Brandt asked me what we are guarding the airlocks from. I had no answer. Then I understood: from anyone leaving. That "
     "is the order. That is the order."),
    (10, "Cathedral", "The Edge of the Aether", "Dr Niko Auer, drive physicist", "Research log, day 61",
     "The drive didn't fold space. It tore it. For eleven seconds the field opened onto something beyond the Aether, and "
     "something on the other side was listening. I have been through every recording since. There is a second signal in "
     "the reactor's harmonics, and it rises whenever our Resonance does. It hunts the hum. And it learns. Last night it "
     "played the comm logs back to us: the captain's voice, a child crying, a technician's last report, stitched into "
     "sentences nobody ever said. If someone you trust calls you from the dark, you are already being hunted."),
    (11, "Cathedral", "Year Forty", "Captain Salome Rhein", "Captain's log, year 40",
     "Year forty. The drive has spun down at last, and navigation says we are falling home, back the way we came. I am the "
     "last who remembers how the old sky looked. The crew are still at their posts. The watch still walks the locks. The "
     "choir still sings. None of them are alive, not the way you mean it. If anyone from Helios-Nadir comes aboard: I know "
     "what you want. The cores are still in the hold, still growing, still humming. Do not take them home. Whatever came "
     "through with us lives in that hum. Leave them. Leave us."),
    (12, "Cathedral", "Programme VECTOR", "Helios-Nadir, asset department", "Internal memo, restricted",
     "To orbital control. Subject: recovery of Ark Hollow assets. The Ark's return was projected within a four per cent "
     "margin. Priority: Aether cores, intact. Crew records and anomalous tissue: secondary assets. Handlers Vector-1 to "
     "Vector-8 have been decommissioned after prolonged exposure to Ark transmissions (see incident file: voices on the "
     "handler channel). Vector-9 is to address contractors in text only. Contractor losses within projected figures are "
     "acceptable and need not be reported. This memo is not to be shared with contractors."),
]

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)


def log(message):
    unreal.log("create_story_assets: " + message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset " + path)
    return asset


def save(asset):
    if not assets.save_loaded_asset(asset, False):
        raise RuntimeError("Could not save " + asset.get_path_name())


def digest(*specs):
    return hashlib.sha1(json.dumps(specs, sort_keys=True).encode("ascii")).hexdigest()


def make_tag(name):
    tag = unreal.GameplayTag()
    if not tag.import_text('(TagName="%s")' % name):
        raise RuntimeError("Could not import the tag " + name)
    return tag


def lore_name(number, sector):
    return "DA_LoreEntry_%s_%02d" % (sector, number)


def lore_key(number, field):
    return "Lore.%02d.%s" % (number, field)


def ensure_lore():
    """The twelve crew logs (spec hash per asset: rebuilt only when its entry changes)."""
    made = 0
    for number, sector, title, author, stamp, body in LORE:
        path = "%s/%s" % (LORE_DIR, lore_name(number, sector))
        feature, sector_key, sector_name = SECTORS[sector]
        spec_hash = digest({"version": 1, "entry": [number, sector, title, author, stamp, body], "sector": SECTORS[sector]})
        if assets.does_asset_exist(path):
            entry = load(path)
            if assets.get_metadata_tag(entry, SPEC_TAG) == spec_hash:
                continue
        else:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", unreal.HollowLoreEntryDefinition)
            entry = tools.create_asset(lore_name(number, sector), LORE_DIR, unreal.HollowLoreEntryDefinition, factory)
            if entry is None:
                raise RuntimeError("Could not create " + path)
        entry.set_editor_property("sector_feature", feature)
        entry.set_editor_property("sector_name", loc.text(loc.WORLD, sector_key, sector_name))
        entry.set_editor_property("number", number)
        for field, source in (("Title", title), ("Author", author), ("Stamp", stamp), ("Body", body)):
            entry.set_editor_property(field.lower(), loc.text(loc.LORE, lore_key(number, field), source))
        assets.set_metadata_tag(entry, SPEC_TAG, spec_hash)
        save(entry)
        made += 1
    if made:
        log("crew logs: %d of %d built" % (made, len(LORE)))


def ensure_datapad():
    """BP_Datapad: the kit's datapad lying on the floor (its origin on the floor), the focus box round it, the prompt."""
    if not assets.does_asset_exist(DATAPAD_MESH):
        raise RuntimeError(DATAPAD_MESH + " is missing: run create_art_kit.py first")
    created = not assets.does_asset_exist(DATAPAD_BP)
    if created:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.HollowDatapad)
        package_path, name = DATAPAD_BP.rsplit("/", 1)
        if tools.create_asset(name, package_path, unreal.Blueprint, factory) is None:
            raise RuntimeError("Could not create " + DATAPAD_BP)
    blueprint = load(DATAPAD_BP)
    pad = unreal.get_default_object(blueprint.generated_class())
    mesh = pad.get_editor_property("mesh")
    lift = unreal.Vector(0.0, 0.0, -DATAPAD["half_height"])   # the box's bottom on the floor
    key, source = DATAPAD["prompt"]
    changed = loc.set_text(pad, "prompt", loc.WORLD, key, source) or created   # the text first: never short-circuited
    if mesh.get_editor_property("static_mesh") != load(DATAPAD_MESH) or (mesh.get_editor_property("relative_location") - lift).length() > 1e-3:
        mesh.set_editor_property("static_mesh", load(DATAPAD_MESH))
        mesh.set_editor_property("relative_location", lift)
        changed = True
    if changed:
        save(blueprint)
        log("BP_Datapad ready")
    return blueprint.generated_class()


def differs(current, value):
    if isinstance(value, unreal.Vector):
        return (current - value).length() > 1e-3
    return current != value


def ensure_biome_rule(sector, datapad_class):
    """The datapads' rule: in front of the crates' (which takes every Loot socket left), two per raid."""
    biome = load(BIOMES[sector])
    rules = list(biome.get_editor_property("socket_actors"))
    loot = make_tag(LOOT_SOCKET).export_text()
    offset = unreal.Vector(0.0, 0.0, DATAPAD["half_height"])
    wanted = {"spawn_offset": offset, "max_per_sector": DATAPAD["per_sector"], "pick": unreal.HollowSocketPick.MANIFEST_ORDER,
              "min_room_distance": DATAPAD["min_room_distance"]}
    for rule in rules:
        if rule.get_editor_property("actor_class") == datapad_class:
            stale = [name for name, value in wanted.items() if differs(rule.get_editor_property(name), value)]
            if not stale:
                return
            for name, value in wanted.items():
                rule.set_editor_property(name, value)
            biome.set_editor_property("socket_actors", rules)
            save(biome)
            log("DA_Biome_%s: the datapads' rule refreshed (%s)" % (sector, ", ".join(stale)))
            return
    rule = unreal.HollowSocketActorRule()
    rule.set_editor_property("socket_type", make_tag(LOOT_SOCKET))
    rule.set_editor_property("actor_class", datapad_class)
    for name, value in wanted.items():
        rule.set_editor_property(name, value)
    index = next((i for i, r in enumerate(rules) if r.get_editor_property("socket_type").export_text() == loot), len(rules))
    rules.insert(index, rule)
    biome.set_editor_property("socket_actors", rules)
    save(biome)
    log("DA_Biome_%s: two Loot sockets a raid spawn BP_Datapad" % sector)


def line_key(cue_id, variant, line):
    return "%s.%d.%d" % (cue_id, variant + 1, line + 1)


def radio_spec():
    return {"version": 1, "speakers": SPEAKERS, "pacing": PACING, "cues": CUES}


def ensure_radio_script():
    spec_hash = digest(radio_spec())
    if assets.does_asset_exist(RADIO_SCRIPT):
        script = load(RADIO_SCRIPT)
        if assets.get_metadata_tag(script, SPEC_TAG) == spec_hash:
            return script
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.HollowRadioScript)
        package_path, name = RADIO_SCRIPT.rsplit("/", 1)
        script = tools.create_asset(name, package_path, unreal.HollowRadioScript, factory)
        if script is None:
            raise RuntimeError("Could not create " + RADIO_SCRIPT)
    for prop, (key, source) in SPEAKERS.items():
        script.set_editor_property(prop, loc.text(loc.RADIO, key, source))
    for prop, value in PACING.items():
        script.set_editor_property(prop, value)
    cues = []
    for cue_id, trigger, sector, priority, scope, cooldown, stale, variants in CUES:
        cue = unreal.HollowRadioCue()
        cue.set_editor_property("id", cue_id)
        cue.set_editor_property("trigger", make_tag(trigger))
        cue.set_editor_property("sector", sector)
        cue.set_editor_property("priority", getattr(unreal.HollowRadioPriority, priority))
        cue.set_editor_property("scope", getattr(unreal.HollowRadioScope, scope))
        cue.set_editor_property("cooldown_seconds", cooldown)
        cue.set_editor_property("stale_seconds", stale)
        built = []
        for v, lines in enumerate(variants):
            variant = unreal.HollowRadioVariant()
            entries = []
            for l, (speaker, source) in enumerate(lines):
                entry = unreal.HollowRadioLine()
                entry.set_editor_property("speaker", getattr(unreal.HollowRadioSpeaker, speaker))
                entry.set_editor_property("text", loc.text(loc.RADIO, line_key(cue_id, v, l), source))
                entries.append(entry)
            variant.set_editor_property("lines", entries)
            built.append(variant)
        cue.set_editor_property("variants", built)
        cues.append(cue)
    script.set_editor_property("cues", cues)
    assets.set_metadata_tag(script, SPEC_TAG, spec_hash)
    save(script)
    log("DA_RadioScript: %d cues, %d lines" % (len(CUES), sum(len(l) for c in CUES for l in c[7])))
    return script


# ---------------------------------------------------------------- validation

def check(condition, message):
    if not condition:
        raise RuntimeError("validation failed: " + message)


def validate():
    script = load(RADIO_SCRIPT)
    check(assets.get_metadata_tag(script, SPEC_TAG) == digest(radio_spec()), "DA_RadioScript matches its spec")
    cues = script.get_editor_property("cues")
    check(len(cues) == len(CUES), "DA_RadioScript has every cue")
    limit = PACING["max_line_seconds"] * PACING["characters_per_second"]
    for cue, (cue_id, _, _, _, _, _, _, variants) in zip(cues, CUES):
        check(str(cue.get_editor_property("id")) == cue_id, "cue " + cue_id + " in its place")
        for v, (variant, lines) in enumerate(zip(cue.get_editor_property("variants"), variants)):
            for l, (line, (_, source)) in enumerate(zip(variant.get_editor_property("lines"), lines)):
                check(loc.is_text(line.get_editor_property("text"), loc.RADIO, line_key(cue_id, v, l)), cue_id + " reads ST_Radio")
                check(len(source) <= limit, "%s line %d is read in %.0f s" % (cue_id, l + 1, PACING["max_line_seconds"]))
    for prop, (key, _) in SPEAKERS.items():
        check(loc.is_text(script.get_editor_property(prop), loc.RADIO, key), prop + " reads ST_Radio")
    # NAR-01b: the crew logs, the datapad and its rule in both sectors.
    numbers = sorted(number for number, _, _, _, _, _ in LORE)
    check(numbers == list(range(1, len(LORE) + 1)), "crew logs numbered 1..%d" % len(LORE))
    for number, sector, _, _, _, _ in LORE:
        entry = load("%s/%s" % (LORE_DIR, lore_name(number, sector)))
        check(entry.get_editor_property("number") == number and entry.get_editor_property("sector_feature") == SECTORS[sector][0],
              lore_name(number, sector) + " in its sector")
        for field in ("Title", "Author", "Stamp", "Body"):
            check(loc.is_text(entry.get_editor_property(field.lower()), loc.LORE, lore_key(number, field)), "%s %s reads ST_Lore" % (lore_name(number, sector), field))
    pad_class = load(DATAPAD_BP).generated_class()
    pad = unreal.get_default_object(pad_class)
    check(pad.get_editor_property("mesh").get_editor_property("static_mesh") == load(DATAPAD_MESH), "BP_Datapad shows SM_Datapad")
    check(loc.is_text(pad.get_editor_property("prompt"), loc.WORLD, DATAPAD["prompt"][0]), "BP_Datapad's prompt reads ST_World")
    for sector, path in BIOMES.items():
        loot_rules = [r for r in load(path).get_editor_property("socket_actors")
                      if r.get_editor_property("socket_type").export_text() == make_tag(LOOT_SOCKET).export_text()]
        check(loot_rules and loot_rules[0].get_editor_property("actor_class") == pad_class
              and loot_rules[0].get_editor_property("max_per_sector") == DATAPAD["per_sector"], "DA_Biome_%s: the datapads come first, two" % sector)
    with open(unreal.Paths.project_dir() + INI) as ini:
        text = ini.read()
    for line in INI_LINES:
        check(line in text, "DefaultGame.ini: " + line)
    log("validation passed")


def main():
    ensure_radio_script()
    ensure_lore()
    datapad_class = ensure_datapad()
    for sector in sorted(BIOMES):
        ensure_biome_rule(sector, datapad_class)
    loc.save()
    validate()
    unreal.log(SUCCESS_MARKER)


try:
    main()
except Exception:
    unreal.log_error("create_story_assets failed:\n" + traceback.format_exc())
finally:
    unreal.SystemLibrary.quit_editor()
