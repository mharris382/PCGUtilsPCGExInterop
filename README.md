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

## SelectEx | By Filters

A DynMesh Selector that selects the vertices passing a stack of PCGEx point or data filters. It replaces
`DynMesh To Points -> Uber Filter -> Select From Vertex IDs` with one node, and works anywhere a Selector does
(Build DynMesh Selection, any process node's Selector pin, Selection to Painter, ...).

Each evaluation turns the mesh's vertices into one point each and runs the filters on them, ANDed in PCGEx order
exactly as Uber Filter runs them:

- **$Position** is the vertex position, converted to world space by the mesh's PCG target actor by default
  (**Positions To World**), so spatial filters such as Inside Path compare against world-space paths.
- **$Color** is the vertex color (white when the mesh has no color overlay).
- **$Rotation** points its Up (Z) axis along the vertex normal (**Rotation From Normal**), so dot and angle filters
  can use `$Rotation.Up`.
- **$Seed** comes from the position, as PCG seeds new points. Every other point property keeps its default.
- The mesh's **@Data** domain is carried over. A filter reads mesh attributes with an explicit `@Data.` selector,
  and data (collection) filters judge the mesh as a whole. There are no point-domain attributes.

**Invert Selection** selects the vertices that fail. The result is a vertex selection; edge and triangle consumers
get it through the standard domain conversion. Cluster (vtx/edge) filters are refused with an error, since a mesh's
vertices are plain points.

PCGEx filters initialize against a PCGEx context, while a Selector runs inside whichever node consumes it. Each
evaluation therefore runs the filters in a short-lived PCGEx context that borrows the consumer's execution source
and node, so target-actor lookups resolve and filter errors land on the consuming node.

Palette family: `SelectEx | `, the PCGEx twin of PCGUtils' `Select | `.

## Filter : Mesh Occlusion

A PCGEx point filter that passes points whose ray hits the DynMeshes on its **Occluders** pin (**Result** =
Occluded), or points whose ray is clear (Unoccluded). It is the point twin of PCGUtils' Select | By Mesh Occlusion
and uses the same ray tests (`PCGUtilsDynMeshOcclusion::FOccluderSet`). Use it in any PCGEx node that takes
filters, such as Uber Filter.

- **Direction**: a constant (default up) or a per-point vector attribute (`$Rotation.Up` by default). **Flip**
  reverses it. **Direction In Point Space** turns it by each point's rotation.
- **Max Distance**: a constant or a per-point attribute, in world units. 0 means unlimited; negative values are read
  as their magnitude.
- **Origin Offset** starts each ray a little along its direction, so a point lying on an occluder does not hit it.
- **Occluders Are Actor Local** (advanced, on by default) places each occluder by its own target actor.

A point with a zero direction fails. With no occluder triangles the filter follows its **Missing Data** policy. The
node is not cached, because its spatial indexes are built from that run's meshes. Listed as
`PCGEx | Filter : Mesh Occlusion` among the PCGEx filters.

## PointEx | Assign Value From Noise

One node for a chain that otherwise takes four or five: Uber Noise -> random offset -> clamp -> remap -> curve ->
scale by attribute -> Lerp(Min, Max, noise) -> write.

- **Noises** (required): PCGEx Noise3D definitions, stacked by Priority with their own weights and blend modes,
  exactly as Uber Noise stacks them. Sampled at each point's position.
- **Noise shaping**, in this order:
  - **Random Offset** (off): adds a random value in [-amount, +amount] per point, seeded by the point's seed and
    the node's Seed.
  - **Clamp** (on, 0-1): holds the offset noise inside [Clamp Min, Clamp Max].
  - **Remap** (on, band 0-1): the [Remap In Min, Remap In Max] band of the noise becomes 0 -> 1; anything outside
    is held at 0 or 1. A narrow band sharpens the noise, In Min above In Max inverts it, an equal pair is a hard
    step. **Use Remap Curve** then shapes the 0-1 value.
  - **Scale By Attribute** (off): multiplies by a per-point attribute (**Scale Attribute Name**), e.g. a falloff.
- **Lerp**: **Value Type** Scalar, Vector 2, Vector, Vector 4 or Rotator, with **Min** and **Max** each a constant
  or a per-point attribute. A Rotator lerps pitch, yaw and roll separately, so Min 0 / Max 360 yaw gives any heading.
- **Output Attribute Name** (`$Density`): attribute or point property to write; a missing attribute is created as
  the Value Type, an existing one keeps its type when the result converts (double to `$Density`'s float, Rotator to
  `$Rotation`'s quaternion). **Write Noise** (off) also writes the shaped 0-1 noise to **Noise Attribute Name**.

The output is the input duplicated with these values written, so other attributes, tags and properties are kept.

## Filter : Shaped Noise

PCGEx's Filter : Noise with the same shaping steps in front of the comparison (random offset, clamp, remap band
and curve, scale by attribute), so a noise filter no longer has to be swapped out when it needs remapping. The
shaped noise is compared (default `>= 0.5`) against a constant or a per-point attribute. **Seed** drives the
random offset. Works in any PCGEx node that takes filters. Listed as `PCGEx | Filter : Shaped Noise`.

Both nodes share `FPCGUtilsExNoiseShapingDetails` (`Noise/PCGUtilsExNoiseShaping.h`), which also offers
`PCGUtilsExNoise::FFactoryNoiseGenerator` for sampling PCGEx noise in a plain PCG element.
