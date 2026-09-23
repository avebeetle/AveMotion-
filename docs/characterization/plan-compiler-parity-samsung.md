# Render-plan compiler parity: Samsung comparison

The committed Samsung render-plan manifest was generated with Clang 17 and
reproduced byte-for-byte with GCC 14 across all 40 corpus samples.

```text
Clang == GCC: 40 / 40 plans
```

Samsung remains a comparison and hardening source; Telegram is the primary
behavioral baseline for AveMotion.
