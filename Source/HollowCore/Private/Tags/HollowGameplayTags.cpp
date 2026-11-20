#include "Tags/HollowGameplayTags.h"

#include "Tags/HollowTagRules.h"

namespace HollowTags
{
	// Abilities
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Tool, "Hollow.Ability.Tool", "Parent of all signature tool abilities.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Tool_PlasmaCutter, "Hollow.Ability.Tool.PlasmaCutter", "Engineer plasma cutter: cuts locks, seams and welds.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Tool_Welder, "Hollow.Ability.Tool.Welder", "Engineer welder: seals doors.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Interact, "Hollow.Ability.Interact", "Generic interaction ability.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Carry_Heavy, "Hollow.Ability.Carry.Heavy", "Pick up / carry a heavy Aether core.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Gadget_PulseMine, "Hollow.Ability.Gadget.PulseMine", "Kinetic pulse mine (stuns the Echo-Stalker).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Enemy, "Hollow.Ability.Enemy", "Parent of enemy abilities (server only).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Enemy_Pilgrim_Breach, "Hollow.Ability.Enemy.Pilgrim.Breach", "Iron Pilgrim: structural blow at a door or bulkhead.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Enemy_Pilgrim_Melee, "Hollow.Ability.Enemy.Pilgrim.Melee", "Iron Pilgrim: telegraphed melee strike.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Ability_Enemy_Stalker_Ambush, "Hollow.Ability.Enemy.Stalker.Ambush", "Echo-Stalker: strike out of an ambush.");

	// Cooldowns
	HOLLOW_DEFINE_GAMEPLAY_TAG(Cooldown_Gadget_PulseMine, "Hollow.Cooldown.Gadget.PulseMine", "Pulse mine recharging: a player places no new one meanwhile.");

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
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Resonance_Low, "Hollow.State.Resonance.Low", "Resonance tier: low.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Resonance_Medium, "Hollow.State.Resonance.Medium", "Resonance tier: medium.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Resonance_High, "Hollow.State.Resonance.High", "Resonance tier: high.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Resonance_Critical, "Hollow.State.Resonance.Critical", "Resonance tier: critical.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Stunned, "Hollow.State.Stunned", "Actor is stunned.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Downed, "Hollow.State.Downed", "Player is downed and can be revived.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Dead, "Hollow.State.Dead", "Actor is dead.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Tool_Overheated, "Hollow.State.Tool.Overheated", "Tool overheated: blocks tool activation.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Tool_Active, "Hollow.State.Tool.Active", "A continuous tool is running (owned while active; predicted locally).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(State_Invulnerable_Kinetic, "Hollow.State.Invulnerable.Kinetic", "Immune to kinetic damage (Echo-Stalker).");

	// Damage types
	HOLLOW_DEFINE_GAMEPLAY_TAG(Damage_Kinetic, "Hollow.Damage.Kinetic", "Kinetic damage.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Damage_Occult, "Hollow.Damage.Occult", "Occult (Aether) damage.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Damage_Thermal, "Hollow.Damage.Thermal", "Thermal damage (fire, cryo).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Damage_Structural, "Hollow.Damage.Structural", "Structural damage to doors, bulkheads and barricades.");

	// Noise
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Voice, "Hollow.Noise.Voice", "Proximity voice chat loudness.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Footstep, "Hollow.Noise.Footstep", "Footsteps.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Tool_Cutter, "Hollow.Noise.Tool.Cutter", "Plasma cutter in use.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Tool_Welder, "Hollow.Noise.Tool.Welder", "Welder in use.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Impact_Heavy, "Hollow.Noise.Impact.Heavy", "Heavy physical impact.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Structural_Breach, "Hollow.Noise.Structural.Breach", "Door or bulkhead breached.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Noise_Core_Drop, "Hollow.Noise.Core.Drop", "Heavy Aether core dropped.");

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

	// Sector rooms and sockets
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type, "Hollow.Room.Type", "Parent of room types; every room definition carries exactly one child.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type_Spawn, "Hollow.Room.Type.Spawn", "Raid start: player spawn sockets.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type_Extraction, "Hollow.Room.Type.Extraction", "Extraction point (shuttle pad).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type_CoreVault, "Hollow.Room.Type.CoreVault", "Holds the heavy Aether core.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type_Hangar, "Hollow.Room.Type.Hangar", "Large hub room with exits on every side.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type_Corridor, "Hollow.Room.Type.Corridor", "Connector piece: straight or corner corridor.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type_Junction, "Hollow.Room.Type.Junction", "Branching piece (T or cross junction).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Room_Type_Storage, "Hollow.Room.Type.Storage", "Optional side room with loot.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket, "Hollow.Socket", "Parent of room socket types.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket_PlayerSpawn, "Hollow.Socket.PlayerSpawn", "Player start of the raid.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket_Loot, "Hollow.Socket.Loot", "Server-spawned loot container.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket_CoreSpawn, "Hollow.Socket.CoreSpawn", "Heavy Aether core spawn point.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket_EnemySpawn, "Hollow.Socket.EnemySpawn", "Enemy spawn point.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket_VentNode, "Hollow.Socket.VentNode", "Echo-Stalker vent graph node.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Socket_Extraction, "Hollow.Socket.Extraction", "Extraction zone.");

	// Item types
	HOLLOW_DEFINE_GAMEPLAY_TAG(Item_Type, "Hollow.Item.Type", "Parent of item types; every item definition carries exactly one child.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Item_Type_Tool, "Hollow.Item.Type.Tool", "Signature tool (welder, plasma cutter).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Item_Type_Consumable, "Hollow.Item.Type.Consumable", "Consumable (fuel cell, stim).");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Item_Type_AetherCore, "Hollow.Item.Type.AetherCore", "Heavy Aether core: sold to the corporation on extraction.");

	// SetByCaller
	HOLLOW_DEFINE_GAMEPLAY_TAG(Data_Damage, "Hollow.Data.Damage", "SetByCaller: damage magnitude.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Data_ResonanceGain, "Hollow.Data.ResonanceGain", "SetByCaller: Resonance gain magnitude.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Data_FuelCost, "Hollow.Data.FuelCost", "SetByCaller: tool fuel cost per period.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(Data_HeatGain, "Hollow.Data.HeatGain", "SetByCaller: tool heat added by one sample.");

	// GameplayCues
	HOLLOW_DEFINE_GAMEPLAY_TAG(GameplayCue_Tool_Beam, "GameplayCue.Hollow.Tool.Beam", "Looping tool beam (cutter/welder) cosmetics.");
	HOLLOW_DEFINE_GAMEPLAY_TAG(GameplayCue_Enemy_Windup, "GameplayCue.Hollow.Enemy.Windup", "Enemy attack wind-up telegraph (while the blow is coming).");
}
