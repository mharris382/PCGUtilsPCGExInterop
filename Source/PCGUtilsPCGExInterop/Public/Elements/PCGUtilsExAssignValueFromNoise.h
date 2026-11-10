// Copyright Max Harris

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "Details/PCGExInputShorthandsDetails.h"
#include "Noise/PCGUtilsExNoiseShaping.h"

#include "PCGUtilsExAssignValueFromNoise.generated.h"

/** The type Min and Max are lerped as, and so the type written to the output. */
UENUM(BlueprintType)
enum class EPCGUtilsExNoiseLerpType : uint8
{
	/** A number (written as Double; converts to the output's type, e.g. Float for $Density). */
	Scalar,
	Vector2 UMETA(DisplayName = "Vector 2"),
	Vector,
	/** Four components, e.g. a color for $Color. */
	Vector4 UMETA(DisplayName = "Vector 4"),
	/** Pitch, yaw and roll are each lerped, so Min 0 / Max 360 yaw can give any heading. Converts to a quaternion for $Rotation. */
	Rotator
};

namespace PCGUtilsExAssignValueFromNoiseConstants
{
	inline const FName NoisesPin = TEXT("Noises");
}

/**
 * PointEx | Assign Value From Noise. One node for the chain
 * Uber Noise -> random offset -> clamp -> remap -> curve -> scale by attribute -> Lerp(Min, Max, noise) -> write.
 *
 * Samples the PCGEx Noise3D stack on the Noises pin at each point's position (stacked exactly as Uber Noise does),
 * shapes it to a 0-1 value, then writes Lerp(Min, Max, shaped noise) to the output attribute ($Density by default).
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category = "PCGUtils",
	meta = (Keywords = "PCGEx noise uber assign value lerp remap clamp random offset curve density perlin simplex worley fbm point points shorthand"))
class PCGUTILSPCGEXINTEROP_API UPCGUtilsExAssignValueFromNoiseSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("AssignValueFromNoise")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual FString GetAdditionalTitleInformation() const override;
	virtual EPCGSettingsType GetType() const override;
#endif
	virtual bool UseSeed() const override { return true; }
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Random offset, clamp, remap band and curve, scale: what turns the raw noise into the 0-1 lerp alpha. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGUtilsExNoiseShapingDetails Shaping;

	/** What Min and Max are, and so what is written. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lerp", meta = (PCG_Overridable))
	EPCGUtilsExNoiseLerpType ValueType = EPCGUtilsExNoiseLerpType::Scalar;

	/** Value where the shaped noise is 0. A constant, or a per-point attribute. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lerp", meta = (PCG_Overridable, DisplayName = "Min", EditCondition = "ValueType == EPCGUtilsExNoiseLerpType::Scalar", EditConditionHides))
	FPCGExInputShorthandNameDouble MinScalar = FPCGExInputShorthandNameDouble(FName(TEXT("Min")), 0.0, false);

	/** Value where the shaped noise is 1. A constant, or a per-point attribute. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lerp", meta = (PCG_Overridable, DisplayName = "Max", EditCondition = "ValueType == EPCGUtilsExNoiseLerpType::Scalar", EditConditionHides))
	FPCGExInputShorthandNameDouble MaxScalar = FPCGExInputShorthandNameDouble(FName(TEXT("Max")), 1.0, false);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lerp", meta = (PCG_Overridable, DisplayName = "Min", EditCondition = "ValueType == EPCGUtilsExNoiseLerpType::Vector2", EditConditionHides))
	FPCGExInputShorthandNameVector2 MinVector2 = FPCGExInputShorthandNameVector2(FName(TEXT("Min")), FVector2D::ZeroVector, false);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lerp", meta = (PCG_Overridable, DisplayName = "Max", EditCondition = "ValueType == EPCGUtilsExNoiseLerpType::Vector2", EditConditionHides))
	FPCGExInputShorthandNameVector2 MaxVector2 = FPCGExInputShorthandNameVector2(FName(TEXT("Max")), FVector2D::UnitVector, false);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lerp", meta = (PCG_Overridable, DisplayName = "Min", EditCondition = "ValueType == EPCGUtilsExNoiseLerpType::Vector", EditConditionHides))
	FPCGExInputShorthandNameVector MinVector = FPCGExInputShorthandNameVector(FName(TEXT("Min")), FVector::ZeroVector, false);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lerp", meta = (PCG_Overridable, DisplayName = "Max", EditCondition = "ValueType == EPCGUtilsExNoiseLerpType::Vector", EditConditionHides))
	FPCGExInputShorthandNameVector MaxVector = FPCGExInputShorthandNameVector(FName(TEXT("Max")), FVector::OneVector, false);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lerp", meta = (PCG_Overridable, DisplayName = "Min", EditCondition = "ValueType == EPCGUtilsExNoiseLerpType::Vector4", EditConditionHides))
	FPCGExInputShorthandNameVector4 MinVector4 = FPCGExInputShorthandNameVector4(FName(TEXT("Min")), FVector4(0, 0, 0, 1), false);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lerp", meta = (PCG_Overridable, DisplayName = "Max", EditCondition = "ValueType == EPCGUtilsExNoiseLerpType::Vector4", EditConditionHides))
	FPCGExInputShorthandNameVector4 MaxVector4 = FPCGExInputShorthandNameVector4(FName(TEXT("Max")), FVector4(1, 1, 1, 1), false);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lerp", meta = (PCG_Overridable, DisplayName = "Min", EditCondition = "ValueType == EPCGUtilsExNoiseLerpType::Rotator", EditConditionHides))
	FPCGExInputShorthandNameRotator MinRotator = FPCGExInputShorthandNameRotator(FName(TEXT("Min")), FRotator::ZeroRotator, false);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lerp", meta = (PCG_Overridable, DisplayName = "Max", EditCondition = "ValueType == EPCGUtilsExNoiseLerpType::Rotator", EditConditionHides))
	FPCGExInputShorthandNameRotator MaxRotator = FPCGExInputShorthandNameRotator(FName(TEXT("Max")), FRotator(0, 360, 0), false);

	/**
	 * Required. Attribute or point property the lerp result is written to. Created (as Value Type) if it is a
	 * missing attribute; an existing attribute or property keeps its type when the result converts to it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (PCG_Overridable))
	FName OutputAttributeName = TEXT("$Density");

	/** Also write the shaped 0-1 noise (the lerp alpha) to its own attribute, for debugging or reuse downstream. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (PCG_Overridable))
	bool bWriteNoise = false;

	/** Optional. Double attribute that receives the shaped noise when Write Noise is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (PCG_Overridable, EditCondition = "bWriteNoise", EditConditionHides))
	FName NoiseAttributeName = TEXT("Noise");
};

class PCGUTILSPCGEXINTEROP_API FPCGUtilsExAssignValueFromNoiseElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
