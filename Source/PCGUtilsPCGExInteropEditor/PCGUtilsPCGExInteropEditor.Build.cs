// Copyright Max Harris

using UnrealBuildTool;

public class PCGUtilsPCGExInteropEditor : ModuleRules
{
    public PCGUtilsPCGExInteropEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "PropertyEditor",
                "InputCore",
                "PCG",
                "PCGExCollections",
                "PCGUtilsPCGExInterop"
            }
        );
    }
}
