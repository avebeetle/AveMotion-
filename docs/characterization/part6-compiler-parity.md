# Part 6 compiler parity

Clang 17 and GCC 14.2 produced byte-identical manifests for every committed
characterization boundary:

| Variant | CPU pixels | Evaluated scenes | Render plans | Immutable model |
|---|---:|---:|---:|---:|
| Telegram | 40/40 | 40/40 | 40/40 | 8/8 |
| Samsung | 40/40 | 40/40 | 40/40 | not applicable |

Samsung does not expose the Telegram stable-source-ID extension, so Part 6 does
not build or compare a Samsung immutable model manifest.
