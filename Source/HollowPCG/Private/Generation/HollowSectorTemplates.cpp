#include "Generation/HollowSectorTemplates.h"

#include "Data/HollowRoomDefinition.h"

TOptional<EHollowRoomRole> HollowSectorTemplates::ResolveRole(const FGameplayTagContainer& RoomTags, const FHollowSectorGenRules& Rules)
{
	if (Rules.SpawnRoomType.IsValid() && RoomTags.HasTagExact(Rules.SpawnRoomType))
	{
		return EHollowRoomRole::Spawn;
	}
	if (Rules.ExtractionRoomType.IsValid() && RoomTags.HasTagExact(Rules.ExtractionRoomType))
	{
		return EHollowRoomRole::Extraction;
	}
	if (Rules.CoreVaultRoomType.IsValid() && RoomTags.HasTagExact(Rules.CoreVaultRoomType))
	{
		return EHollowRoomRole::CoreVault;
	}
	if (RoomTags.HasAnyExact(Rules.PathRoomTypes))
	{
		return EHollowRoomRole::Path;
	}
	if (RoomTags.HasAnyExact(Rules.BranchRoomTypes))
	{
		return EHollowRoomRole::Branch;
	}
	return {};
}

TOptional<FHollowRoomTemplate> HollowSectorTemplates::MakeTemplate(const UHollowRoomDefinition& Room, const FHollowSectorGenRules& Rules)
{
	const TOptional<EHollowRoomRole> Role = ResolveRole(Room.RoomTags, Rules);
	if (!Role.IsSet())
	{
		return {};
	}

	FHollowRoomTemplate Template;
	Template.Id = Room.GetPrimaryAssetId().ToString();
	Template.Role = Role.GetValue();
	Template.Footprint = FIntPoint(Room.Footprint.X, Room.Footprint.Y);   // the sector is one floor: tall halls only rise (ADR-0038)
	for (const FHollowRoomConnector& Connector : Room.Connectors)
	{
		FHollowTemplateConnector& Plain = Template.Connectors.AddDefaulted_GetRef();
		Plain.Cell = FIntPoint(Connector.Cell.X, Connector.Cell.Y);
		Plain.Side = Connector.Side;
		Plain.Kinds = Connector.AllowedKinds;
	}
	return Template;
}

TArray<FHollowRoomTemplate> HollowSectorTemplates::MakeTemplates(TConstArrayView<const UHollowRoomDefinition*> Rooms, const FHollowSectorGenRules& Rules)
{
	TArray<FHollowRoomTemplate> Templates;
	for (const UHollowRoomDefinition* Room : Rooms)
	{
		if (Room)
		{
			if (TOptional<FHollowRoomTemplate> Template = MakeTemplate(*Room, Rules))
			{
				Templates.Add(MoveTemp(Template.GetValue()));
			}
		}
	}
	return Templates;
}
