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
                "PCGExCollections"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Engine",
                "PCG",
                "PCGUtilsDynMesh",
                "PCGExMatching"
            }
        );
    }
}
