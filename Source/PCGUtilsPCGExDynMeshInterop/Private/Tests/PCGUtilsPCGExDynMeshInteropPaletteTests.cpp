// Copyright Max Harris

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "PCGSettings.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

/**
 * The interop half of the PCGUtils context-menu naming contract.
 *
 * PCGUtils.Palette.SearchContract (in PCGUtilsFracture) covers the PCGUtils plugin's own modules and cannot
 * see this one: it lives in a sibling plugin that PCGUtils must not depend on. The search model below is the
 * same deliberate reimplementation of FEdGraphSchemaAction::UpdateSearchText and
 * SGraphActionMenu::GenerateFilteredItems - each field split on spaces, lowercased and concatenated with NO
 * separator, and every space-separated search term required as a substring - so the two tests agree on what
 * "findable" means. Extend this one when this plugin grows a family.
 *
 * The interop family prefix is DynMeshEx: it contains DynMesh as a substring, so a single "DynMesh" search
 * still returns the whole modelling library including these nodes, while "DynMeshEx" narrows to the ones
 * that need PCGEx.
 */
namespace PCGUtilsPCGExDynMeshInteropPalette
{
	struct FEntry
	{
		FString Label;
		FString SearchText;
		FString ClassName;
		FString Category;
		FString Subtitle;
	};

	FString MakeSearchText(const FString& Label, const FString& Keywords, const FString& Category)
	{
		FString Out;
		for (const FString& Part : {Label, Keywords, Category})
		{
			TArray<FString> Words;
			Part.ParseIntoArray(Words, TEXT(" "), true);
			for (const FString& Word : Words)
			{
				Out += Word.ToLower();
			}
			// The engine separates the three fields, so a term can never straddle field boundaries.
			Out += TEXT("\n");
		}
		return Out;
	}

	bool Matches(const FString& SearchText, const FString& Filter)
	{
		TArray<FString> Terms;
		Filter.ToLower().ParseIntoArray(Terms, TEXT(" "), true);
		for (const FString& Term : Terms)
		{
			if (!SearchText.Contains(Term, ESearchCase::CaseSensitive))
			{
				return false;
			}
		}
		return true;
	}

	TArray<FEntry> CollectEntries()
	{
		static const FString Package = TEXT("/Script/PCGUtilsPCGExDynMeshInterop");

		TArray<FEntry> Entries;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (!Class->IsChildOf(UPCGSettings::StaticClass()) ||
				Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_Hidden))
			{
				continue;
			}

			if (Class->GetOutermost()->GetName() != Package)
			{
				continue;
			}

			const UPCGSettings* Settings = Class->GetDefaultObject<UPCGSettings>();
			if (!Settings || !Settings->bExposeToLibrary)
			{
				continue;
			}

			const FString Keywords = Class->GetMetaData(TEXT("Keywords"));
			const FString Category = StaticEnum<EPCGSettingsType>()
				->GetDisplayNameTextByValue(static_cast<int64>(Settings->GetType())).ToString();
			const FString Subtitle = Settings->GetAdditionalTitleInformation();

			const TArray<FPCGPreConfiguredSettingsInfo> Presets = Settings->GetPreconfiguredInfo();
			if (Presets.IsEmpty() || !Settings->OnlyExposePreconfiguredSettings())
			{
				const FString Label = Settings->GetDefaultNodeTitle().ToString();
				Entries.Add({Label, MakeSearchText(Label, Keywords, Category), Class->GetName(), Category, Subtitle});
			}
			for (const FPCGPreConfiguredSettingsInfo& Preset : Presets)
			{
				const FString Label = Preset.Label.ToString();
				const FString PresetKeywords = Keywords + TEXT(" ") + Preset.SearchHints.ToString();
				Entries.Add({Label, MakeSearchText(Label, PresetKeywords, Category), Class->GetName(), Category, Subtitle});
			}
		}
		return Entries;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGUtilsPCGExDynMeshInteropPaletteTest,
	"PCGUtils.Palette.PCGExDynMeshInterop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGUtilsPCGExDynMeshInteropPaletteTest::RunTest(const FString&)
{
	using namespace PCGUtilsPCGExDynMeshInteropPalette;

	const TArray<FEntry> Entries = CollectEntries();
	if (!TestTrue(TEXT("The interop palette contract found entries to check"), Entries.Num() > 0))
	{
		return false;
	}

	bool bFoundRealizeBuilders = false;

	for (const FEntry& Entry : Entries)
	{
		const FString Where = FString::Printf(TEXT("%s (%s)"), *Entry.Label, *Entry.ClassName);

		// An element deriving from UPCGSettings without overriding GetType() silently lands in Generic. These
		// inherit GetType() from their PCGUtilsDynMesh base, and this is what proves the inheritance holds.
		TestTrue(*FString::Printf(TEXT("%s has a PCGUtils DynMesh menu category"), *Where),
			Entry.Category.StartsWith(TEXT("PCGUtils|DynMesh")));

		TestTrue(*FString::Printf(TEXT("%s carries the DynMeshEx| prefix"), *Where),
			Entry.Label.StartsWith(TEXT("DynMeshEx | ")));

		// One "DynMesh" search must still return the whole modelling library, these nodes included.
		TestTrue(*FString::Printf(TEXT("'DynMesh' finds %s"), *Where), Matches(Entry.SearchText, TEXT("DynMesh")));
		TestTrue(*FString::Printf(TEXT("'DynMeshEx' finds %s"), *Where), Matches(Entry.SearchText, TEXT("DynMeshEx")));
		TestTrue(*FString::Printf(TEXT("'PCGEx' finds %s"), *Where), Matches(Entry.SearchText, TEXT("PCGEx")));

		// Nothing here may answer a GC search: the two families stay separable, and no concatenation of
		// adjacent keywords may accidentally spell "gc".
		TestFalse(*FString::Printf(TEXT("'GC' excludes %s"), *Where), Matches(Entry.SearchText, TEXT("GC")));

		// The name never repeats its family.
		const FString Name = Entry.Label.RightChop(FString(TEXT("DynMeshEx | ")).Len()).ToLower();
		TestFalse(*FString::Printf(TEXT("%s name omits DynMesh"), *Where), Name.Contains(TEXT("dynmesh")));
		TestFalse(*FString::Printf(TEXT("%s name omits DynamicMesh"), *Where), Name.Contains(TEXT("dynamicmesh")));

		TestFalse(*FString::Printf(TEXT("%s subtitle is not a qualified enum value"), *Where),
			Entry.Subtitle.Contains(TEXT("::")));

		if (Entry.ClassName == TEXT("PCGDynMeshExRealizeBuildersSettings"))
		{
			bFoundRealizeBuilders = true;
			TestEqual(TEXT("DynMeshEx Realize Builders keeps its agreed title"), Entry.Label,
				FString(TEXT("DynMeshEx | Realize Builders")));
			TestTrue(TEXT("'Builder' finds DynMeshEx Realize Builders"),
				Matches(Entry.SearchText, TEXT("Builder")));
			TestTrue(TEXT("'Build' finds DynMeshEx Realize Builders"),
				Matches(Entry.SearchText, TEXT("Build")));
		}
	}

	TestTrue(TEXT("DynMeshEx | Realize Builders is exposed"), bFoundRealizeBuilders);
	return true;
}

#endif
