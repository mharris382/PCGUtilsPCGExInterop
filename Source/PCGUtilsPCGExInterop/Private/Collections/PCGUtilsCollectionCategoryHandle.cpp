// Copyright Max Harris

#include "Collections/PCGUtilsCollectionCategoryHandle.h"

#include "Core/PCGExAssetCollection.h"

UPCGExAssetCollection* FPCGUtilsCollectionCategoryHandle::LoadCollection() const
{
	return Collection.IsNull() ? nullptr : Collection.LoadSynchronous();
}

bool FPCGUtilsCollectionCategoryHandle::IsValid() const
{
	if (Category.IsNone())
	{
		return false;
	}

	const UPCGExAssetCollection* LoadedCollection = LoadCollection();
	if (!LoadedCollection)
	{
		return false;
	}

	bool bFound = false;
	LoadedCollection->ForEachEntry([this, &bFound](const FPCGExAssetCollectionEntry* Entry, int32)
	{
		bFound |= Entry && Entry->Category == Category;
	});
	return bFound;
}

TArray<FName> FPCGUtilsCollectionCategoryHandle::GetCategoryNames(const UPCGExAssetCollection* InCollection)
{
	TSet<FName> Unique;
	if (InCollection)
	{
		// Read the entries rather than the pick cache: the cache leaves out entries that are not yet staged,
		// and a category an author has typed is a category whether or not its assets have been staged.
		InCollection->ForEachEntry([&Unique](const FPCGExAssetCollectionEntry* Entry, int32)
		{
			if (Entry && !Entry->Category.IsNone())
			{
				Unique.Add(Entry->Category);
			}
		});
	}

	TArray<FName> Names = Unique.Array();
	Names.Sort(FNameLexicalLess());
	return Names;
}

FString FPCGUtilsCollectionCategoryHandle::ToDebugString() const
{
	return FString::Printf(TEXT("%s.%s"),
		Collection.IsNull() ? TEXT("None") : *Collection.GetAssetName(), *Category.ToString());
}
