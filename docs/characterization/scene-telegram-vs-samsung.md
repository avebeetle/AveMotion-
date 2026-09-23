# Evaluated-scene comparison

- Left: `tests/golden/scene-telegram.tsv`
- Right: `tests/golden/scene-samsung.tsv`
- Samples: **40**
- Missing: **0**

| Field | Identical | Different |
|---|---:|---:|
| `scene` | 12 | 28 |
| `topology` | 27 | 13 |
| `geometry` | 20 | 20 |
| `paint` | 35 | 5 |
| `frame` | 21 | 19 |
| `path_verbs` | 27 | 13 |
| `path_points` | 35 | 5 |

## `scene` differences

| Asset | Sample | Left | Right |
|---|---|---|---|
| `1667-firework.json` | `p000` | `07b1119db5dc0dd0` | `a1ff2ee61c0fbb48` |
| `1667-firework.json` | `p025` | `6ff6610c7ac5c8a7` | `0844c83a66e676f6` |
| `1667-firework.json` | `p050` | `176f449fea04294d` | `95916549a172505b` |
| `1667-firework.json` | `p075` | `346ca5f88d9f0b45` | `dd06b345120ad86d` |
| `1667-firework.json` | `p100` | `18a959ad7f41b3fd` | `b6b99a66e081bf3d` |
| `ModernPictogramsForLottie_LoudMute.json` | `p000` | `ac9842f70dd8e659` | `5485af218b316269` |
| `ModernPictogramsForLottie_LoudMute.json` | `p025` | `7cec1f97fb32a75f` | `a0ab8191733e196d` |
| `ModernPictogramsForLottie_LoudMute.json` | `p050` | `0c282e4054d7608b` | `6d6a3f76b4656c80` |
| `ModernPictogramsForLottie_LoudMute.json` | `p075` | `2fe7e6be2ef0b207` | `bb3e316b4e023dcb` |
| `ModernPictogramsForLottie_LoudMute.json` | `p100` | `aff0d784392977b0` | `baf7e24f123b2c90` |
| `StickAndBall.json` | `p025` | `e01d2f7543ba6964` | `23695ec21a8e1479` |
| `StickAndBall.json` | `p050` | `085c726a102a92c8` | `5a580926b4fd9914` |
| `StickAndBall.json` | `p075` | `bcbae23d493ce577` | `a92db5fc7365719a` |
| `dynamic_path_test.json` | `p025` | `0909baaa628b98f6` | `b21a927a892797ee` |
| `dynamic_path_test.json` | `p050` | `e167746b6df23856` | `88da28fe5cb36f91` |
| `dynamic_path_test.json` | `p075` | `1bfc35a3e4920fb8` | `a43cb1c0b07b9fc6` |
| `gradient_animated_background.json` | `p025` | `cd601bed3d1b0571` | `82b615838ee91aed` |
| `gradient_animated_background.json` | `p050` | `69df5c98311eb1bb` | `9c2f7cdf5fa7eef5` |
| `gradient_animated_background.json` | `p075` | `75454bc734f1ab40` | `90bcf35078407a24` |
| `mask.json` | `p025` | `5fd4a71553add8b0` | `fdefb36c1e0c3a97` |
| `mask.json` | `p050` | `fc522c0fbf0b2097` | `4c88734a4af37e72` |
| `mask.json` | `p075` | `af74f54230b579c6` | `82bfe171abb920db` |
| `matte_two_item_with_lowerlayer.json` | `p000` | `eb925e1282f25b90` | `f016b89b11ca1522` |
| `matte_two_item_with_lowerlayer.json` | `p025` | `eb925e1282f25b90` | `f016b89b11ca1522` |
| `matte_two_item_with_lowerlayer.json` | `p050` | `eb925e1282f25b90` | `f016b89b11ca1522` |
| `matte_two_item_with_lowerlayer.json` | `p075` | `eb925e1282f25b90` | `f016b89b11ca1522` |
| `matte_two_item_with_lowerlayer.json` | `p100` | `eb925e1282f25b90` | `f016b89b11ca1522` |
| `polystar_line_clockwise_trim.json` | `p075` | `fadc2ab390e9ac63` | `59f15bb38cc3f9c7` |

## `topology` differences

| Asset | Sample | Left | Right |
|---|---|---|---|
| `1667-firework.json` | `p025` | `8335222bdd05d92e` | `498f9aa59916dfd6` |
| `1667-firework.json` | `p050` | `4f9a18fcda180109` | `562a252f981ac725` |
| `1667-firework.json` | `p075` | `4f9a18fcda180109` | `562a252f981ac725` |
| `ModernPictogramsForLottie_LoudMute.json` | `p025` | `91dfcf27d573b181` | `82d0deca23dab0bb` |
| `ModernPictogramsForLottie_LoudMute.json` | `p075` | `63a420f8d602230f` | `d35b417fb93abc5d` |
| `mask.json` | `p025` | `18dfae7ecbeeaf41` | `63e85ede1f05c9d3` |
| `mask.json` | `p050` | `18dfae7ecbeeaf41` | `63e85ede1f05c9d3` |
| `mask.json` | `p075` | `18dfae7ecbeeaf41` | `63e85ede1f05c9d3` |
| `matte_two_item_with_lowerlayer.json` | `p000` | `df1ad0c673509bc6` | `38bce7101ea3c54d` |
| `matte_two_item_with_lowerlayer.json` | `p025` | `df1ad0c673509bc6` | `38bce7101ea3c54d` |
| `matte_two_item_with_lowerlayer.json` | `p050` | `df1ad0c673509bc6` | `38bce7101ea3c54d` |
| `matte_two_item_with_lowerlayer.json` | `p075` | `df1ad0c673509bc6` | `38bce7101ea3c54d` |
| `matte_two_item_with_lowerlayer.json` | `p100` | `df1ad0c673509bc6` | `38bce7101ea3c54d` |

## `geometry` differences

| Asset | Sample | Left | Right |
|---|---|---|---|
| `1667-firework.json` | `p025` | `656094cf1b9ec70d` | `f8f4c4acd482394b` |
| `1667-firework.json` | `p050` | `a25b84e12138ad58` | `08d2fdec7810e016` |
| `1667-firework.json` | `p075` | `c710e2aa84a286c5` | `a4ac357fecf94d33` |
| `ModernPictogramsForLottie_LoudMute.json` | `p025` | `876201b9904e783a` | `e3ecbf80734b33d2` |
| `ModernPictogramsForLottie_LoudMute.json` | `p075` | `2ab646f230de36db` | `decf9c1a2d604f43` |
| `StickAndBall.json` | `p025` | `05884e8fa7e586b8` | `bf0662e773cfd3be` |
| `StickAndBall.json` | `p050` | `85675f2ae7c7e647` | `a9a87fbf543a236e` |
| `StickAndBall.json` | `p075` | `47bde6f6c469091d` | `a915f986cdcb8bf8` |
| `dynamic_path_test.json` | `p025` | `84169be847d5e21c` | `8195186f2691b904` |
| `dynamic_path_test.json` | `p050` | `7073f3b62eb82245` | `547bc84b7edee73f` |
| `dynamic_path_test.json` | `p075` | `d824d8a2c2b241fe` | `4aa1685c9c98c0e2` |
| `mask.json` | `p025` | `15c6262570b39500` | `a39072a24f771376` |
| `mask.json` | `p050` | `ed4ae07440b4c2ae` | `e1d9c221e7538680` |
| `mask.json` | `p075` | `8a7c49d7a338bd48` | `9937882619b6beb1` |
| `matte_two_item_with_lowerlayer.json` | `p000` | `37de92e16b643fbe` | `bbd4a3ecbbd1fe7b` |
| `matte_two_item_with_lowerlayer.json` | `p025` | `37de92e16b643fbe` | `bbd4a3ecbbd1fe7b` |
| `matte_two_item_with_lowerlayer.json` | `p050` | `37de92e16b643fbe` | `bbd4a3ecbbd1fe7b` |
| `matte_two_item_with_lowerlayer.json` | `p075` | `37de92e16b643fbe` | `bbd4a3ecbbd1fe7b` |
| `matte_two_item_with_lowerlayer.json` | `p100` | `37de92e16b643fbe` | `bbd4a3ecbbd1fe7b` |
| `polystar_line_clockwise_trim.json` | `p075` | `84f8e66218cc3e50` | `e4f6a468f14c62bd` |

## `paint` differences

| Asset | Sample | Left | Right |
|---|---|---|---|
| `StickAndBall.json` | `p025` | `9a74978966d68341` | `213778a25ca07618` |
| `StickAndBall.json` | `p050` | `9a74978966d68341` | `518cd5779fc2f650` |
| `gradient_animated_background.json` | `p025` | `47121a96fad7858e` | `4ca74fa470e6bb32` |
| `gradient_animated_background.json` | `p050` | `463a8fd04c776746` | `3728899f39de2c43` |
| `gradient_animated_background.json` | `p075` | `d83dedefab96dd23` | `131c31e2af9c949f` |

## `frame` differences

| Asset | Sample | Left | Right |
|---|---|---|---|
| `1667-firework.json` | `p075` | `58` | `59` |
| `1667-firework.json` | `p100` | `77` | `78` |
| `ModernPictogramsForLottie_LoudMute.json` | `p100` | `87` | `88` |
| `StickAndBall.json` | `p025` | `5` | `6` |
| `StickAndBall.json` | `p050` | `11` | `12` |
| `StickAndBall.json` | `p075` | `17` | `18` |
| `StickAndBall.json` | `p100` | `22` | `23` |
| `dynamic_path_test.json` | `p075` | `112` | `113` |
| `dynamic_path_test.json` | `p100` | `149` | `150` |
| `gradient_animated_background.json` | `p025` | `38` | `39` |
| `gradient_animated_background.json` | `p050` | `77` | `78` |
| `gradient_animated_background.json` | `p075` | `116` | `117` |
| `gradient_animated_background.json` | `p100` | `154` | `155` |
| `mask.json` | `p075` | `22` | `23` |
| `mask.json` | `p100` | `29` | `30` |
| `matte_two_item_with_lowerlayer.json` | `p075` | `112` | `113` |
| `matte_two_item_with_lowerlayer.json` | `p100` | `149` | `150` |
| `polystar_line_clockwise_trim.json` | `p075` | `112` | `113` |
| `polystar_line_clockwise_trim.json` | `p100` | `149` | `150` |

## `path_verbs` differences

| Asset | Sample | Left | Right |
|---|---|---|---|
| `1667-firework.json` | `p025` | `222` | `240` |
| `1667-firework.json` | `p050` | `1356` | `1458` |
| `1667-firework.json` | `p075` | `1356` | `1458` |
| `ModernPictogramsForLottie_LoudMute.json` | `p025` | `43` | `44` |
| `ModernPictogramsForLottie_LoudMute.json` | `p075` | `43` | `44` |
| `mask.json` | `p025` | `11` | `12` |
| `mask.json` | `p050` | `11` | `12` |
| `mask.json` | `p075` | `11` | `12` |
| `matte_two_item_with_lowerlayer.json` | `p000` | `31` | `64` |
| `matte_two_item_with_lowerlayer.json` | `p025` | `31` | `64` |
| `matte_two_item_with_lowerlayer.json` | `p050` | `31` | `64` |
| `matte_two_item_with_lowerlayer.json` | `p075` | `31` | `64` |
| `matte_two_item_with_lowerlayer.json` | `p100` | `31` | `64` |

## `path_points` differences

| Asset | Sample | Left | Right |
|---|---|---|---|
| `matte_two_item_with_lowerlayer.json` | `p000` | `55` | `82` |
| `matte_two_item_with_lowerlayer.json` | `p025` | `55` | `82` |
| `matte_two_item_with_lowerlayer.json` | `p050` | `55` | `82` |
| `matte_two_item_with_lowerlayer.json` | `p075` | `55` | `82` |
| `matte_two_item_with_lowerlayer.json` | `p100` | `55` | `82` |
