#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "HollowNiagaraAuthoring.generated.h"

class UMaterialInterface;
class UNiagaraEmitter;
class UNiagaraScript;
class UNiagaraSystem;

/**
 * Editor-only helpers for building Niagara systems from the content scripts (AK-8, ADR-0043): editor Python cannot
 * add an emitter to a system, a module to an emitter's stack or change a module's constant. A system here is one
 * emitter copied from an engine template, with modules appended to its particle update; its constants (the stack's
 * values, rapid iteration parameters) are set by the end of their names ("GravityForce.Gravity").
 */
UCLASS()
class HOLLOWEDITOR_API UHollowNiagaraAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Makes System exactly one emitter named EmitterName, a copy of Template's exposed version, with UpdateModules
	 * appended in order to its particle update stack, and compiles it. False if anything is missing or does not compile.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Niagara")
	static bool BuildSingleEmitterSystem(UNiagaraSystem* System, UNiagaraEmitter* Template, const FString& EmitterName, const TArray<UNiagaraScript*>& UpdateModules);

	/** Every constant of the system and its emitters ("Constants.Emitter.Module.Input = value") and every module of the emitters. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Niagara")
	static TArray<FString> Describe(UNiagaraSystem* System);

	/**
	 * Sets every constant whose name ends with Suffix to Value, read for the constant's type: "1.5", "4", "true",
	 * "(X=0,Y=0,Z=1)", "(X=2,Y=8)", "(R=4,G=1.6,B=0.4,A=1)". Returns how many it set (0: no such constant or a bad value).
	 * Compile afterwards.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Niagara")
	static int32 SetConstant(UNiagaraSystem* System, const FString& Suffix, const FString& Value);

	/** The emitters' sprite renderers: this material, aligned with the particles' velocity (streaks) or facing the camera. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Niagara")
	static int32 SetSpriteRenderer(UNiagaraSystem* System, UMaterialInterface* Material, bool bVelocityAligned);

	/** Compiles the system and waits for it; false if a script does not compile. */
	UFUNCTION(BlueprintCallable, Category = "Hollow|Editor|Niagara")
	static bool Compile(UNiagaraSystem* System);
};
