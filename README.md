# PCGUtils PCGEx Interop

PCGUtilsPCGExInterop provides optional interoperability between PCGUtils and PCGExtendedToolkit (PCGEx).

It depends on both plugins. PCGUtils itself remains independent of PCGExtendedToolkit, and this repository is not part of the standalone PCGUtils Fab distribution.

Target engine: Unreal Engine 5.8.

## Collection Category Handle

`FPCGUtilsCollectionCategoryHandle` (`Collections/PCGUtilsCollectionCategoryHandle.h`) references one category of a PCGEx asset collection, the way `FDataTableRowHandle` references one row of a data table. It holds a soft `Collection` and a `Category` name.

In the Details panel, assigning a collection turns `Category` into a dropdown of the categories that collection's entries use, with None at the top to clear it. A category no entry uses is shown in red, with a tooltip naming the collection. The dropdown re-reads the collection each time it opens, so recategorizing entries in the collection editor shows up without reopening the panel.

From C++, `IsValid()` checks that the collection loads and an entry uses the category, `LoadCollection()` loads it, and `GetCategoryNames(Collection)` returns the sorted category list the dropdown shows.

## PainterEx | Noise

A PCGUtils Painter that samples a stack of PCGEx Noise3D definitions (Perlin, Simplex, Worley, FBM, ...) at every
DynMesh vertex: Uber Noise, but writing a Painter value per vertex instead of an attribute per point. It replaces
`DynMesh To Points -> Uber Noise -> Painter by Vertex ID` with one node, and plugs into any Painter consumer or
composite (Paint DynMesh Vertex Color, Painter | Blend, Painter | Blur, ...).

- **Noises**: any number of Noise3D definitions, stacked exactly as Uber Noise stacks them: stable by Priority,
  each with its own weight and blend mode. The generator is built from the stored definitions when the Painter is
  evaluated, because a Painter runs inside its consumer, long after its own node.
- **Value Type**: Scalar paints the noise's double value (the consumer broadcasts it to its Write Channels); Color
  paints its Vector4 value as RGBA on the enabled **Color Channels**.
- **Space**: World (default) samples each vertex's world position, matching Uber Noise on points from DynMesh To
  Points; DynMesh Local keeps the pattern fixed to the mesh as it moves.
- Uber Noise's Mutate mode has no setting here; blend the noise with another Painter through Painter | Blend.

Palette family: `PainterEx | `, the PCGEx twin of PCGUtils' `Painter | `, listed beside the PCGUtils Painters.
