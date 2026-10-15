# PCGUtils PCGEx Interop

PCGUtilsPCGExInterop provides optional interoperability between PCGUtils and PCGExtendedToolkit (PCGEx).

It depends on both plugins. PCGUtils itself remains independent of PCGExtendedToolkit, and this repository is not part of the standalone PCGUtils Fab distribution.

Target engine: Unreal Engine 5.8.

## Collection Category Handle

`FPCGUtilsCollectionCategoryHandle` (`Collections/PCGUtilsCollectionCategoryHandle.h`) references one category of a PCGEx asset collection, the way `FDataTableRowHandle` references one row of a data table. It holds a soft `Collection` and a `Category` name.

In the Details panel, assigning a collection turns `Category` into a dropdown of the categories that collection's entries use, with None at the top to clear it. A category no entry uses is shown in red, with a tooltip naming the collection. The dropdown re-reads the collection each time it opens, so recategorizing entries in the collection editor shows up without reopening the panel.

From C++, `IsValid()` checks that the collection loads and an entry uses the category, `LoadCollection()` loads it, and `GetCategoryNames(Collection)` returns the sorted category list the dropdown shows.
