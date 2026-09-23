# rlottie reference comparison

- Left: `docs/characterization/part6-gcc/samsung-gcc/manifest.tsv`
- Right: `docs/characterization/part6-gcc/telegram-gcc/manifest.tsv`
- Compared normalized samples: **40**
- Pixel-identical samples: **10**
- Pixel-different samples: **30**
- Assets with metadata differences: **8**
- Missing samples: **0**

## Metadata differences

| Asset | Left frames/duration | Right frames/duration |
|---|---|---|
| `1667-firework.json` | 79 / 2.602602481842041 | 78 / 2.5692358016967773 |
| `ModernPictogramsForLottie_LoudMute.json` | 89 / 1.4666666984558105 | 88 / 1.4500000476837158 |
| `StickAndBall.json` | 24 / 0.95833331346511841 | 23 / 0.91666668653488159 |
| `dynamic_path_test.json` | 151 / 5.0050048828125 | 150 / 4.9716382026672363 |
| `gradient_animated_background.json` | 156 / 10.333333015441895 | 155 / 10.266666412353516 |
| `mask.json` | 31 / 0.5 | 30 / 0.48333331942558289 |
| `matte_two_item_with_lowerlayer.json` | 151 / 5.0050048828125 | 150 / 4.9716382026672363 |
| `polystar_line_clockwise_trim.json` | 151 / 5.0050048828125 | 150 / 4.9716382026672363 |

## Pixel differences at normalized samples

| Asset | Sample | Left frame/hash | Right frame/hash |
|---|---|---|---|
| `1667-firework.json` | `p000` | 0 / `71651747eacf7610` | 0 / `6c4b6079d30872b8` |
| `1667-firework.json` | `p025` | 19 / `7d64f9e8b24c79e3` | 19 / `77d4d2050702178a` |
| `1667-firework.json` | `p050` | 39 / `b274038e9c54eb53` | 39 / `f00d1bf419190f4d` |
| `1667-firework.json` | `p075` | 59 / `bf333877831a0025` | 58 / `9c03bd029d7bf454` |
| `ModernPictogramsForLottie_LoudMute.json` | `p000` | 0 / `30f9e948998ddc28` | 0 / `c5621232b26bce2c` |
| `ModernPictogramsForLottie_LoudMute.json` | `p025` | 22 / `8658fe93e3f4e665` | 22 / `4a5a20711ef5dbe2` |
| `ModernPictogramsForLottie_LoudMute.json` | `p050` | 44 / `d59bf3c5ef2a09c9` | 44 / `03118ed1ccafce1c` |
| `ModernPictogramsForLottie_LoudMute.json` | `p075` | 66 / `ee5241aa63521f13` | 66 / `1ce9d4da6ded5b66` |
| `ModernPictogramsForLottie_LoudMute.json` | `p100` | 88 / `d0b8146d50c825f5` | 87 / `2bc7ec42fa6cec3e` |
| `StickAndBall.json` | `p000` | 0 / `009527092661b117` | 0 / `15d2d2bcfb6898df` |
| `StickAndBall.json` | `p025` | 6 / `0d47f97cd2aef47a` | 5 / `036a020a26c2f811` |
| `StickAndBall.json` | `p050` | 12 / `45190bcbec81ad98` | 11 / `feb11dddca8d9d41` |
| `StickAndBall.json` | `p075` | 18 / `ba16a8de6838431c` | 17 / `661da4df3b99c01f` |
| `StickAndBall.json` | `p100` | 23 / `9d5feacf0708d3a3` | 22 / `828c6342b69bd5cb` |
| `dynamic_path_test.json` | `p000` | 0 / `cad7cf9c3ef638a5` | 0 / `c515f2af662c8395` |
| `dynamic_path_test.json` | `p025` | 37 / `ace834ca442187b8` | 37 / `d8f58d05bea4c818` |
| `dynamic_path_test.json` | `p050` | 75 / `949c74f7b7b8ef8c` | 75 / `413db1e0177d3ed5` |
| `dynamic_path_test.json` | `p075` | 113 / `46f802fbd81bdc43` | 112 / `f8dc58660204ff29` |
| `dynamic_path_test.json` | `p100` | 150 / `756366731b6cfc84` | 149 / `fb7fcde7254ecfd3` |
| `gradient_animated_background.json` | `p025` | 39 / `c35bfd539d67eea0` | 38 / `ae8b6ce5a9f11e23` |
| `gradient_animated_background.json` | `p050` | 78 / `fa5bf038b09e3da2` | 77 / `a883e70352c27f00` |
| `gradient_animated_background.json` | `p075` | 117 / `5552d335f73b914e` | 116 / `6ef3cf19a7f02c16` |
| `mask.json` | `p050` | 15 / `7c723baf8d959aa7` | 15 / `c18ff53ef485dd81` |
| `mask.json` | `p075` | 23 / `2134baabd304f4ba` | 22 / `746f008319011518` |
| `matte_two_item_with_lowerlayer.json` | `p000` | 0 / `252feabaec4f63b5` | 0 / `8e57d5d7d6a45619` |
| `matte_two_item_with_lowerlayer.json` | `p025` | 37 / `252feabaec4f63b5` | 37 / `8e57d5d7d6a45619` |
| `matte_two_item_with_lowerlayer.json` | `p050` | 75 / `252feabaec4f63b5` | 75 / `8e57d5d7d6a45619` |
| `matte_two_item_with_lowerlayer.json` | `p075` | 113 / `252feabaec4f63b5` | 112 / `8e57d5d7d6a45619` |
| `matte_two_item_with_lowerlayer.json` | `p100` | 150 / `252feabaec4f63b5` | 149 / `8e57d5d7d6a45619` |
| `polystar_line_clockwise_trim.json` | `p075` | 113 / `86c71e63e4a6bf7d` | 112 / `e39c8fe6941cc475` |

