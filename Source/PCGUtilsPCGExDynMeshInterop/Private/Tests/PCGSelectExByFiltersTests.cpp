// Copyright Max Harris

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Core/PCGExPointFilter.h"
#include "Data/PCGDynamicMeshData.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/PCGUtilsDynMeshAttributeHelpers.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/PCGMetadataDomain.h"
#include "Selections/GeometrySelection.h"
#include "Selections/PCGSelectExByFilters.h"
#include "UDynamicMesh.h"
#include "UObject/Package.h"

namespace PCGSelectExByFiltersTests
{
	using namespace UE::Geometry;

	/**
	 * A PCGEx Numeric Compare filter: OperandA <Comparison> (a constant, or an attribute when OperandBAttribute is
	 * set). The factory class is not exported, so it is made by name and configured through reflection, exactly the
	 * fields its provider node would copy in.
	 */
	const UPCGExPointFilterFactoryData* MakeNumericCompare(
		const TCHAR* OperandA, const TCHAR* Comparison, double OperandB, const TCHAR* OperandBAttribute = nullptr)
	{
		UClass* Class = FindObject<UClass>(nullptr, TEXT("/Script/PCGExFilters.PCGExNumericCompareFilterFactory"));
		if (!Class)
		{
			return nullptr;
		}
		UPCGExPointFilterFactoryData* Factory = NewObject<UPCGExPointFilterFactoryData>(GetTransientPackage(), Class);
		const FStructProperty* ConfigProperty = FindFProperty<FStructProperty>(Class, TEXT("Config"));
		if (!ConfigProperty)
		{
			return nullptr;
		}
		void* Config = ConfigProperty->ContainerPtrToValuePtr<void>(Factory);
		const UScriptStruct* ConfigStruct = ConfigProperty->Struct;

		auto Selector = [&](const TCHAR* Name) -> FPCGAttributePropertyInputSelector*
		{
			const FStructProperty* Property = FindFProperty<FStructProperty>(ConfigStruct, Name);
			return Property ? Property->ContainerPtrToValuePtr<FPCGAttributePropertyInputSelector>(Config) : nullptr;
		};
		auto Import = [&](const TCHAR* Name, const TCHAR* Value)
		{
			if (const FProperty* Property = FindFProperty<FProperty>(ConfigStruct, Name))
			{
				Property->ImportText_InContainer(Value, Config, Factory, PPF_None);
			}
		};

		if (FPCGAttributePropertyInputSelector* A = Selector(TEXT("OperandA")))
		{
			A->Update(OperandA);
		}
		Import(TEXT("Comparison"), Comparison);
		Import(TEXT("OperandBConstant"), *FString::SanitizeFloat(OperandB));
		if (OperandBAttribute)
		{
			Import(TEXT("CompareAgainst"), TEXT("Attribute"));
			if (FPCGAttributePropertyInputSelector* B = Selector(TEXT("OperandB")))
			{
				B->Update(OperandBAttribute);
			}
		}
		return Factory;
	}

	/**
	 * A strip along +X: columns at x = 25, 35, ..., 85 and y = 0, 10. Vertex color R is 1 from x = 65 on, and
	 * @Data.Threshold is 70.
	 */
	UPCGDynamicMeshData* MakeStrip()
	{
		FDynamicMesh3 Mesh;
		for (int32 Column = 0; Column <= 6; ++Column)
		{
			Mesh.AppendVertex(FVector3d(25.0 + Column * 10.0, 0.0, 0.0));
			Mesh.AppendVertex(FVector3d(25.0 + Column * 10.0, 10.0, 0.0));
		}
		for (int32 Column = 0; Column < 6; ++Column)
		{
			const int32 A = Column * 2, B = A + 1, C = A + 2, D = A + 3;
			Mesh.AppendTriangle(A, C, D);
			Mesh.AppendTriangle(A, D, B);
		}
		Mesh.EnableAttributes();
		FDynamicMeshColorOverlay* Colors = PCGUtilsDynMeshAttributeHelpers::EnsurePrimaryColorOverlay(Mesh, FVector4f(0, 0, 0, 1));
		for (const int32 VertexID : Mesh.VertexIndicesItr())
		{
			const bool bRed = Mesh.GetVertex(VertexID).X >= 65.0;
			PCGUtilsDynMeshAttributeHelpers::SetVertexColor(Mesh, *Colors, VertexID, FVector4f(bRed ? 1.0f : 0.0f, 0, 0, 1));
		}

		UPCGDynamicMeshData* Data = NewObject<UPCGDynamicMeshData>();
		Data->Initialize(MoveTemp(Mesh));
		FPCGMetadataDomain* Domain = Data->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Data);
		Domain->AddEntry();
		Domain->FindOrCreateAttribute<double>(TEXT("Threshold"), 0.0, false, false, true)->SetValue(PCGFirstEntryKey, 70.0);
		return Data;
	}

	UPCGSelectExByFiltersFactoryData* MakeSelector(const UPCGExPointFilterFactoryData* Filter, bool bInvert = false)
	{
		UPCGSelectExByFiltersFactoryData* Selector = NewObject<UPCGSelectExByFiltersFactoryData>();
		Selector->Filters.Add(Filter);
		Selector->bInvertSelection = bInvert;
		// No PCG target actor here; the strip is authored where the filters expect it.
		Selector->bPositionsToWorld = false;
		return Selector;
	}

	/** Evaluates in the vertex domain and returns the sorted, de-duplicated X of the selected vertices. */
	bool SelectedX(const UPCGUtilsDynMeshSelectionFactoryData* Selector, const UPCGDynamicMeshData* Data, FString& OutX)
	{
		FPCGUtilsDynMeshSelectionDomain Domain;
		Domain.ElementType = EGeometryElementType::Vertex;
		const FDynamicMesh3& Mesh = *Data->GetDynamicMesh()->GetMeshPtr();
		const FPCGUtilsDynMeshSelectionEvaluationContext Evaluation(Data, Mesh, Domain);
		FGeometrySelection Result;
		if (!PCGUtilsDynMeshSelectionFactories::EvaluateFactory(Selector, Evaluation, nullptr, Result))
		{
			return false;
		}
		FGeometryScriptMeshSelection ScriptSelection;
		ScriptSelection.SetSelection(MoveTemp(Result));
		TArray<int32> IDs;
		ScriptSelection.ConvertToMeshIndexArray(Mesh, IDs, EGeometryScriptIndexType::Vertex);
		TArray<double> X;
		for (const int32 ID : IDs)
		{
			X.AddUnique(Mesh.GetVertex(ID).X);
		}
		X.Sort();
		OutX = FString::JoinBy(X, TEXT(", "), [](double Value) { return FString::SanitizeFloat(Value); });
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGSelectExByFiltersTest,
	"PCGUtils.PCGExInterop.SelectExByFilters.FiltersVerticesAsPoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGSelectExByFiltersTest::RunTest(const FString&)
{
	using namespace PCGSelectExByFiltersTests;

	const UPCGDynamicMeshData* Strip = MakeStrip();

	struct FCase
	{
		const TCHAR* Name;
		const UPCGExPointFilterFactoryData* Filter;
		bool bInvert;
		const TCHAR* Expected;
	};
	const FCase Cases[] = {
		{TEXT("$Position.X > 50 reads vertex positions"),
			MakeNumericCompare(TEXT("$Position.X"), TEXT("StrictlyGreater"), 50.0), false, TEXT("55.0, 65.0, 75.0, 85.0")},
		{TEXT("Invert selects the vertices that fail"),
			MakeNumericCompare(TEXT("$Position.X"), TEXT("StrictlyGreater"), 50.0), true, TEXT("25.0, 35.0, 45.0")},
		{TEXT("$Color.R > 0.5 reads vertex colors"),
			MakeNumericCompare(TEXT("$Color.R"), TEXT("StrictlyGreater"), 0.5), false, TEXT("65.0, 75.0, 85.0")},
		{TEXT("$Position.X > @Data.Threshold reads the mesh's @Data"),
			MakeNumericCompare(TEXT("$Position.X"), TEXT("StrictlyGreater"), 0.0, TEXT("@Data.Threshold")), false, TEXT("75.0, 85.0")},
	};

	for (const FCase& Case : Cases)
	{
		if (!TestNotNull(*FString::Printf(TEXT("%s: the PCGEx filter can be made"), Case.Name), Case.Filter))
		{
			continue;
		}
		FString Actual;
		if (TestTrue(*FString::Printf(TEXT("%s: evaluates"), Case.Name), SelectedX(MakeSelector(Case.Filter, Case.bInvert), Strip, Actual)))
		{
			TestEqual(Case.Name, Actual, FString(Case.Expected));
		}
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
