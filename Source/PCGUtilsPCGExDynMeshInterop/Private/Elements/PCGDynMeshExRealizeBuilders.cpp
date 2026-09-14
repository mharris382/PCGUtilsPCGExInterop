// Copyright Max Harris

#include "Elements/PCGDynMeshExRealizeBuilders.h"

#include "Core/PCGExMatchRuleFactoryProvider.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointElements.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExTaggedData.h"
#include "Factories/PCGUtilsDynMeshBuilderFactory.h"
#include "Helpers/PCGExDataMatcher.h"
#include "Helpers/PCGExMatchingHelpers.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Utils/PCGLogErrors.h"

#define LOCTEXT_NAMESPACE "PCGDynMeshExRealizeBuilders"

namespace
{
	/**
	 * The seed points a match rule reads from. PCGEx match rules take their source side as a facade, so the
	 * incoming point data is wrapped in the thinnest possible one: an input-only FPointIO over the existing
	 * data, carrying the pin's tags. Nothing is ever written through it, so no output initialization - and
	 * therefore no FPCGExContext - is needed.
	 */
	struct FDynMeshExSeedSources
	{
		TArray<TSharedPtr<PCGExData::FFacade>> Facades;

		/** Seed data to its position in Facades, which is the IO index the rules' per-source getters use. */
		TMap<const UPCGBasePointData*, int32> IndexByData;

		void Add(FPCGContext* Context, const UPCGBasePointData* PointData, const TSet<FString>& Tags)
		{
			// The matcher refuses duplicate sources, and one data can legitimately arrive on the pin twice.
			if (IndexByData.Contains(PointData))
			{
				return;
			}

			const TSharedRef<PCGExData::FPointIO> PointIO =
				MakeShared<PCGExData::FPointIO>(Context->GetOrCreateHandle(), PointData);
			PointIO->Tags = MakeShared<PCGExData::FTags>(Tags);
			PointIO->IOIndex = Facades.Num();

			IndexByData.Add(PointData, Facades.Num());
			Facades.Add(MakeShared<PCGExData::FFacade>(PointIO));
		}
	};

	/**
	 * The Builders a seed is matched against. A Builder reaches the node as factory data on a pin, so its
	 * tags - the thing a rule most naturally keys on - live on the pin entry rather than on the object,
	 * which is why the pin is walked here instead of going through PCGUtilsDynMeshFactories.
	 */
	struct FDynMeshExBuilderCandidates
	{
		TMap<const UPCGUtilsDynMeshBuilderFactoryData*, FPCGExTaggedData> ByBuilder;

		/** FPCGExTaggedData holds its tags weakly, so the shared tag objects must outlive the match. */
		TArray<TSharedPtr<PCGExData::FTags>> TagStorage;

		void Add(const UPCGUtilsDynMeshBuilderFactoryData* Builder, const TSet<FString>& Tags)
		{
			if (ByBuilder.Contains(Builder))
			{
				return;
			}

			const TSharedPtr<PCGExData::FTags> SharedTags = MakeShared<PCGExData::FTags>(Tags);
			TagStorage.Add(SharedTags);
			ByBuilder.Add(Builder, FPCGExTaggedData(Builder, ByBuilder.Num(), SharedTags, nullptr));
		}
	};
}

#if WITH_EDITOR
FText UPCGDynMeshExRealizeBuildersSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "DynMeshEx | Realize Builders");
}

FText UPCGDynMeshExRealizeBuildersSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Realize Builders, with the Builder-to-seed pairing under the graph's control. Each seed point is "
		"tested against each Builder using PCGEx match rules, and only the Builders that match are evaluated "
		"for that seed - so one scatter can become walls, doors and windows without one Realize Builders "
		"node and a Point Filter per kind. The seeds are the matching source and the Builders the "
		"candidates: a rule compares a tag or attribute on the Builder against one on the seed.");
}
#endif

TArray<FPCGPinProperties> UPCGDynMeshExRealizeBuildersSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins = Super::InputPinProperties();
	PCGExMatching::Helpers::DeclareMatchingRulesInputs(DataMatching, Pins);
	return Pins;
}

FPCGElementPtr UPCGDynMeshExRealizeBuildersSettings::CreateElement() const
{
	return MakeShared<FPCGDynMeshExRealizeBuildersElement>();
}

bool FPCGDynMeshExRealizeBuildersElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDynMeshExRealizeBuildersElement::ExecuteInternal);
	check(Context);

	const UPCGDynMeshExRealizeBuildersSettings* Settings =
		Context->GetInputSettings<UPCGDynMeshExRealizeBuildersSettings>();
	check(Settings);

	const FText NodeName = LOCTEXT("NodeName", "Realize Builders");

	auto RealizeEveryBuilder = [Context, Settings, &NodeName]()
	{
		return PCGUtilsDynMeshBuilderRealization::Realize(
			Context, Settings->bConvertSeedsToLocalSpace, Settings->OutputMode, NodeName);
	};

	if (!Settings->DataMatching.IsEnabled())
	{
		return RealizeEveryBuilder();
	}

	TArray<TObjectPtr<const UPCGExMatchRuleFactoryData>> MatchRules;
	for (const FPCGTaggedData& Input :
		Context->InputData.GetInputsByPin(PCGExMatching::Labels::SourceMatchRulesLabel))
	{
		if (const UPCGExMatchRuleFactoryData* MatchRule = Cast<const UPCGExMatchRuleFactoryData>(Input.Data))
		{
			MatchRules.Add(MatchRule);
		}
	}

	if (MatchRules.IsEmpty())
	{
		PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("NoMatchRules",
			"{0} has matching enabled but received no Match Rules. Connect a rule, or set Mode to Disabled "
			"to apply every Builder to every seed."), NodeName), Context);
		return true;
	}

	FDynMeshExSeedSources Seeds;
	for (const FPCGTaggedData& SeedInput :
		Context->InputData.GetInputsByPin(PCGDynMeshRealizeBuildersConstants::SeedsPin))
	{
		if (const UPCGBasePointData* SeedPointData = Cast<const UPCGBasePointData>(SeedInput.Data))
		{
			Seeds.Add(Context, SeedPointData, SeedInput.Tags);
		}
	}

	FDynMeshExBuilderCandidates Builders;
	for (const FPCGTaggedData& BuilderInput :
		Context->InputData.GetInputsByPin(PCGUtilsDynMeshBuilderFactoryConstants::BuildersInputPin))
	{
		if (const UPCGUtilsDynMeshBuilderFactoryData* Builder =
			Cast<const UPCGUtilsDynMeshBuilderFactoryData>(BuilderInput.Data))
		{
			Builders.Add(Builder, BuilderInput.Tags);
		}
	}

	// Realize reports its own missing-input errors, so an empty side here just means there is nothing to
	// match and the unfiltered path will produce the right diagnostic.
	if (Seeds.Facades.IsEmpty() || Builders.ByBuilder.IsEmpty())
	{
		return RealizeEveryBuilder();
	}

	const TSharedPtr<PCGExMatching::FDataMatcher> Matcher = MakeShared<PCGExMatching::FDataMatcher>();
	Matcher->SetDetails(&Settings->DataMatching);
	if (!Matcher->Init(MatchRules, Seeds.Facades, /*bThrowError=*/false))
	{
		PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("MatcherInitFailed",
			"{0} could not prepare its Match Rules against the incoming seed points. Check that every "
			"attribute the rules read exists on the seeds."), NodeName), Context);
		return true;
	}

	const PCGUtilsDynMeshBuilderRealization::FSeedBuilderFilter Filter =
		[&Seeds, &Builders, &Matcher](
		const FPCGUtilsDynMeshBuildContext& SeedContext,
		const UPCGUtilsDynMeshBuilderFactoryData& Builder)
		{
			const FPCGExTaggedData* Candidate = Builders.ByBuilder.Find(&Builder);
			const int32* SourceIndex = Seeds.IndexByData.Find(SeedContext.SeedData);
			if (!Candidate || !SourceIndex)
			{
				// Neither side was registered, so there is nothing to match on: fall back to the base
				// node's contract rather than silently dropping geometry.
				return true;
			}

			const PCGExData::FConstPoint SeedPoint(SeedContext.SeedData, SeedContext.SeedIndex, *SourceIndex);

			// Matching is a per-pairing filter here, never a budget, so each test gets its own unlimited
			// scope. This is also why the settings declare Filter usage, which hides the limit options.
			PCGExMatching::FScope Scope(/*InNumCandidates=*/1, /*bUnlimited=*/true);
			return Matcher->Test(SeedPoint, *Candidate, Scope);
		};

	return PCGUtilsDynMeshBuilderRealization::Realize(
		Context, Settings->bConvertSeedsToLocalSpace, Settings->OutputMode, NodeName, Filter);
}

#undef LOCTEXT_NAMESPACE
