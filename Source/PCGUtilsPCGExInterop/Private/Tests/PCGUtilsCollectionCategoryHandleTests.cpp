// Copyright Max Harris

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Collections/PCGExMeshCollection.h"
#include "Collections/PCGUtilsCollectionCategoryHandle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGUtilsCollectionCategoryHandleTest,
	"PCGUtils.PCGExInterop.CollectionCategoryHandle.NamesTheCategoriesTheEntriesUse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGUtilsCollectionCategoryHandleTest::RunTest(const FString&)
{
	UPCGExMeshCollection* Collection = NewObject<UPCGExMeshCollection>();
	const FName Categories[] = {TEXT("Rocks"), NAME_None, TEXT("Barrels"), TEXT("Rocks")};
	for (const FName Category : Categories)
	{
		Collection->Entries.AddDefaulted_GetRef().Category = Category;
	}

	const TArray<FName> Names = FPCGUtilsCollectionCategoryHandle::GetCategoryNames(Collection);
	UTEST_EQUAL(TEXT("Each used category once, without None"), Names.Num(), 2);
	TestEqual(TEXT("Sorted by name: Barrels first"), Names[0], FName(TEXT("Barrels")));
	TestEqual(TEXT("Sorted by name: Rocks second"), Names[1], FName(TEXT("Rocks")));
	TestEqual(TEXT("No collection has no categories"), FPCGUtilsCollectionCategoryHandle::GetCategoryNames(nullptr).Num(), 0);

	const TSoftObjectPtr<UPCGExAssetCollection> SoftCollection(Collection);
	TestTrue(TEXT("A category the entries use is valid"), FPCGUtilsCollectionCategoryHandle(SoftCollection, TEXT("Rocks")).IsValid());
	TestFalse(TEXT("A category no entry uses is not"), FPCGUtilsCollectionCategoryHandle(SoftCollection, TEXT("Crates")).IsValid());
	TestFalse(TEXT("None is not a category"), FPCGUtilsCollectionCategoryHandle(SoftCollection, NAME_None).IsValid());
	TestTrue(TEXT("No collection is null"), FPCGUtilsCollectionCategoryHandle().IsNull());
	TestFalse(TEXT("And not valid"), FPCGUtilsCollectionCategoryHandle(nullptr, TEXT("Rocks")).IsValid());
	return true;
}

#endif
