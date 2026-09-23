# Render-plan compiler parity: Telegram baseline

The committed Telegram render-plan manifest was generated with Clang 17 and
reproduced byte-for-byte with GCC 14 across all 40 corpus samples.

```text
Clang == GCC: 40 / 40 plans
```

The comparison covers plan, topology, geometry-identity, paint-identity and
presentation fingerprints, update counts, draw-item counts and feature flags.
