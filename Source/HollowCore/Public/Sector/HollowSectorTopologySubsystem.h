#pragma once

#include "Sector/HollowSectorTopology.h"
#include "Subsystems/WorldSubsystem.h"

#include "HollowSectorTopologySubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnHollowTopologyChanged);

/** The current sector's topology (ADR-0019): set by the server once the sector is assembled, read by the AI. */
UCLASS()
class HOLLOWCORE_API UHollowSectorTopologySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UHollowSectorTopologySubsystem* Get(const UObject* WorldContext);

	const FHollowSectorTopology& GetTopology() const { return Topology; }
	void SetTopology(const FHollowSectorTopology& InTopology);

	FOnHollowTopologyChanged OnTopologyChanged;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UPROPERTY(Transient)
	FHollowSectorTopology Topology;
};
