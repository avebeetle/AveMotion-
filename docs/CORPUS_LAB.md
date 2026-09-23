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
- `corpus_benchmarks.tsv`: load/model, first-frame, steady pipeline, CPU reference,
  percentile timings, session diagnostics, workspace observations and memory estimates.
- `corpus_decision.tsv`: deterministic next-feature ranking.
- `corpus_summary.txt`: aggregate counts and top candidate.

The normal corpus reports use schema 2. Existing benchmark columns remain in
place. `*_us` columns use microseconds; `exact_scene_median_ns`,
`exact_scene_p95_ns`, `pipeline_median_ns` and `pipeline_p95_ns` use nanoseconds.
The median is the middle sorted value, or the average of the two middle values.
P95 uses the nearest rank: sorted position `ceil(0.95 * N) - 1`.

`--samples` (default 60, range 1–10000) counts measured steady frames.
`--warmup-samples` (default 20, range 0–10000) runs between the first
measured sample and the measured steady loop. The first sample is frame 0;
warm-up and measured steady frames independently use `sample % totalFrames`.
The four percentile columns include the first frame and measured steady
frames, and exclude warm-up. The sampled workload
totals (`projected_items`, `source_draw_items`, `unsupported_draw_items`)
also include first plus measured frames and exclude warm-up. Asset
classification and validation do not depend on these sample counts.

`exact_scene_*_ns` comes from the runtime's exact scene evaluation timer.
`pipeline_*_ns` measures one evaluator → instance exact/model evaluation →
projector → planner call. Diagnostics snapshots occur immediately before and
after that call, outside its wall-clock interval. This is an application
pipeline observation, not a CPU raster render. `cpu_render_median_us` is a
separate oracle measurement after the diagnostic phases.

The `{setup,first,steady}_{metadata,scene,model,cpu}_sessions` columns count
new reference sessions by role. `setup_model_samples` counts model samples
during asset preparation; `first_scene_samples` and `steady_scene_samples`
count exact scene samples in their respective phases. Setup starts with the
asset load and ends after model, instance, evaluator and projector preparation;
separate load-timing runtimes are excluded. First and steady values are
counter deltas. Warm-up is excluded from all three phases. The current runtime
creates fresh scene sessions for sampled frames, so nonzero scene counts are
expected baseline observations. These counters do not imply session reuse.

`{evaluator,projector}_{after_prepare,after_warmup,after_measured}_{retained_bytes,storage_generation}`
records each workspace's retained storage and generation at those boundaries.
The after-warm-up observation follows both the first measured frame and the
warm-up loop; the steady diagnostic delta starts after this observation.
These are workspace observations, not process allocation counts. Player's
storage generation is checked over 1,000 stable ticks in its runtime test.
The planner's existing diagnostics and repeat/forget tests check update and
revision behavior, but planner allocation stability remains unverified because
it exposes no allocation or capacity metric.

On Windows, `--memory-instances 1`, `16`, or `64` switches to an observational
memory run and requires exactly one input asset. It prepares the model, creates
the requested number of live instances, evaluates frame 0 at 128×128 on each,
then writes `memory_observation.tsv` with the privacy alias, instance count,
current process working set and process peak working set in bytes. Every
instance remains live through the process memory query. Both byte values
include the entire process and are observations, never CI pass/fail thresholds.
The mode fails explicitly on non-Windows platforms. The Python runner forwards
`--memory-instances` and `--warmup-samples`. For `--skip-build`,
`--executable PATH` selects an existing binary directly, including one outside
the preset build directory. It is rejected without `--skip-build`.

The output directory is created if needed. Existing unrelated files and input
assets remain in place; only named report files are overwritten.

## Privacy

Default aliases are content-derived, for example:

```text
asset-4f82d2a7a0bf2f11
```

The output never copies source assets. `--include-names` is explicit opt-in and
records only base file names. Memory mode uses the same content-derived alias.

## Ranking

```text
blockedAssets * 1000
+ occurrences * 25
+ supportWeight
- estimatedEffort * 50
```

This is an engineering triage aid, not an automatic product decision. The
result is marked preliminary until the corpus represents the intended product.
