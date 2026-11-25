#include "Hub/HollowCodexWidget.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Lore/HollowLoreEntryDefinition.h"
#include "Lore/HollowLoreRules.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowCodexWidget)

#define LOCTEXT_NAMESPACE "HollowCodex"

namespace HollowCodexPrivate
{
	FText Encrypted(const UHollowLoreEntryDefinition& Entry)
	{
		return FText::Format(LOCTEXT("Encrypted", "[DATA ENCRYPTED - {0}]"), Entry.SectorName);
	}
}

TArray<UPanelWidget*> UHollowCodexWidget::GetLists() const
{
	return {EntryList};
}

const UHollowLoreEntryDefinition* UHollowCodexWidget::FindEntry(const FPrimaryAssetId& Id) const
{
	const TObjectPtr<const UHollowLoreEntryDefinition>* Found = Entries.FindByPredicate([&Id](const UHollowLoreEntryDefinition* Entry)
	{
		return Entry && Entry->GetPrimaryAssetId() == Id;
	});
	return Found ? Found->Get() : nullptr;
}

void UHollowCodexWidget::RefreshRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms)
{
	if (Entries.IsEmpty())
	{
		for (const UHollowLoreEntryDefinition* Entry : UHollowLoreEntryDefinition::LoadAll())
		{
			Entries.Add(Entry);
		}
	}
	Known = State.Profile.Lore;
	TArray<FPrimaryAssetId> All;
	for (const UHollowLoreEntryDefinition* Entry : Entries)
	{
		All.Add(Entry->GetPrimaryAssetId());
	}
	FoundCount = HollowLoreRules::CountKnown(All, Known);

	// The logs by number, a sector's together under its name and found count.
	TArray<FHollowHubRow> Rows;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const UHollowLoreEntryDefinition& Entry = *Entries[Index];
		if (Index == 0 || Entries[Index - 1]->SectorFeature != Entry.SectorFeature)
		{
			int32 SectorTotal = 0;
			int32 SectorFound = 0;
			for (const UHollowLoreEntryDefinition* Other : Entries)
			{
				if (Other->SectorFeature == Entry.SectorFeature)
				{
					++SectorTotal;
					SectorFound += Known.Contains(Other->GetPrimaryAssetId()) ? 1 : 0;
				}
			}
			FHollowHubRow& Header = Rows.AddDefaulted_GetRef();
			Header.bHeader = true;
			Header.Name = FText::Format(LOCTEXT("Sector", "{0}   {1}/{2}"), Entry.SectorName, SectorFound, SectorTotal);
		}
		const bool bFound = Known.Contains(Entry.GetPrimaryAssetId());
		FHollowHubRow& Row = Rows.AddDefaulted_GetRef();
		Row.Name = bFound ? Entry.Title : HollowCodexPrivate::Encrypted(Entry);
		Row.Detail = bFound ? Entry.Author : FText::GetEmpty();
		FHollowHubAction& Read = Row.Actions.AddDefaulted_GetRef();
		Read.Label = LOCTEXT("Read", "Read");
		Read.Command = EHollowHubCommand::ReadLore;
		Read.Lore = Entry.GetPrimaryAssetId();
		Read.bAvailable = bFound;
		Read.Reason = LOCTEXT("NotFound", "not found yet");
	}
	FillList(EntryList, Rows);
	StatusText->SetText(FText::Format(LOCTEXT("Status", "Crew logs found: {0}/{1}"), FoundCount, Entries.Num()));
	if (!Reading.IsValid())
	{
		// The first log found, else the archive's hint.
		const TObjectPtr<const UHollowLoreEntryDefinition>* First = Entries.FindByPredicate([this](const UHollowLoreEntryDefinition* Entry)
		{
			return Known.Contains(Entry->GetPrimaryAssetId());
		});
		Reading = First ? (*First)->GetPrimaryAssetId() : FPrimaryAssetId();
	}
	ShowReader();
}

void UHollowCodexWidget::RunCommand(const FHollowHubAction& Action, const FText& RowName)
{
	if (Action.Command == EHollowHubCommand::ReadLore)
	{
		Read(Action.Lore);
	}
}

void UHollowCodexWidget::OnActionFocused(const FHollowHubAction& Action, const FText& RowName)
{
	if (Action.Command == EHollowHubCommand::ReadLore)
	{
		Read(Action.Lore);
	}
}

void UHollowCodexWidget::Read(const FPrimaryAssetId& Entry)
{
	if (FindEntry(Entry))
	{
		Reading = Entry;
		ShowReader();
	}
}

FText UHollowCodexWidget::GetReaderTitle() const
{
	const UHollowLoreEntryDefinition* Entry = FindEntry(Reading);
	if (!Entry)
	{
		return LOCTEXT("EmptyTitle", "CREW ARCHIVE");
	}
	return Known.Contains(Reading) ? Entry->Title : HollowCodexPrivate::Encrypted(*Entry);
}

FText UHollowCodexWidget::GetReaderMeta() const
{
	const UHollowLoreEntryDefinition* Entry = FindEntry(Reading);
	return Entry && Known.Contains(Reading) ? FText::Format(LOCTEXT("Meta", "{0}   |   {1}"), Entry->Author, Entry->Stamp) : FText::GetEmpty();
}

FText UHollowCodexWidget::GetReaderBody() const
{
	const UHollowLoreEntryDefinition* Entry = FindEntry(Reading);
	if (!Entry)
	{
		return LOCTEXT("EmptyBody", "No crew logs yet. Datapads lie in the Ark's rooms: download one (E) and the whole squad keeps its log. A new log pays a bonus when you extract.");
	}
	return Known.Contains(Reading) ? Entry->Body : FText::Format(LOCTEXT("EncryptedBody", "Find this datapad in {0} to decrypt the log."), Entry->SectorName);
}

void UHollowCodexWidget::ShowReader()
{
	ReaderTitle->SetText(GetReaderTitle());
	ReaderMeta->SetText(GetReaderMeta());
	ReaderBody->SetText(GetReaderBody());
}

#undef LOCTEXT_NAMESPACE
