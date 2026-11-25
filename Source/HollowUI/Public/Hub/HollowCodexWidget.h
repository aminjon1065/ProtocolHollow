#pragma once

#include "Hub/HollowHubScreenWidget.h"

#include "HollowCodexWidget.generated.h"

class UHollowLoreEntryDefinition;

/**
 * The crew archive's screen (W_Codex, NAR-01b, ADR-0050), at the hub's fourth terminal: the twelve crew logs by sector,
 * each sector's found count, the profile's total on the status line. A found log reads in full (title, author, stamp,
 * text) in the reader beside the list as the focus moves over it; one not found yet shows "[DATA ENCRYPTED - sector]".
 * Each player's own archive (the local profile). Event driven, no tick; keyboard and gamepad as every hub screen.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class HOLLOWUI_API UHollowCodexWidget : public UHollowHubScreenWidget
{
	GENERATED_BODY()

public:
	/** Shows Entry in the reader: in full if this profile found it, encrypted otherwise. */
	void Read(const FPrimaryAssetId& Entry);

	/** The log in the reader (invalid: the archive's hint). */
	const FPrimaryAssetId& GetReading() const { return Reading; }

	FText GetReaderTitle() const;
	FText GetReaderMeta() const;
	FText GetReaderBody() const;

	/** Found of all, as the status line says it. */
	int32 GetFoundCount() const { return FoundCount; }
	int32 GetTotalCount() const { return Entries.Num(); }

protected:
	virtual TArray<UPanelWidget*> GetLists() const override;
	virtual void RefreshRows(const FHollowProfileState& State, const FHollowEconomyTerms& Terms) override;
	virtual TArray<FPrimaryAssetId> GetNamedIds(const FHollowProfileState& State, const FHollowEconomyTerms& Terms) const override { return {}; }
	virtual void RunCommand(const FHollowHubAction& Action, const FText& RowName) override;
	virtual void OnActionFocused(const FHollowHubAction& Action, const FText& RowName) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> EntryList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ReaderTitle;

	/** Author and stamp. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ReaderMeta;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ReaderBody;

private:
	const UHollowLoreEntryDefinition* FindEntry(const FPrimaryAssetId& Id) const;
	void ShowReader();

	UPROPERTY(Transient)
	TArray<TObjectPtr<const UHollowLoreEntryDefinition>> Entries;

	/** The profile's found logs, as of the last refresh. */
	TArray<FPrimaryAssetId> Known;
	FPrimaryAssetId Reading;
	int32 FoundCount = 0;
};
