#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "HollowStateTreeAuthoring.generated.h"

class UStateTree;

/**
 * Editor-only StateTree authoring for the content scripts (ADR-0010, ADR-0017): editor Python cannot build a tree's
 * states and instanced nodes, so Build/Python/create_pilgrim_assets.py describes the tree as JSON and these calls
 * build, compile and describe it. Spec of a state:
 *   {"name": "Root", "conditions": ["HollowSTCond_HasObstacle"], "tasks": ["HollowSTTask_Breach"],
 *    "transitions": [{"trigger": "OnEvent", "event": "Hollow.AI.Event.Reconsider", "target": "Root"}], "children": [...]}
 * Nodes are the Hollow AI StateTree nodes by struct name (they take no parameters: tuning lives in the definitions).
 */
UCLASS()
class HOLLOWEDITOR_API UHollowStateTreeAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** A new, unsaved StateTree asset with the StateTree AI component schema. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|StateTree")
	static UStateTree* CreateAIStateTree(const FString& PackagePath, const FString& AssetName);

	/**
	 * Replaces every state of the tree with the JSON spec (one root state) and compiles it. Returns what went wrong, or
	 * an empty string (Python turns a false bool with out-parameters into None, so the error is the result).
	 */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|StateTree")
	static FString BuildStateTree(UStateTree* StateTree, const FString& Spec);

	/** The tree's root state in the spec format, for scripts that rebuild only on change; empty without one. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|StateTree")
	static FString DescribeStateTree(const UStateTree* StateTree);
};
