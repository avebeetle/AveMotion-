# Render-plan comparison

- Left: `docs/characterization/plan-samsung-clang/plan_manifest.tsv`
- Right: `docs/characterization/plan-telegram-clang/plan_manifest.tsv`
- Samples: **40**
- Missing: **0**

| Field | Identical | Different |
|---|---:|---:|
| `plan` | 19 | 21 |
| `topology` | 40 | 0 |
| `geometry_identity` | 23 | 17 |
| `paint_identity` | 33 | 7 |
| `presentation` | 27 | 13 |
| `frame` | 21 | 19 |
| `visible_items` | 40 | 0 |
| `geometry_updates` | 40 | 0 |
| `paint_updates` | 37 | 3 |
| `unsupported_items` | 40 | 0 |

## `plan` differences

| Asset | Sample | Left | Right |
|---|---|---|---|
| `1667-firework.json` | `p025` | `569c18ffbc7033be` | `dbcf2bd98cf950cb` |
| `1667-firework.json` | `p050` | `49e2df3cf2ed8c54` | `c0574c31ab9ca3ed` |
| `1667-firework.json` | `p075` | `800f10c684134f51` | `80c87ebfca655af0` |
| `ModernPictogramsForLottie_LoudMute.json` | `p025` | `8e654d68a7a06a8e` | `9c5b13f4ef47131c` |
| `ModernPictogramsForLottie_LoudMute.json` | `p075` | `ed5e73a94b90bd57` | `bde972ba7fb4c346` |
| `StickAndBall.json` | `p025` | `5d97120a69a1a5b8` | `262badf7cc644a53` |
| `StickAndBall.json` | `p050` | `d8bca128ebea11ae` | `480e1b5a39d4b797` |
| `StickAndBall.json` | `p075` | `c2d5bff192e00d1e` | `bf96d0a0bb95276f` |
| `StickAndBall.json` | `p100` | `58a996bff837b310` | `91729c06f03183a9` |
| `dynamic_path_test.json` | `p025` | `175d2bfca935133d` | `ff56b62a3f61afc0` |
| `dynamic_path_test.json` | `p050` | `1c819ada4c505776` | `51dc9b4e6979ead5` |
| `dynamic_path_test.json` | `p075` | `edbed2964dfe00f0` | `0400b9203066d5a1` |
| `gradient_animated_background.json` | `p025` | `415a39d612655952` | `90411f5ac2cd7384` |
| `gradient_animated_background.json` | `p050` | `d2eb39b71ac94f2d` | `f3b3aeb49ba138b0` |
| `gradient_animated_background.json` | `p075` | `74439016479abf39` | `3e04ce120cead62e` |
| `matte_two_item_with_lowerlayer.json` | `p000` | `8a1d67ae1a567857` | `92e2f75d9d154085` |
| `matte_two_item_with_lowerlayer.json` | `p025` | `8a1d67ae1a567857` | `92e2f75d9d154085` |
| `matte_two_item_with_lowerlayer.json` | `p050` | `8a1d67ae1a567857` | `92e2f75d9d154085` |
| `matte_two_item_with_lowerlayer.json` | `p075` | `8a1d67ae1a567857` | `92e2f75d9d154085` |
| `matte_two_item_with_lowerlayer.json` | `p100` | `8a1d67ae1a567857` | `92e2f75d9d154085` |
| `polystar_line_clockwise_trim.json` | `p075` | `37ac709346e13587` | `a9b7e0cc28504ae1` |

## `geometry_identity` differences

| Asset | Sample | Left | Right |
|---|---|---|---|
| `1667-firework.json` | `p025` | `03a56c44da25be16` | `2966e0cc18d783f5` |
| `1667-firework.json` | `p050` | `4966727ae3edf430` | `0fd6a1cf0d6812e2` |
| `1667-firework.json` | `p075` | `be7998ca8c3110bc` | `b06b980160bffa35` |
| `ModernPictogramsForLottie_LoudMute.json` | `p025` | `f9dbb695e1a2fb1c` | `ba90873e810db660` |
| `ModernPictogramsForLottie_LoudMute.json` | `p075` | `d84f7fb18c9bca56` | `41687316f436ae49` |
| `StickAndBall.json` | `p025` | `c7900e0e01eabe2d` | `aaa11aa6e2b0cb08` |
| `StickAndBall.json` | `p050` | `c6e53e4074ade48a` | `85dfbd3f267b496d` |
| `StickAndBall.json` | `p075` | `8a9171e097334ef2` | `8b054f24b175f3f4` |
| `dynamic_path_test.json` | `p025` | `13971dbc4abc7055` | `49087af4e914ffc9` |
| `dynamic_path_test.json` | `p050` | `1f6b71036fc42bef` | `283f5c66a8ce4666` |
| `dynamic_path_test.json` | `p075` | `7fae2206e14b33ad` | `5068451c721ab957` |
| `matte_two_item_with_lowerlayer.json` | `p000` | `ef4c14ad5db785fa` | `8354592218dbc2ad` |
| `matte_two_item_with_lowerlayer.json` | `p025` | `ef4c14ad5db785fa` | `8354592218dbc2ad` |
| `matte_two_item_with_lowerlayer.json` | `p050` | `ef4c14ad5db785fa` | `8354592218dbc2ad` |
| `matte_two_item_with_lowerlayer.json` | `p075` | `ef4c14ad5db785fa` | `8354592218dbc2ad` |
| `matte_two_item_with_lowerlayer.json` | `p100` | `ef4c14ad5db785fa` | `8354592218dbc2ad` |
| `polystar_line_clockwise_trim.json` | `p075` | `13039ca32f9e1915` | `1b75fc2b66406b99` |

## `paint_identity` differences

| Asset | Sample | Left | Right |
|---|---|---|---|
| `StickAndBall.json` | `p025` | `bb1b5ab6e5692074` | `9af041fbe39823b2` |
| `StickAndBall.json` | `p050` | `75684269667213e8` | `9af041fbe39823b2` |
| `StickAndBall.json` | `p075` | `31b8401d72a87a4b` | `9af041fbe39823b2` |
| `StickAndBall.json` | `p100` | `31b8401d72a87a4b` | `9af041fbe39823b2` |
| `gradient_animated_background.json` | `p025` | `beab30c0a0627705` | `4791bcfec21683d2` |
| `gradient_animated_background.json` | `p050` | `36e8ab28583d9c0e` | `05f36367d69f0248` |
| `gradient_animated_background.json` | `p075` | `f00d3d623fb367c7` | `c9b7890cd79b15b9` |

## `presentation` differences

| Asset | Sample | Left | Right |
|---|---|---|---|
| `1667-firework.json` | `p075` | `4d1cebe475ef2e94` | `f7165cbfb6d6cae9` |
| `StickAndBall.json` | `p025` | `b4a31e618bc4c48b` | `11c8fb4c6fefaae0` |
| `StickAndBall.json` | `p050` | `0298e2273bcf5592` | `ed9eb2da8ab14619` |
| `StickAndBall.json` | `p075` | `05a5eb4bf399f36d` | `fc33438dff66ef79` |
| `dynamic_path_test.json` | `p025` | `d6af2bf0b756fa18` | `d9ed9afb99a40e5e` |
| `dynamic_path_test.json` | `p050` | `33db4d6e3cf25335` | `f773a33788c2ceb0` |
| `dynamic_path_test.json` | `p075` | `13676bd001615aa2` | `e65ae95fb6c675ff` |
| `matte_two_item_with_lowerlayer.json` | `p000` | `de5a1faa31812623` | `833653e019d4aad0` |
| `matte_two_item_with_lowerlayer.json` | `p025` | `de5a1faa31812623` | `833653e019d4aad0` |
| `matte_two_item_with_lowerlayer.json` | `p050` | `de5a1faa31812623` | `833653e019d4aad0` |
| `matte_two_item_with_lowerlayer.json` | `p075` | `de5a1faa31812623` | `833653e019d4aad0` |
| `matte_two_item_with_lowerlayer.json` | `p100` | `de5a1faa31812623` | `833653e019d4aad0` |
| `polystar_line_clockwise_trim.json` | `p075` | `eb683534cbd0c94b` | `39ecf4086da9dca9` |

## `frame` differences

| Asset | Sample | Left | Right |
|---|---|---|---|
| `1667-firework.json` | `p075` | `59` | `58` |
| `1667-firework.json` | `p100` | `78` | `77` |
| `ModernPictogramsForLottie_LoudMute.json` | `p100` | `88` | `87` |
| `StickAndBall.json` | `p025` | `6` | `5` |
| `StickAndBall.json` | `p050` | `12` | `11` |
| `StickAndBall.json` | `p075` | `18` | `17` |
| `StickAndBall.json` | `p100` | `23` | `22` |
| `dynamic_path_test.json` | `p075` | `113` | `112` |
| `dynamic_path_test.json` | `p100` | `150` | `149` |
| `gradient_animated_background.json` | `p025` | `39` | `38` |
| `gradient_animated_background.json` | `p050` | `78` | `77` |
| `gradient_animated_background.json` | `p075` | `117` | `116` |
| `gradient_animated_background.json` | `p100` | `155` | `154` |
| `mask.json` | `p075` | `23` | `22` |
| `mask.json` | `p100` | `30` | `29` |
| `matte_two_item_with_lowerlayer.json` | `p075` | `113` | `112` |
| `matte_two_item_with_lowerlayer.json` | `p100` | `150` | `149` |
| `polystar_line_clockwise_trim.json` | `p075` | `113` | `112` |
| `polystar_line_clockwise_trim.json` | `p100` | `150` | `149` |

## `paint_updates` differences

| Asset | Sample | Left | Right |
|---|---|---|---|
| `StickAndBall.json` | `p025` | `1` | `0` |
| `StickAndBall.json` | `p050` | `1` | `0` |
| `StickAndBall.json` | `p075` | `1` | `0` |
