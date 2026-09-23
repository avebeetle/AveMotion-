# AveMotion Part 18.1 report — cross-platform vendor verifier hotfix

## Scope

Part 18.1 changes no Lottie semantics, canonical data, evaluator result,
geometry output, render plan or Direct2D rendering behavior. It fixes the one
platform-dependent test discovered by the first native Windows run of Part 18.

Version:

```text
0.18.1
```

## Native Windows evidence

The first external Windows/MSVC run established the important production gate:

```text
CMake configure                         passed
MSVC build                              47/47
Direct2D public-header test             passed
Direct2D production contract            passed
D3D11 WARP + real Direct2D smoke        passed
WARP capture                            produced and visually valid
Compiled functional/offline tests       all passed
```

The full suite reported one failure only:

```text
avemotion.vendor.verify
```

The Direct2D result was therefore valid before this hotfix; the failure was in
an integrity-test implementation, not in the source trees or renderer.

## Root cause

The previous verifier used:

```python
sorted(Path objects)
```

`PosixPath` ordering is case-sensitive, while `WindowsPath` follows
case-insensitive Windows path semantics. The vendored trees contain names whose
relative order changes under case folding, for example:

```text
CMakeLists.txt
cmake/...
```

The aggregate hash includes files sequentially. Identical paths and bytes in a
different traversal order therefore produced a different final digest.

This was proven by emulation on the clean delivered source trees. The
case-insensitive order generated exactly the two hashes reported by Windows:

```text
Samsung:
5e1115bf9fed2e246f6b1d6c5a42c4d98dcb38be7464e1995e161d03b3ebb479

Telegram:
7deaf9545524c395ee151e356ec1d035897778fe7b066ee94bbe95e87b30aae1
```

The expected case-sensitive canonical hashes remain:

```text
Samsung:
f710dd5f7fcbe789d88438d8d0a876d7bdf0bc55831548286d064cb3e7f18b84

Telegram:
5d03d1b9867a15f9fea916410e43dd6319eaea574ba4e0a2dee78d6f5d96dfb3
```

File counts and total byte counts were unchanged. This rules out CRLF
conversion, missing files, added files, source edits and archive corruption.

## Fix

The fingerprint format now explicitly defines traversal order as:

```text
relative path
→ POSIX slash form
→ UTF-8 bytes
→ case-sensitive bytewise sort
```

The verifier no longer sorts host-native `Path` objects.

A new CTest regression test creates deliberately case-sensitive path names and
proves that:

- canonical ordering and Windows-like case-folded ordering differ;
- the production verifier selects canonical ordering;
- file and byte counts remain correct.

CTest names:

```text
avemotion.vendor.ordering
avemotion.vendor.verify
```

## Console encoding

The Windows launcher now selects UTF-8 for CMD, PowerShell and Python output.
This does not affect paths passed to APIs, but keeps Cyrillic user-directory
names readable in CTest diagnostics and artifact messages.

## Regression boundary

Unchanged:

- Telegram and Samsung vendored bytes;
- upstream commit identities;
- all pixel, scene, model, property, geometry and render-plan goldens;
- Direct2D backend;
- WARP smoke image semantics;
- cache diagnostics;
- package API.

Changed only:

- project version `0.18.0` → `0.18.1`;
- platform-independent vendor traversal order;
- verifier-order regression test;
- Windows console encoding;
- retained native Windows evidence.

## Expected Windows rerun

From the Visual Studio Developer Command Prompt:

```bat
scripts\run_part18_windows.cmd
```

The full standalone suite now contains thirteen tests and should finish with:

```text
100% tests passed, 0 tests failed out of 13
[AveMotion Part 18.1] PASS
```
