# World-transform hierarchy — Part 9

## Purpose

Part 9 introduces a backend-neutral composition-local world-transform buffer.
It is independent of `rlottie` at runtime and is intended to feed later scene,
render-plan and Direct2D replacement stages.

## Two parent relations

The canonical source graph may contain two distinct relations:

```text
structural parent
    containment, draw hierarchy and opacity inheritance

transform parent
    authored After Effects layer parenting for matrix inheritance
```

A layer may be structurally under a composition root while receiving its matrix
from another layer. That transform parent does not contribute opacity.

## Update order

The evaluator compiles a deterministic topological order over the union of both
relations. A node is evaluated only after every matrix and opacity dependency is
available. Invalid references, cross-composition edges and cycles reject the
evaluator.

## Per-node output

```text
localMatrix / localOpacity / localRevision
worldMatrix / worldOpacity / worldRevision
local state / world state
changed / worldChanged
```

`NotPresent` local transforms are treated as identity with opacity 1. Static
world state is retained only when the local state and both parent contributions
are static.

## Matrix convention

The canonical 3x2 affine representation follows the existing Telegram parity
convention. Composition is:

```text
world = local * parentWorld
```

This is validated field by field against an independent Telegram `VMatrix`
oracle for the committed corpus.

## Opacity convention

```text
worldOpacity = localOpacity * structuralParent.worldOpacity
```

A synthetic regression test proves that a layer transform-parent affects the
child matrix but not its opacity, while an ordinary shape-group child inherits
both matrix and opacity from the structural parent.

## Dirty behavior

A local change advances local revision. A parent-only change can advance the
child world revision without changing child local revision. Repeated same-time
evaluation advances neither.

## Current boundary

World values are local to each canonical composition. Precomposition
instantiation, time remapping and mapping a reusable composition into a parent
instance are deferred until the relevant scene-instancing stage.
