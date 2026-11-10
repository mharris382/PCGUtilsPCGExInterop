// Copyright Max Harris

using UnrealBuildTool;

public class PCGUtilsPCGExInterop : ModuleRules
{
    public PCGUtilsPCGExInterop(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // FPCGUtilsCollectionCategoryHandle names a PCGEx collection in its public header.
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "PCGExCollections",
                // The noise shorthand nodes name PCGEx noise, filter and shorthand types in their public headers.
                "Engine", // FRuntimeFloatCurve in the noise shaping details
                "PCG",
                "PCGExCore",
                "PCGExFilters",
                "PCGExNoise3D"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "PCGUtilsDynMesh",
                "PCGExMatching",
                "PCGUtilsCore"
            }
        );
    }
}
