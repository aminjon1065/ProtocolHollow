#include "Tags/HollowGameplayTags.h"

#include "Tags/HollowTagRules.h"

namespace HollowTags
{
	// Abilities
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Tool, "Hollow.Ability.Tool", "Parent of all signature tool abilities.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Tool_PlasmaCutter, "Hollow.Ability.Tool.PlasmaCutter", "Engineer plasma cutter: cuts locks, seams and welds.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Tool_Welder, "Hollow.Ability.Tool.Welder", "Engineer welder: seals doors.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Tool_AetherLantern, "Hollow.Ability.Tool.AetherLantern", "Techno-Occultist Aether lantern (S6-02): exposes enemies in its cone, dampens the Resonance of the squad around.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Tool_Refuel, "Hollow.Ability.Tool.Refuel", "Burns a fuel cell into the tool tank (S5-01): blocked like the tools while carrying or dead.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Interact, "Hollow.Ability.Interact", "Generic interaction ability.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Carry_Heavy, "Hollow.Ability.Carry.Heavy", "Pick up / carry a heavy Aether core.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Gadget_PulseMine, "Hollow.Ability.Gadget.PulseMine", "Kinetic pulse mine (stuns the Echo-Stalker).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Gadget_OccultWard, "Hollow.Ability.Gadget.OccultWard", "Techno-Occultist ritual ward (S6-03): a rune circle on the floor that slows enemies and calms the players inside.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_HelmetLamp, "Hollow.Ability.HelmetLamp", "Helmet lamp toggle (S4-08): predicted, the Echo-Stalker keeps out of a lit cone only.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Enemy, "Hollow.Ability.Enemy", "Parent of enemy abilities (server only).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Enemy_Pilgrim_Breach, "Hollow.Ability.Enemy.Pilgrim.Breach", "Iron Pilgrim: structural blow at a door or bulkhead.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Enemy_Pilgrim_Melee, "Hollow.Ability.Enemy.Pilgrim.Melee", "Iron Pilgrim: telegraphed melee strike.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Enemy_Stalker_Ambush, "Hollow.Ability.Enemy.Stalker.Ambush", "Echo-Stalker: strike out of an ambush.");

	// Cooldowns
	HOLLOW_DEFINE_GAMEPLAY_TAG(Cooldown_Gadget, "Hollow.Cooldown.Gadget", "Any gadget recharging (S6-03): every gadget ability is blocked by it, so one press never spends two.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Cooldown_Gadget_PulseMine, "Hollow.Cooldown.Gadget.PulseMine", "Pulse mine recharging: a player places no new one meanwhile.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Cooldown_Gadget_OccultWard, "Hollow.Cooldown.Gadget.OccultWard", "Ritual ward recharging: a player places no new one meanwhile.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Cooldown_HelmetLamp, "Hollow.Cooldown.HelmetLamp", "Helmet lamp switched a moment ago: no strobing.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Cooldown_Tool_Refuel, "Hollow.Cooldown.Tool.Refuel", "A fuel cell went in a moment ago: a held key burns no second one.");

	// Gameplay events (ability triggers)
	HOLLOW_DEFINE_GAMEPLAY_TAG(Event_Enemy, "Hollow.Event.Enemy", "Parent of the gameplay events that trigger enemy abilities.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Event_Enemy_Breach, "Hollow.Event.Enemy.Breach", "Brain asks for a structural blow; payload target: the obstacle.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Event_Enemy_Melee, "Hollow.Event.Enemy.Melee", "Brain asks for a melee strike; payload target: the victim.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Event_Enemy_Ambush, "Hollow.Event.Enemy.Ambush", "Brain springs an ambush; payload target: the victim.");

	// AI brain (StateTree) events
	HOLLOW_DEFINE_GAMEPLAY_TAG(AI_Event_Reconsider, "Hollow.AI.Event.Reconsider", "Brain facts changed (target, stimulus, obstacle): pick the state again.");

	// States
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_CarryingHeavy, "Hollow.State.CarryingHeavy", "Carrying a heavy object (any mode).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_CarryingHeavy_Solo, "Hollow.State.CarryingHeavy.Solo", "Carrying a heavy object alone.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_CarryingHeavy_Coop, "Hollow.State.CarryingHeavy.Coop", "Carrying a heavy object with a teammate.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Resonance_Low, "Hollow.State.Resonance.Low", "Squad Resonance tier low (S4-08: every player, mirrored on each machine from the replicated squad tier).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Resonance_Medium, "Hollow.State.Resonance.Medium", "Squad Resonance tier medium (every player).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Resonance_High, "Hollow.State.Resonance.High", "Squad Resonance tier high (every player; the visor alerts).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Resonance_Critical, "Hollow.State.Resonance.Critical", "Squad Resonance tier critical (every player; the visor alerts).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Stunned, "Hollow.State.Stunned", "Actor is stunned.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Downed, "Hollow.State.Downed", "Player is downed and can be revived.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Dead, "Hollow.State.Dead", "Actor is dead.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Tool_Overheated, "Hollow.State.Tool.Overheated", "Tool overheated: blocks tool activation.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Tool_Active, "Hollow.State.Tool.Active", "A continuous tool is running (owned while active; predicted locally).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Invulnerable, "Hollow.State.Invulnerable", "Takes no damage of any type (exact tag only; dev: Hollow.Dev.Invulnerable).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Invulnerable_Kinetic, "Hollow.State.Invulnerable.Kinetic", "Immune to kinetic damage (Echo-Stalker).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Exposed, "Hollow.State.Exposed", "In the Aether lantern's light (GE_Occult_Expose): slowed, weaker armour, no kinetic immunity; the Echo-Stalker flees.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_AntiResonance, "Hollow.State.AntiResonance", "Within an Aether lantern's aura (GE_AntiResonance): Resonance builds slower.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Warded, "Hollow.State.Warded", "Inside a ritual ward (S6-03): an enemy is slowed (GE_Ward_Bind), a player's Resonance builds slower and drains (GE_Ward_Calm).");

	// Damage types
	HOLLOW_DEFINE_GAMEPLAY_TAG(Damage_Kinetic, "Hollow.Damage.Kinetic", "Kinetic damage.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Damage_Occult, "Hollow.Damage.Occult", "Occult (Aether) damage.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Damage_Thermal, "Hollow.Damage.Thermal", "Thermal damage (fire, cryo).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Damage_Structural, "Hollow.Damage.Structural", "Structural damage to doors, bulkheads and barricades.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Damage_Electric, "Hollow.Damage.Electric", "Electric damage (the Reactor Cathedral's arcs, S7-02).");

	// Noise
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Voice, "Hollow.Noise.Voice", "Proximity voice chat loudness (parent of the tiers; players hear voices through VOIP).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Voice_Whisper, "Hollow.Noise.Voice.Whisper", "A transmitted voice at whisper level (S4-07).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Voice_Speech, "Hollow.Noise.Voice.Speech", "A transmitted voice at speaking level (S4-07).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Voice_Shout, "Hollow.Noise.Voice.Shout", "A transmitted voice at shouting level (S4-07).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Footstep, "Hollow.Noise.Footstep", "Footsteps.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Tool_Cutter, "Hollow.Noise.Tool.Cutter", "Plasma cutter in use.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Tool_Welder, "Hollow.Noise.Tool.Welder", "Welder in use.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Tool_Lantern, "Hollow.Noise.Tool.Lantern", "Aether lantern in use: a quiet hum.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Impact_Heavy, "Hollow.Noise.Impact.Heavy", "Heavy physical impact.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Structural_Breach, "Hollow.Noise.Structural.Breach", "Door or bulkhead breached.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Core_Drop, "Hollow.Noise.Core.Drop", "Heavy Aether core dropped.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Ritual, "Hollow.Noise.Ritual", "The Purification Altar's ritual: a bell toll, loud on purpose (S7-02).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise, "Hollow.Noise", "Parent of noises the AI hears (DA_NoiseConfig) and players hear (DA_SoundBank).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Sound, "Hollow.Sound", "Parent of sounds no AI hears (or heard as a linked noise): DA_SoundBank.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Sound_Door_Servo, "Hollow.Sound.Door.Servo", "A door opening or closing.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Sound_Footstep_Heavy, "Hollow.Sound.Footstep.Heavy", "Iron Pilgrim footstep.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Sound_Footstep_Stalker, "Hollow.Sound.Footstep.Stalker", "Echo-Stalker footstep (barely there).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Sound_Enemy_PilgrimWindup, "Hollow.Sound.Enemy.PilgrimWindup", "Iron Pilgrim winding up a blow.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Sound_Enemy_StalkerShriek, "Hollow.Sound.Enemy.StalkerShriek", "Echo-Stalker about to strike.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Sound_Stalker_Vent, "Hollow.Sound.Stalker.Vent", "Echo-Stalker slipping into or out of a vent.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Sound_Mine_Arm, "Hollow.Sound.Mine.Arm", "Pulse mine armed.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Sound_Mine_Blast, "Hollow.Sound.Mine.Blast", "Pulse mine going off (heard by the AI as Impact.Heavy).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Sound_Core_Hum, "Hollow.Sound.Core.Hum", "Aether core hum (loop).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Sound_HelmetLamp_Toggle, "Hollow.Sound.HelmetLamp.Toggle", "Helmet lamp click (players only; the AI does not hear it).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Sound_Ward_Hum, "Hollow.Sound.Ward.Hum", "Ritual ward hum (loop while it burns; players only, the AI does not hear it).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Sound_Arc_Telegraph, "Hollow.Sound.Arc.Telegraph", "An electric arc's warning crackle (S7-03; players only, the AI does not hear it).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Sound_Arc_Discharge, "Hollow.Sound.Arc.Discharge", "An electric arc striking (S7-03; players only).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Sound_Altar_Purified, "Hollow.Sound.Altar.Purified", "The Purification Altar's ritual done: the core purified (S7-03; players only).");

	// Input
	HOLLOW_DEFINE_GAMEPLAY_TAG(Input_Move, "Hollow.Input.Move", "Move input.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Input_Look, "Hollow.Input.Look", "Look input.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Input_Jump, "Hollow.Input.Jump", "Jump input.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Input_PrimaryUse, "Hollow.Input.PrimaryUse", "Primary tool use.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Input_SecondaryUse, "Hollow.Input.SecondaryUse", "Secondary tool use.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Input_Interact, "Hollow.Input.Interact", "Interact input.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Input_Sprint, "Hollow.Input.Sprint", "Sprint input.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Input_Crouch, "Hollow.Input.Crouch", "Crouch input.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Input_Gadget, "Hollow.Input.Gadget", "Gadget input.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Input_PushToTalk, "Hollow.Input.PushToTalk", "Push-to-talk (held): transmits the voice (S4-07).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Input_HelmetLamp, "Hollow.Input.HelmetLamp", "Helmet lamp on / off (S4-08).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Input_Reload, "Hollow.Input.Reload", "Refuel the tools from a fuel cell (S5-01).");

	// Sector rooms and sockets
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type, "Hollow.Room.Type", "Parent of room types; every room definition carries exactly one child.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type_Spawn, "Hollow.Room.Type.Spawn", "Raid start: player spawn sockets.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type_Extraction, "Hollow.Room.Type.Extraction", "Extraction point (shuttle pad).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type_CoreVault, "Hollow.Room.Type.CoreVault", "Holds the heavy Aether core.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type_Hangar, "Hollow.Room.Type.Hangar", "Large hub room with exits on every side.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type_Corridor, "Hollow.Room.Type.Corridor", "Connector piece: straight or corner corridor.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type_Junction, "Hollow.Room.Type.Junction", "Branching piece (T or cross junction).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type_Storage, "Hollow.Room.Type.Storage", "Optional side room with loot.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type_Hub, "Hollow.Room.Type.Hub", "The hub shuttle Skiff (S5-03): not a sector room, its light budget only.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket, "Hollow.Socket", "Parent of room socket types.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket_PlayerSpawn, "Hollow.Socket.PlayerSpawn", "Player start of the raid.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket_Loot, "Hollow.Socket.Loot", "Server-spawned loot container.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket_CoreSpawn, "Hollow.Socket.CoreSpawn", "Heavy Aether core spawn point.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket_EnemySpawn, "Hollow.Socket.EnemySpawn", "Enemy spawn point.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket_VentNode, "Hollow.Socket.VentNode", "Echo-Stalker vent graph node.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket_Extraction, "Hollow.Socket.Extraction", "Extraction zone.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket_Altar, "Hollow.Socket.Altar", "Purification Altar (Reactor Cathedral, S7-02).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket_ArcHazard, "Hollow.Socket.ArcHazard", "Electric arc across a passage, with its breaker (Reactor Cathedral, S7-02).");

	// Item types
	HOLLOW_DEFINE_GAMEPLAY_TAG(Item_Type, "Hollow.Item.Type", "Parent of item types; every item definition carries exactly one child.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Item_Type_Tool, "Hollow.Item.Type.Tool", "Signature tool (welder, plasma cutter).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Item_Type_Consumable, "Hollow.Item.Type.Consumable", "Consumable (fuel cell, stim).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Item_Type_AetherCore, "Hollow.Item.Type.AetherCore", "Heavy Aether core: sold to the corporation on extraction.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Item_Type_Material, "Hollow.Item.Type.Material", "Crafting material (S5-02): bought or salvaged, used on the workbench, never taken into a raid.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Item_Type_Relic, "Hollow.Item.Type.Relic", "Occult relic (S6-04): crafted, taken into a raid, its effect lasts while it is in the raid inventory.");

	// Equip slots
	HOLLOW_DEFINE_GAMEPLAY_TAG(Item_Slot, "Hollow.Item.Slot", "Parent of equip slots: a loadout holds at most a few different items of each (S6-04).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Item_Slot_Primary, "Hollow.Item.Slot.Primary", "On the primary use input (plasma cutter, Aether lantern).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Item_Slot_Secondary, "Hollow.Item.Slot.Secondary", "On the secondary use input (welder).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Item_Slot_Gadget, "Hollow.Item.Slot.Gadget", "On the gadget input (pulse mine, ritual ward).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Item_Slot_Relic, "Hollow.Item.Slot.Relic", "A relic slot (one, two with the Reliquary).");

	// Player classes
	HOLLOW_DEFINE_GAMEPLAY_TAG(Class, "Hollow.Class", "Parent of player classes; a player's ability system carries its class's child.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Class_Engineer, "Hollow.Class.Engineer", "Engineer: cutter, welder and mines; hotter tools, heavier plating.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Class_Occultist, "Hollow.Class.Occultist", "Techno-Occultist: the Aether lantern and wards; slower Resonance.");

	// SetByCaller
	HOLLOW_DEFINE_GAMEPLAY_TAG(Data_Damage, "Hollow.Data.Damage", "SetByCaller: damage magnitude.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Data_ResonanceGain, "Hollow.Data.ResonanceGain", "SetByCaller: Resonance gain magnitude.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Data_FuelCost, "Hollow.Data.FuelCost", "SetByCaller: tool fuel cost per period.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Data_HeatGain, "Hollow.Data.HeatGain", "SetByCaller: tool heat added by one sample.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Data_ResonanceDrain, "Hollow.Data.ResonanceDrain", "SetByCaller: Resonance change of one drain (negative: taken away, never through the gain multiplier).");

	// GameplayCues
	HOLLOW_DEFINE_GAMEPLAY_TAG(GameplayCue_Tool_Beam, "GameplayCue.Hollow.Tool.Beam", "Looping tool beam (cutter/welder) cosmetics.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(GameplayCue_Tool_Lantern, "GameplayCue.Hollow.Tool.Lantern", "Aether lantern light cone cosmetics (while it runs).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(GameplayCue_Status_Exposed, "GameplayCue.Hollow.Status.Exposed", "Glow on an enemy in the Aether lantern's light (GE_Occult_Expose).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(GameplayCue_Enemy_Windup, "GameplayCue.Hollow.Enemy.Windup", "Enemy attack wind-up telegraph (while the blow is coming).");
}
