#include "Validation/HollowArtBudget.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/Level.h"
#include "Engine/Texture.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HollowArtBudget)

int32 HollowArtBudget::CookedMaxSize(const UTexture& Texture)
{
	int32 Size = FMath::Max(Texture.Source.GetSizeX(), Texture.Source.GetSizeY());
	if (Texture.MaxTextureSize > 0)
	{
		Size = FMath::Min(Size, Texture.MaxTextureSize);
	}
	return Texture.LODBias > 0 ? Size >> Texture.LODBias : Size;
}

TArray<FString> HollowArtBudget::CheckTexture(const UTexture& Texture, const int32 MaxSize)
{
	TArray<FString> Problems;
	const int32 Size = CookedMaxSize(Texture);
	if (MaxSize > 0 && Size > MaxSize)
	{
		Problems.Add(FString::Printf(TEXT("%s cooks at %d px (source %dx%d, MaxTextureSize %d, LODBias %d), over the %d px budget: set MaxTextureSize"),
			*Texture.GetName(), Size, Texture.Source.GetSizeX(), Texture.Source.GetSizeY(), Texture.MaxTextureSize, Texture.LODBias, MaxSize));
	}
	return Problems;
}

bool HollowArtBudget::IsDecorFolder(const FString& Folder, const TConstArrayView<FString> DecorFolders)
{
	for (const FString& Decor : DecorFolders)
	{
		if (!Decor.IsEmpty() && (Folder == Decor || Folder.StartsWith(Decor + TEXT("/"))))
		{
			return true;
		}
	}
	return false;
}

TArray<FString> HollowArtBudget::CheckDecor(const ULevel& Level, const TConstArrayView<FString> DecorFolders)
{
	TArray<FString> Problems;
	for (const AActor* Actor : Level.Actors)
	{
		if (!Actor || !IsDecorFolder(Actor->GetFolderPath().ToString(), DecorFolders))
		{
			continue;
		}
		TInlineComponentArray<const UPrimitiveComponent*> Primitives(Actor);
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			const bool bCollides = Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
			if (bCollides || Primitive->CanEverAffectNavigation())
			{
				Problems.Add(FString::Printf(TEXT("decoration %s (%s, folder %s)%s%s: decoration neither collides nor shapes the navmesh, structure goes outside the decor folders"),
					*Actor->GetActorNameOrLabel(), *Primitive->GetName(), *Actor->GetFolderPath().ToString(), bCollides ? TEXT(" collides") : TEXT(""),
					Primitive->CanEverAffectNavigation() ? TEXT(" affects navigation") : TEXT("")));
			}
		}
	}
	return Problems;
}

bool HollowArtBudget::IsScanned(const FString& PackageName, const TConstArrayView<FString> ScannedPaths)
{
	for (const FString& Path : ScannedPaths)
	{
		if (!Path.IsEmpty() && (PackageName == Path || PackageName.StartsWith(Path + TEXT("/"))))
		{
			return true;
		}
	}
	return false;
}
