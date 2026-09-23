# Recording and reference session regressions

All fixtures in this directory are self-authored minimal regressions. They are
nested deliberately: the corpus remains the eight top-level JSONs in this
directory's parent plus the eight in `tests/corpus`.

The `recording-*` fixtures cover outer paint/trim bindings into skipped repeater
copies, nested and fractional repeaters, negative child time, unvisited children
whose constructor frame is visible, local frame -1 trim cache behavior,
all-zero dash patterns, and strokes returning to disabled constructor defaults.
The six initial skipped-content fixtures preserve the self-authored Part25C
scratch probe inputs; the remaining cases extend those inputs.

`dashed_stroke_session.json`, `translation-near-default.json`, `width.json` and
`opacity.json` are the existing Part25B exact-state regressions.

`late-overflow.json` uses finite authored values: frame 0 is valid, while
frame 1's held scale overflows evaluated path coordinates. It protects typed
ordinary/recording error parity, recovery to frame 0, and model preparation
retry without partial publication. It does not enter the top-level smoke corpus.
