// Copyright Max Harris

using UnrealBuildTool;

public class PCGUtilsPCGExInterop : ModuleRules
{
    public PCGUtilsPCGExInterop(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core"
            }
        );

        // The module does not expose an interop API yet. Keep both sides of the boundary private until one is needed.
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "PCG",
                "PCGUtilsDynMesh",
                "PCGExMatching"
            }
        );
    }
}
