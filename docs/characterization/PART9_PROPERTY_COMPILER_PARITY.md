# Part 9 property/world compiler parity

The Telegram property characterizer was built and run with both Clang 17 Debug
and GCC 14 Release against the same eight-asset persisted corpus.

All three files are byte-identical:

```text
c67ccd48ea6bc401be307be6a0a6353f967d6dfc624accededaba66e8961a20f  Clang output
c67ccd48ea6bc401be307be6a0a6353f967d6dfc624accededaba66e8961a20f  GCC output
c67ccd48ea6bc401be307be6a0a6353f967d6dfc624accededaba66e8961a20f  committed golden
```

The broader parity suite additionally covers the dedicated auto-orient fixture,
213 source-frame samples, spatial cubic position and composition-local world
matrices. It compares AveMotion output field by field against an independent
Telegram `VMatrix` reference path.
