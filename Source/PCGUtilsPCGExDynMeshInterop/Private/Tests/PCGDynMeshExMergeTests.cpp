// Copyright Max Harris

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Core/PCGExMatchRuleFactoryProvider.h"
#include "Data/PCGDynamicMeshData.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Elements/PCGDynMeshExMerge.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/PCGMetadataDomain.h"
#include "OrientedBoxTypes.h"
#include "PCGContext.h"
#include "PCGExMatchingCommon.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "UDynamicMesh.h"
#include "UObject/UnrealType.h"

namespace PCGDynMeshExMergeTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	TArray<FPCGTaggedData> Run(UPCGSettings* Settings, TArray<FPCGTaggedData> Inputs)
	{
		FPCGDataCollection Input;
		Input.TaggedData = MoveTemp(Inputs);
		Input.TaggedData.Emplace_GetRef().Data = Settings;
		FPCGElementPtr Element = Settings->GetElement();
		// PCGEx contexts hold a handle and must be released, not deleted.
		FPCGContext* Context = Element->Initialize(FPCGInitializeElementParams(&Input, nullptr, nullptr));
		Context->AsyncState.bIsRunningOnMainThread = true;
		Context->AsyncState.NumAvailableTasks = 1;
		TArray<FPCGTaggedData> Outputs;
		for (int32 Iteration = 0; Iteration < 50; ++Iteration)
		{
			if (Element->Execute(Context))
			{
				Outputs = Context->OutputData.TaggedData;
				break;
			}
		}
		FPCGContext::Release(Context);
		return Outputs;
	}

	/** A 100 box at X, with an ID on the data itself written as the first entry, and one material. */
	FPCGTaggedData Box(double X, double DataID, const TCHAR* Tag, UMaterialInterface* Material)
	{
		UE::Geometry::FMinimalBoxMeshGenerator Generator;
		Generator.Box = UE::Geometry::FOrientedBox3d(FVector3d(X, 0, 0), FVector3d(50, 50, 50));
		UPCGDynamicMeshData* Data = NewObject<UPCGDynamicMeshData>();
		Data->Initialize(UE::Geometry::FDynamicMesh3(&Generator.Generate()), { Material });

		FPCGMetadataDomain* Domain = Data->MutableMetadata()->GetMetadataDomain(PCGMetadataDomainID::Data);
		FPCGMetadataAttribute<double>* ID = Domain->FindOrCreateAttribute<double>(TEXT("CutID"), 0.0, false, false, true);
		Domain->AddEntry();
		ID->SetValue(PCGFirstEntryKey, DataID);

		FPCGTaggedData Tagged;
		Tagged.Data = Data;
		Tagged.Pin = PCGPinConstants::DefaultInputLabel;
		Tagged.Tags.Add(Tag);
		return Tagged;
	}

	double ReadDataID(const UPCGData* Data)
	{
		const FPCGMetadataAttribute<double>* Attribute = Data->ConstMetadata()
			? Data->ConstMetadata()->GetConstTypedAttribute<double>(FPCGAttributeIdentifier(TEXT("CutID"), PCGMetadataDomainID::Data)) : nullptr;
		return Attribute ? Attribute->GetValueFromItemKey(PCGFirstEntryKey) : -1.0;
	}

	const UE::Geometry::FDynamicMesh3& MeshOf(const FPCGTaggedData& Output)
	{
		return CastChecked<UPCGDynamicMeshData>(Output.Data)->GetDynamicMesh()->GetMeshRef();
	}

	/**
	 * A Match | Attributes rule comparing @Data.CutID with itself, made the way a graph makes it: by running the
	 * PCGEx node that creates one. Found by name because PCGEx exports the class but not the functions of its
	 * config. The rule belongs to the context of the node that made it, so the caller keeps that context until it
	 * is done with the rule, then releases it.
	 */
	const UPCGData* MakeSameCutIDRule(FPCGContext*& OutProviderContext)
	{
		OutProviderContext = nullptr;
		UClass* ProviderClass = FindObject<UClass>(nullptr, TEXT("/Script/PCGExMatching.PCGExCreateMatchAttrToAttrSettings"));
		const FStructProperty* ConfigProperty = ProviderClass ? FindFProperty<FStructProperty>(ProviderClass, TEXT("Config")) : nullptr;
		if (!ConfigProperty)
		{
			return nullptr;
		}
		UPCGSettings* Provider = NewObject<UPCGSettings>(GetTransientPackage(), ProviderClass);
		void* Config = ConfigProperty->ContainerPtrToValuePtr<void>(Provider);
		const FNameProperty* Candidate = FindFProperty<FNameProperty>(ConfigProperty->Struct, TEXT("CandidateAttributeName"));
		const FNameProperty* Target = FindFProperty<FNameProperty>(ConfigProperty->Struct, TEXT("TargetAttributeName"));
		if (!Candidate || !Target)
		{
			return nullptr;
		}
		Candidate->SetPropertyValue_InContainer(Config, FName(TEXT("@Data.CutID")));
		Target->SetPropertyValue_InContainer(Config, FName(TEXT("@Data.CutID")));

		FPCGDataCollection Input;
		Input.TaggedData.Emplace_GetRef().Data = Provider;
		FPCGElementPtr Element = Provider->GetElement();
		FPCGContext* Context = Element->Initialize(FPCGInitializeElementParams(&Input, nullptr, nullptr));
		Context->AsyncState.bIsRunningOnMainThread = true;
		Context->AsyncState.NumAvailableTasks = 1;
		OutProviderContext = Context;
		for (int32 Iteration = 0; Iteration < 50; ++Iteration)
		{
			if (Element->Execute(Context))
			{
				break;
			}
		}
		for (const FPCGTaggedData& Output : Context->OutputData.TaggedData)
		{
			if (Cast<UPCGExMatchRuleFactoryData>(Output.Data))
			{
				return Output.Data;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGDynMeshExMergeEverythingTest,
	"PCGUtils.DynMeshEx.Merge.WithMatchingDisabledEverythingBecomesOneMesh", PCGDynMeshExMergeTests::Flags)

bool FPCGDynMeshExMergeEverythingTest::RunTest(const FString&)
{
	using namespace PCGDynMeshExMergeTests;
	UMaterialInterface* Grey = UMaterial::GetDefaultMaterial(MD_Surface);
	UMaterialInterface* Other = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));

	UPCGDynMeshExMergeSettings* Settings = NewObject<UPCGDynMeshExMergeSettings>();
	const FPCGTaggedData A = Box(-200, 3.0, TEXT("A"), Grey);
	const FPCGTaggedData B = Box(200, 9.0, TEXT("B"), Other);

	const TArray<FPCGTaggedData> Outputs = Run(Settings, { A, B });
	UTEST_EQUAL(TEXT("Two meshes become one"), Outputs.Num(), 1);
	const UPCGDynamicMeshData* Merged = Cast<UPCGDynamicMeshData>(Outputs[0].Data);
	UTEST_NOT_NULL(TEXT("The output is a DynMesh"), Merged);

	TestEqual(TEXT("It has both meshes' triangles"), MeshOf(Outputs[0]).TriangleCount(), MeshOf(A).TriangleCount() + MeshOf(B).TriangleCount());
	TestTrue(TEXT("It reaches from one box to the other"), FBox(MeshOf(Outputs[0]).GetBounds()).GetExtent().Equals(FVector(250, 50, 50), 0.01));
	TestTrue(TEXT("It has the tags of both"), Outputs[0].Tags.Contains(TEXT("A")) && Outputs[0].Tags.Contains(TEXT("B")));
	TestEqual(TEXT("It keeps the first mesh's @Data attribute"), ReadDataID(Merged), 3.0);
	UTEST_EQUAL(TEXT("It has both materials, once each"), Merged->GetMaterials().Num(), Grey == Other ? 1 : 2);

	// The second box's triangles must point at the second material now, not at slot 0 as they did on their own.
	if (Grey != Other)
	{
		const UE::Geometry::FDynamicMesh3& Mesh = MeshOf(Outputs[0]);
		const UE::Geometry::FDynamicMeshMaterialAttribute* MaterialIDs = Mesh.Attributes()->GetMaterialID();
		bool bSlotsFollowTheirBox = true;
		for (const int32 TriangleID : Mesh.TriangleIndicesItr())
		{
			const int32 Expected = Mesh.GetTriCentroid(TriangleID).X < 0.0 ? 0 : 1;
			bSlotsFollowTheirBox &= MaterialIDs->GetValue(TriangleID) == Expected;
		}
		TestTrue(TEXT("Each box's triangles use its own material's slot"), bSlotsFollowTheirBox);
	}

	const TArray<FPCGTaggedData> Single = Run(Settings, { A });
	UTEST_EQUAL(TEXT("One mesh on its own comes out"), Single.Num(), 1);
	TestTrue(TEXT("It is the same object it came in as"), Single[0].Data == A.Data);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGDynMeshExMergeByRuleTest,
	"PCGUtils.DynMeshEx.Merge.ARuleOnADataAttributeLeavesOneMeshPerValue", PCGDynMeshExMergeTests::Flags)

bool FPCGDynMeshExMergeByRuleTest::RunTest(const FString&)
{
	using namespace PCGDynMeshExMergeTests;
	UMaterialInterface* Grey = UMaterial::GetDefaultMaterial(MD_Surface);

	FPCGContext* RuleContext = nullptr;
	const UPCGData* Rule = MakeSameCutIDRule(RuleContext);
	ON_SCOPE_EXIT
	{
		if (RuleContext)
		{
			FPCGContext::Release(RuleContext);
		}
	};
	UTEST_NOT_NULL(TEXT("A Match | Attributes rule could be made"), Rule);
	FPCGTaggedData RuleInput;
	RuleInput.Data = Rule;
	RuleInput.Pin = PCGExMatching::Labels::SourceMatchRulesLabel;

	UPCGDynMeshExMergeSettings* Settings = NewObject<UPCGDynMeshExMergeSettings>();
	Settings->DataMatching.Mode = EPCGExMapMatchMode::All;

	// Cut 5 arrives in two pieces with cut 2 between them on the pin.
	const FPCGTaggedData FiveA = Box(-300, 5.0, TEXT("FiveA"), Grey);
	const FPCGTaggedData Two = Box(0, 2.0, TEXT("Two"), Grey);
	const FPCGTaggedData FiveB = Box(300, 5.0, TEXT("FiveB"), Grey);

	const TArray<FPCGTaggedData> Outputs = Run(Settings, { FiveA, Two, FiveB, RuleInput });
	UTEST_EQUAL(TEXT("Three meshes with two IDs become two meshes"), Outputs.Num(), 2);

	const FPCGTaggedData* MergedFive = Outputs.FindByPredicate([](const FPCGTaggedData& Output) { return ReadDataID(Output.Data) == 5.0; });
	const FPCGTaggedData* PassedTwo = Outputs.FindByPredicate([](const FPCGTaggedData& Output) { return ReadDataID(Output.Data) == 2.0; });
	UTEST_TRUE(TEXT("There is one mesh for each ID"), MergedFive && PassedTwo);

	TestEqual(TEXT("The mesh for ID 5 has both of its pieces"), MeshOf(*MergedFive).TriangleCount(), MeshOf(FiveA).TriangleCount() * 2);
	TestTrue(TEXT("It has the tags of both pieces and not of the other ID"),
		MergedFive->Tags.Contains(TEXT("FiveA")) && MergedFive->Tags.Contains(TEXT("FiveB")) && !MergedFive->Tags.Contains(TEXT("Two")));
	TestTrue(TEXT("The mesh for ID 2, which matched no other, is the object it came in as"), PassedTwo->Data == Two.Data);

	AddExpectedError(TEXT("has matching enabled but received no Match Rules"), EAutomationExpectedErrorFlags::Contains, 1);
	TestEqual(TEXT("Matching with no rule connected outputs nothing"), Run(Settings, { FiveA, Two }).Num(), 0);
	return true;
}

#endif
