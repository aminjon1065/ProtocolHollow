#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "HollowVentNetworkSubsystem.generated.h"

class UHollowThreatConfig;

/** A link of the vent graph. */
struct FHollowVentLink
{
	int32 A = INDEX_NONE;
	int32 B = INDEX_NONE;
	float Seconds = 0.f;
};

/**
 * The vents as an abstract graph (Docs/Architecture/03 sec. 3.7, ADR-0019), server only: nodes are the topology's
 * vent openings, links join vents whose rooms are at most MaxRoomLinks portals apart, a trip costs SecondsPerLink per
 * room. No navmesh in the shafts: the Echo-Stalker disappears in one opening and comes out of another after the trip.
 */
UCLASS()
class HOLLOWAI_API UHollowVentNetworkSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHollowVentNetworkSubsystem* Get(const UObject* WorldContext);

	int32 NumNodes() const { return Nodes.Num(); }
	FVector GetNodeLocation(int32 Node) const;
	const TArray<FHollowVentLink>& GetLinks() const { return Links; }

	/** Nearest vent opening to Location, INDEX_NONE without vents. */
	int32 NearestNode(const FVector& Location) const;
	/** Shortest trip between two vents (Dijkstra), negative if they are not linked. */
	float TravelSeconds(int32 From, int32 To) const;
	/** Of the vents reachable from From, the one farthest from every location (ties: the lower index); From itself if alone. */
	int32 FarthestNodeFrom(int32 From, TConstArrayView<FVector> Locations) const;

	/** Rebuilds the graph from the current topology (the topology's change does it; tests call it directly). */
	void Rebuild();

	FString Describe() const;

	/** Tuning in use (UHollowThreatConfig::Get by default); tests swap in their own before the topology is set. */
	void SetConfig(const UHollowThreatConfig* InConfig) { Config = InConfig; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<const UHollowThreatConfig> Config;

	TArray<FVector> Nodes;
	TArray<FHollowVentLink> Links;
	FDelegateHandle TopologyHandle;
};
