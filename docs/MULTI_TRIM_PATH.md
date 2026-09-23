# Multi-path Trim Path implementation notes

## Goal

Part 15 extends the proven one-path Trim Path implementation without replacing
Telegram's mature fallback. The owned subset is deliberately narrow and fully
characterized.

## Data flow

```text
SourceNodeId[] in authored order
-> generate/evaluate each canonical local path
-> retain flattened source streams and per-path ranges
-> evaluate start/end/offset once
-> TrimmedPathCollection
-> transform each result into paint-local coordinates
-> concatenate in authored order
-> compare to Telegram local/final paths
-> publish only on exact parity
```

## Prepared workspace

A workspace belongs to one serialized evaluation stream or one MotionInstance.
It is move-only and not concurrently reusable. Its model fingerprint prevents
using storage prepared for another canonical asset.

```cpp
SourceGeometryProjectionWorkspace workspace;
projector.prepare(workspace);
projector.project(scene, properties, workspace);
```

The convenience two-argument `project()` remains for one-shot tools but creates
a temporary workspace and is not the steady-state API.

## Ordering

The canonical source-child order is preserved in the binding's `pathNodes`
range. This matches the observable `LOTPaintDataItem::mPathItems` order for the
supported direct-group subset and is verified by exact Telegram path parity.

## Safety

- all source ranges and IDs are validated;
- finite path coordinates and transforms are required;
- capacity arithmetic is bounded by canonical model limits;
- malformed or unsupported groups fail closed;
- parity rejection never overwrites Telegram geometry;
- wrapped Individual behaviour is explicit and diagnosed;
- stale cursor/history state cannot affect geometry identity.
