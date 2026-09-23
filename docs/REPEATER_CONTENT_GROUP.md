# Telegram-compatible Repeater content groups

Part 17 extends the Part 16 direct Repeater seam to a bounded content-group
subset while keeping Telegram as an exact oracle.

## Supported authored structure

```text
supported ShapeGroup
├── direct path-like nodes
├── nested direct ShapeGroups
│   ├── path-like nodes
│   ├── solid Fill and/or dashless solid Stroke
│   └── Transform
├── one or more solid Fill / dashless solid Stroke applications
├── exactly one Repeater
└── group Transform
```

The traversal follows Telegram's paint-item behaviour: paths accumulate within
the current group, nested groups are processed recursively, and each authored
Fill or Stroke creates a distinct paint application over the ordered active
path set.

## Resource separation

A paint application does not imply a unique geometry resource.

```text
ordered source paths + paint-space node
              -> RepeaterGeometryBinding
              -> one geometry identity

Fill node      -> one paint identity
Stroke node    -> one paint identity
```

Therefore a fill and stroke over the same paths can share one future
`ID2D1PathGeometry` while using separate brush/stroke-style resources.

## Coordinate spaces

For each source path:

```text
path world matrix
* inverse(paint-space world matrix)
= path-to-paint relative matrix
```

The combined canonical geometry is materialized in paint-local space. For each
Repeater copy:

```text
paint-relative matrix
* copy-local matrix
* repeater-parent world matrix
* root viewport matrix
= expected Telegram final matrix
```

The projection is accepted only when the generated local path, transformed
final path and final matrix agree with Telegram.

## Local solid paint evaluation

Fill and Stroke values are reconstructed from canonical property tables:

- color;
- opacity;
- stroke width;
- cap;
- join;
- miter limit.

Telegram-compatible byte conversion is used for RGBA. Stroke width is compared
after Telegram's matrix scale calculation. Dashless strokes only are accepted
in Part 17.

The item then publishes:

```text
local solid paint
+ separated Repeater/group opacity
+ optional local stroke
```

The final Telegram color, alpha, stroke width, cap, join and miter must match
before the local seam replaces the extracted state.

## Nested groups

Direct nested ShapeGroups are accepted when:

- their structural and transform ancestry is valid;
- all path-to-paint transforms are available;
- no unsupported modifier interrupts the group;
- every value is finite;
- Telegram parity succeeds.

The fixture contains 16 nested paint applications per sampled frame and all are
accepted without fallback.

## Identity and revisions

```text
geometry identity:
Asset + Repeater + paint-space group + ordered source paths

paint identity:
Asset + Repeater + authored Fill/Stroke node

copy state:
Instance + copy index + transform revision + opacity revision
```

Animated paint values make the paint instance-scoped. Paint animation does not
change geometry identity. Repeater transform or opacity animation changes only
per-copy presentation state.

## Fixture coverage

`tests/fixtures/repeater_content_group.json` contains four groups:

1. direct Rectangle with Fill and Stroke;
2. nested Rectangle and Ellipse groups with independent Fill/Stroke pairs;
3. two nested path groups with outer Fill and Stroke applications;
4. animated Fill/Stroke values with a Repeater.

Per source frame:

```text
Repeater groups:                    4
accepted paint/copy projections:   34
solid fill applications:           17
solid stroke applications:         17
nested paint applications:         16
animated paint applications:        6
unique shared geometry identities:  5
unique authored paint identities:  10
fallbacks/parity mismatches:         0
```

All 61 source frames are tested. Direct seek, sequential traversal and repeated
same-time evaluation produce identical cache identities and no redundant
geometry/paint updates. Prepared workspace capacity remains stable.

## Fail-closed exclusions

Part 17 does not accept:

- chained or nested Repeaters;
- Repeater combined with Trim or Merge operations;
- gradients;
- dashed strokes;
- masks, mattes or effects;
- unresolved precomposition attribution;
- unsupported nested modifiers.

These cases retain the established Telegram geometry and paint path.
