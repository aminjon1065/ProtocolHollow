#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "HollowEnvQueryAuthoring.generated.h"

class UEnvQuery;

/**
 * Editor-only EQS authoring for the content scripts (ADR-0010, ADR-0019): editor Python cannot build a query's options,
 * generators and tests, so Build/Python/create_stalker_assets.py describes the query as JSON and these calls build and
 * check it. Spec:
 *   {"options": [{"generator": {"class": "/Script/HollowAI.HollowEnvQueryGenerator_FocusRoomPoints",
 *                               "properties": {"PointSpacing": "100.0"}},
 *                 "tests": [{"class": "/Script/AIModule.EnvQueryTest_Distance", "properties": {"TestPurpose": "Score"}}]}]}
 * Property values are Unreal text (what the details panel's copy gives); tests run in their listed order.
 */
UCLASS()
class HOLLOWEDITOR_API UHollowEnvQueryAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** A new, unsaved and empty EQS query asset. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|EQS")
	static UEnvQuery* CreateEnvQuery(const FString& PackagePath, const FString& AssetName);

	/**
	 * Replaces every option of the query with the JSON spec (the editor graph is rebuilt from it when the asset is opened).
	 * Returns what went wrong, or an empty string (Python turns a false bool with out-parameters into None).
	 */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|EQS")
	static FString BuildEnvQuery(UEnvQuery* Query, const FString& Spec);

	/** Whether the query already is the spec (same classes in order, the listed properties equal), for idempotent scripts. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|EQS")
	static bool EnvQueryMatches(const UEnvQuery* Query, const FString& Spec);
};
