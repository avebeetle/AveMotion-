# AveMotion private corpus laboratory

Part 24 adds a reproducible laboratory for external `.tgs` and Lottie JSON
collections. It does **not** redistribute input animations. By default, reports
replace file names with stable content-derived aliases and contain only
metadata, validation, feature frequency, timing and memory estimates.

## Quick start

```bash
python scripts/run_part24_corpus_lab.py \
  --input-dir C:/stickers \
  --output C:/reports/avemotion-corpus
```

A ZIP can be supplied without manual extraction:

```bash
python scripts/run_part24_corpus_lab.py \
  --input-zip C:/stickers.zip \
  --output C:/reports/avemotion-corpus
```

The ZIP path rejects traversal, symlinks, more than 1000 entries and more than
64 MiB expanded content.

## Reports

- `corpus_manifest.tsv`: metadata, canonical-model size and support state.
- `corpus_issues.tsv`: typed diagnostics for application, Direct2D and Telegram profiles.
- `corpus_features.tsv`: occurrence and asset frequency by feature.
- `corpus_benchmarks.tsv`: load/model, first-frame, steady pipeline, CPU reference and memory estimates.
- `corpus_decision.tsv`: deterministic next-feature ranking.
- `corpus_summary.txt`: aggregate counts and top candidate.

## Privacy

Default aliases are content-derived, for example:

```text
asset-4f82d2a7a0bf2f11
```

The output never copies source assets. `--include-names` is explicit opt-in and
records only base file names.

## Ranking

```text
blockedAssets * 1000
+ occurrences * 25
+ supportWeight
- estimatedEffort * 50
```

This is an engineering triage aid, not an automatic product decision. The
result is marked preliminary until the corpus represents the intended product.
