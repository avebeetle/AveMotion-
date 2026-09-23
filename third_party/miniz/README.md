# miniz decompression dependency

AveMotion Part 22 vendors the `miniz.h` snapshot already present in the
Samsung rlottie comparison tree (`miniz.c 2.2.0`). It is compiled only inside
AveMotion's private TGS transport implementation with an AveMotion-private symbol prefix so it can coexist with the independent copy embedded in Samsung rlottie.

The production TGS decoder uses only:

- raw DEFLATE inflation (`tinfl_decompress`);
- CRC-32 (`mz_crc32`).

ZIP, stdio and zlib-wrapper APIs are disabled. No miniz type or function is
exposed through AveMotion public headers.

The upstream file declares itself public domain / Unlicense. See
`LICENSES/UNLICENSE-miniz.txt` and the notices embedded in `miniz.h`.

Pinned file SHA-256:

```text
2ebb203d5ec271847495cd8504bab7d681f6c90946ee948b58a6a2566353542c  miniz.h
```
