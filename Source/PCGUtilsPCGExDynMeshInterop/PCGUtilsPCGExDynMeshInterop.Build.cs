// Copyright Max Harris

using UnrealBuildTool;

public class PCGUtilsPCGExDynMeshInterop : ModuleRules
{
    public PCGUtilsPCGExDynMeshInterop(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // The elements here derive from PCGUtilsDynMesh settings classes and declare pins carrying PCGEx
        // match rule data, so both libraries are part of this module's public surface.
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "PCG",
                "PCGUtilsCore",
                "PCGUtilsDynMesh",
                "PCGExCore",
                "PCGExMatching"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
            }
        );
    }
}
