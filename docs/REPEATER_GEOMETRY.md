# Telegram-compatible Repeater geometry

Part 16 introduced the first AveMotion-owned direct Repeater seam. Part 17
extends that proven base to nested direct content groups and multiple solid
fill/stroke paint applications.

## Core copy semantics

For copy `i`:

```text
multiplier = i + offset
scale      = pow(authoredScale / 100, multiplier)
copyMatrix = translate(position * multiplier)
             translate(anchor)
             scale(scale)
             rotate(rotation * multiplier)
             translate(-anchor)
opacity    = lerp(startOpacity, endOpacity, i / copies)
```

The exact Telegram `VMatrix` operation order and special 90/180/270 degree
branches are preserved.

Visibility follows the pinned implementation:

```text
visibleCopies = int(copies)
copyIndex >= visibleCopies -> opacity 0
```

## Part 17 content model

```text
one direct Repeater
+ supported direct/nested ShapeGroups
+ one or more solid Fill / dashless solid Stroke applications
```

Geometry is keyed independently from paint. Fill and Stroke over the same path
set share geometry identity but keep separate authored paint identities.

See `REPEATER_CONTENT_GROUP.md` for traversal, coordinate-space and parity
details.

## Acceptance gate

AveMotion replaces Telegram-extracted copy state only if all relevant values
match:

- canonical base local path versus Telegram local path;
- canonical path transformed by AveMotion versus Telegram final path;
- AveMotion-composed copy matrix versus Telegram `localToViewport`;
- AveMotion copy opacity versus Telegram separated opacity;
- local/final solid color;
- local/final stroke width, cap, join and miter;
- all values are finite and within configured copy limits.

Any mismatch leaves the complete Telegram result untouched.

## Current restrictions

- one direct Repeater per supported group;
- no chained/nested Repeaters;
- no Repeater + Trim/Merge combination;
- no gradients or dashed strokes;
- no masks, mattes or effects;
- no unresolved precomposition attribution.
