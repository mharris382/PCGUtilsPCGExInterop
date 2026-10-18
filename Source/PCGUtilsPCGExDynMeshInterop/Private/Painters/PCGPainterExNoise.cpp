// Copyright Max Harris

#include "Painters/PCGPainterExNoise.h"

#include "Core/PCGExNoise3DCommon.h"
#include "Core/PCGExNoise3DFactoryProvider.h"
#include "Core/PCGExNoise3DOperation.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Helpers/PCGExNoiseGenerator.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Serialization/ArchiveCrc32.h"
#include "Utils/PCGLogErrors.h"

#define LOCTEXT_NAMESPACE "PCGPainterExNoise"

namespace
{
	EPCGUtilsDynMeshPainterColorChannel ToPainterChannels(const FGeometryScriptColorFlags& Flags)
	{
		EPCGUtilsDynMeshPainterColorChannel Channels = EPCGUtilsDynMeshPainterColorChannel::None;
		if (Flags.bRed) Channels |= EPCGUtilsDynMeshPainterColorChannel::Red;
		if (Flags.bGreen) Channels |= EPCGUtilsDynMeshPainterColorChannel::Green;
		if (Flags.bBlue) Channels |= EPCGUtilsDynMeshPainterColorChannel::Blue;
		if (Flags.bAlpha) Channels |= EPCGUtilsDynMeshPainterColorChannel::Alpha;
		return Channels;
	}

	/**
	 * The PCGEx noise generator, initialized from stored factories rather than from a PCGEx context's pins.
	 *
	 * A Painter is evaluated inside whichever node consumes it, long after its own node ran, so there is no
	 * context holding the Noises pin. This mirrors FNoiseGenerator::Init exactly: same order, same weights, same
	 * precomputed blend factors. Noise3D factories ignore the context they create operations with.
	 */
	class FFactoryNoiseGenerator final : public PCGExNoise3D::FNoiseGenerator
	{
	public:
		bool InitFromFactories(TConstArrayView<TObjectPtr<const UPCGExNoise3DFactoryData>> Factories)
		{
			Operations.Reset();
			OperationsPtr.Reset();
			Weights.Reset();
			BlendModes.Reset();
			BlendFactors.Reset();
			TotalWeight = 0.0;

			for (const TObjectPtr<const UPCGExNoise3DFactoryData>& Factory : Factories)
			{
				const TSharedPtr<FPCGExNoise3DOperation> Operation = Factory ? Factory->CreateOperation(nullptr) : nullptr;
				if (!Operation)
				{
					continue;
				}

				const double Weight = FMath::Max(SMALL_NUMBER, Factory->ConfigBase.WeightFactor);
				TotalWeight += Weight;

				Operations.Add(Operation);
				OperationsPtr.Add(Operation.Get());
				Weights.Add(Weight);
				BlendModes.Add(Operation->BlendMode);
				BlendFactors.Add(Weight / TotalWeight);
			}
			return Operations.Num() > 0;
		}
	};

	class FNoisePainterOperation final : public FPCGUtilsDynMeshPainterOperation
	{
	public:
		explicit FNoisePainterOperation(const UPCGPainterExNoiseFactoryData* InFactory)
			: Factory(InFactory)
		{
		}

		virtual bool Initialize(const FPCGUtilsDynMeshPainterEvaluationContext& InPainterContext) override
		{
			if (!FPCGUtilsDynMeshPainterOperation::Initialize(InPainterContext) || !Factory)
			{
				return false;
			}
			if (Factory->ValueType == EPCGUtilsDynMeshPainterValueType::Color
				&& ToPainterChannels(Factory->ColorChannels) == EPCGUtilsDynMeshPainterColorChannel::None)
			{
				PCGLog::LogErrorOnGraph(LOCTEXT("NoChannels",
					"PainterEx | Noise: Value Type is Color but Color Channels has no channel enabled; enable at least one of R, G, B or A."),
					Context);
				return false;
			}

			Generator = MakeShared<FFactoryNoiseGenerator>();
			if (!Generator->InitFromFactories(Factory->Noises))
			{
				PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("NoNoise",
					"PainterEx | Noise: none of the {0} connected Noise3D definitions produced a noise."),
					FText::AsNumber(Factory->Noises.Num())), Context);
				return false;
			}
			Channels = ToPainterChannels(Factory->ColorChannels);
			return true;
		}

		/** Samples every vertex once, in parallel batches, so Evaluate is a lookup. */
		virtual bool Prepare(const FPCGUtilsDynMeshPainterEvaluationContext& InPainterContext) override
		{
			const UE::Geometry::FDynamicMesh3& Mesh = *InPainterContext.Mesh;
			TArray<int32> VertexIDs;
			TArray<FVector> Positions;
			VertexIDs.Reserve(Mesh.VertexCount());
			Positions.Reserve(Mesh.VertexCount());
			for (const int32 VertexID : Mesh.VertexIndicesItr())
			{
				VertexIDs.Add(VertexID);
				Positions.Add(ToNoiseSpace(FVector(Mesh.GetVertex(VertexID)), InPainterContext.LocalToWorld));
			}

			Values.SetNum(Mesh.MaxVertexID());
			if (Factory->ValueType == EPCGUtilsDynMeshPainterValueType::Scalar)
			{
				TArray<double> Noise;
				Noise.SetNumUninitialized(Positions.Num());
				Generator->GenerateParallel(Positions, Noise);
				for (int32 Index = 0; Index < VertexIDs.Num(); ++Index)
				{
					Values[VertexIDs[Index]] = FPCGUtilsDynMeshPainterValue::MakeScalar(static_cast<float>(Noise[Index]));
				}
			}
			else
			{
				TArray<FVector4> Noise;
				Noise.SetNumUninitialized(Positions.Num());
				Generator->GenerateParallel(Positions, Noise);
				for (int32 Index = 0; Index < VertexIDs.Num(); ++Index)
				{
					Values[VertexIDs[Index]] = FPCGUtilsDynMeshPainterValue::MakeColor(FVector4f(Noise[Index]), Channels);
				}
			}
			return true;
		}

		virtual EPCGUtilsDynMeshPainterValueType GetOutputType() const override
		{
			return Factory->ValueType;
		}

		virtual FPCGUtilsDynMeshPainterValue Evaluate(const FPCGUtilsDynMeshPainterSample& Sample) const override
		{
			if (Values.IsValidIndex(Sample.VertexID) && PainterContext && PainterContext->Mesh
				&& PainterContext->Mesh->IsVertex(Sample.VertexID))
			{
				return Values[Sample.VertexID];
			}

			// A sample that is not a prepared vertex is sampled on the spot.
			const FVector Position = Factory->Space == EPCGUtilsDynMeshPainterCoordinateSpace::World
				? Sample.WorldPosition : Sample.LocalPosition;
			return Factory->ValueType == EPCGUtilsDynMeshPainterValueType::Scalar
				? FPCGUtilsDynMeshPainterValue::MakeScalar(static_cast<float>(Generator->GetDouble(Position)))
				: FPCGUtilsDynMeshPainterValue::MakeColor(FVector4f(Generator->GetVector4(Position)), Channels);
		}

	private:
		FVector ToNoiseSpace(const FVector& LocalPosition, const FTransform& LocalToWorld) const
		{
			return Factory->Space == EPCGUtilsDynMeshPainterCoordinateSpace::World
				? LocalToWorld.TransformPosition(LocalPosition) : LocalPosition;
		}

		TObjectPtr<const UPCGPainterExNoiseFactoryData> Factory;
		TSharedPtr<FFactoryNoiseGenerator> Generator;
		EPCGUtilsDynMeshPainterColorChannel Channels = EPCGUtilsDynMeshPainterColorChannel::All;
		TArray<FPCGUtilsDynMeshPainterValue> Values;
	};
}

TSharedPtr<FPCGUtilsDynMeshPainterOperation> UPCGPainterExNoiseFactoryData::CreateOperationInternal() const
{
	return MakeShared<FNoisePainterOperation>(this);
}

void UPCGPainterExNoiseFactoryData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	// The noise definitions are inputs, so the provider records them as data dependencies of this factory.
	Super::AddToCrc(Ar, bFullDataCrc);
	if (bFullDataCrc)
	{
		uint8 TypeValue = static_cast<uint8>(ValueType);
		uint8 ChannelValue = static_cast<uint8>(ToPainterChannels(ColorChannels));
		uint8 SpaceValue = static_cast<uint8>(Space);
		Ar << TypeValue;
		Ar << ChannelValue;
		Ar << SpaceValue;
	}
}

#if WITH_EDITOR
FText UPCGPainterExNoiseProviderSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("Title", "PainterEx | Noise");
}

FText UPCGPainterExNoiseProviderSettings::GetNodeTooltipText() const
{
	return LOCTEXT("Tooltip",
		"Paints PCGEx noise per vertex: Uber Noise for DynMesh Painters. Stacks the connected Noise3D definitions with "
		"their own weights and blend modes, exactly as Uber Noise does, and samples them at each vertex. Scalar paints "
		"the noise value; Color paints its RGBA.");
}
#endif

FName UPCGPainterExNoiseProviderSettings::GetMainOutputPin() const
{
	return PCGUtilsDynMeshPainterConstants::OutputPin;
}

const FPCGDataTypeBaseId& UPCGPainterExNoiseProviderSettings::GetFactoryTypeId() const
{
	return FPCGUtilsDynMeshPainterFactoryDataTypeInfo::AsId();
}

TArray<FPCGPinProperties> UPCGPainterExNoiseProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(PCGExNoise3D::Labels::SourceNoise3DLabel, FPCGExDataTypeInfoNoise3D::AsId(), true, true).SetRequiredPin();
	return Pins;
}

UPCGUtilsDynMeshFactoryData* UPCGPainterExNoiseProviderSettings::CreateFactory(
	FPCGContext* InContext, UPCGUtilsDynMeshFactoryData* InFactory) const
{
	TArray<TObjectPtr<const UPCGExNoise3DFactoryData>> Noises;
	for (const FPCGTaggedData& Input : InContext->InputData.GetInputsByPin(PCGExNoise3D::Labels::SourceNoise3DLabel))
	{
		if (const UPCGExNoise3DFactoryData* Noise = Cast<const UPCGExNoise3DFactoryData>(Input.Data))
		{
			Noises.Add(Noise);
		}
	}
	if (Noises.IsEmpty())
	{
		PCGLog::LogErrorOnGraph(LOCTEXT("MissingNoises",
			"PainterEx | Noise needs at least one PCGEx Noise3D definition on its Noises pin."), InContext);
		return nullptr;
	}
	// Uber Noise's stacking order: stable by Priority, so equal priorities keep their pin order.
	Noises.StableSort([](const UPCGExNoise3DFactoryData& A, const UPCGExNoise3DFactoryData& B)
	{
		return A.Priority < B.Priority;
	});

	UPCGPainterExNoiseFactoryData* Factory = InFactory
		? Cast<UPCGPainterExNoiseFactoryData>(InFactory)
		: FPCGContext::NewObject_AnyThread<UPCGPainterExNoiseFactoryData>(InContext);
	if (!Factory)
	{
		return nullptr;
	}

	Factory->Priority = Priority;
	Factory->Noises = MoveTemp(Noises);
	Factory->ValueType = ValueType;
	Factory->ColorChannels = ColorChannels;
	Factory->Space = Space;
	return Super::CreateFactory(InContext, Factory);
}

#undef LOCTEXT_NAMESPACE
