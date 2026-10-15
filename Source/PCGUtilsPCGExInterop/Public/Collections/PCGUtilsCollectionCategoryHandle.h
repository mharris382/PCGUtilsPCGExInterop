// Copyright Max Harris

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"

#include "PCGUtilsCollectionCategoryHandle.generated.h"

class UPCGExAssetCollection;

/**
 * A reference to one named category inside a PCGEx asset collection: the collection equivalent of a
 * DataTableRowHandle. In the editor, assigning a collection turns Category into a dropdown of the categories
 * that collection's entries use.
 */
USTRUCT(BlueprintType)
struct PCGUTILSPCGEXINTEROP_API FPCGUtilsCollectionCategoryHandle
{
	GENERATED_BODY()

	FPCGUtilsCollectionCategoryHandle() = default;

	FPCGUtilsCollectionCategoryHandle(const TSoftObjectPtr<UPCGExAssetCollection>& InCollection, FName InCategory)
		: Collection(InCollection), Category(InCategory)
	{
	}

	/** The collection the category belongs to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collection Category")
	TSoftObjectPtr<UPCGExAssetCollection> Collection;

	/** A category used by at least one of the collection's entries. None means no category. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collection Category")
	FName Category = NAME_None;

	/** True when no collection is assigned. */
	bool IsNull() const { return Collection.IsNull(); }

	/** The collection, loading it if it is not already loaded. Null when none is assigned or it fails to load. */
	UPCGExAssetCollection* LoadCollection() const;

	/** True when a collection is assigned, it loads, and at least one of its entries uses Category. */
	bool IsValid() const;

	/** The categories used by InCollection's own entries, without None, sorted by name. */
	static TArray<FName> GetCategoryNames(const UPCGExAssetCollection* InCollection);

	/** "CollectionName.Category", for logs. */
	FString ToDebugString() const;

	bool operator==(const FPCGUtilsCollectionCategoryHandle& Other) const
	{
		return Collection == Other.Collection && Category == Other.Category;
	}

	bool operator!=(const FPCGUtilsCollectionCategoryHandle& Other) const { return !(*this == Other); }

	friend uint32 GetTypeHash(const FPCGUtilsCollectionCategoryHandle& Handle)
	{
		return HashCombine(GetTypeHash(Handle.Collection), GetTypeHash(Handle.Category));
	}
};
