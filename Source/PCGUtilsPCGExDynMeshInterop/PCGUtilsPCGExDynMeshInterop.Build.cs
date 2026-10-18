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
                "PCGExMatching",
                // DynMeshEx | Merge appends meshes itself.
                "DynamicMesh",
                "GeometryCore",
                "GeometryFramework",
                "PCGGeometryScriptInterop",
                // PainterEx | Noise is a PCGUtils Painter fed by PCGEx Noise3D definitions.
                "PCGUtilsPainter",
                "PCGExNoise3D",
                "GeometryScriptingCore",
                // SelectEx | By Filters runs PCGEx point and data filters over a mesh's vertices.
                "PCGExFilters",
                // Vertex color helpers (PCGUtilsDynMeshAttributeHelpers).
                "PCGUtils"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
            }
        );
    }
}
