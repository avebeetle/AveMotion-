# rlottie dirty flags do not prove static provenance

## Original limitation

The public `renderTree()` output exposes paths after runtime transforms and fresh
Telegram runtime trees set `ChangeFlagPath` on nearly every draw item. Therefore
neither upstream dirty bits nor equality across a few samples can prove that a
resource is asset-static.

Part 4 consequently treated every extracted resource as
`InstanceEvaluated`.

## Part 5 containment

Part 5 does not reinterpret those dirty flags. Instead it adds a documented,
additive metadata seam to the pinned Telegram runtime that exposes:

- the local path already retained before the paint-group transform;
- the separate affine transform;
- source-path staticness;
- staticness of trim/modifier and internal transform chains;
- base solid paint values and inherited opacity;
- staticness of the solid paint source properties.

Only resources explicitly proven by that metadata are promoted to
`AssetStatic`. Exact content comparison guards repeated source IDs. Any conflict
increments diagnostics and leaves the resource instance-scoped.

## Remaining limitation

The proof is currently discovered through the exact runtime tree, not an
independent compiled `MotionAssetModel`. Source IDs are transitional and based
on asset hash, layer index and local ordinal. Dynamic property overrides are not
exposed through the current AveMotion API; when introduced, they must invalidate
or bypass affected static proofs.

The next stage should extract explicit immutable asset tables with canonical
`GeometryId` and `PaintId` identities.
