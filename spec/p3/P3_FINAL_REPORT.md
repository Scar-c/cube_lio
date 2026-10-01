# P3 final report: intensity representation and cubemap

## Result

The P3 implementation adds a shared `IntensityRepresentation` boundary for COIN and raster measurements, then compares equirectangular and cubemap charts under the same Super-LIO update. All new P3 bag runs used the offline runner with 32 TBB threads. The Super estimator, ESKF, COIN residual/fusion path, and frozen G1 threshold were retained.

Shield4 meets the prompt's acceptance rule in one configuration: **cubemap + raw intensity + depth-gated IDW + all-use** gives **90.3942 m ATE RMSE**, below the strict limit of **100.2879 m** (20% of the 501.4395 m GT path). A second complete run has the exact same trajectory SHA and ATE. The pass is narrow: 18.03% of the GT path, 9.89 m below the limit. It is the only tested Shield4 photo mode below the limit. The evaluator matched 898 time pairs; the GT ends about 13.4 seconds before the bag and therefore does not score the final bag tail.

The principal result table reports RMSE in meters. `C0` is cubemap/raw with IDW off; `C1` is cubemap/raw with IDW on; `C2` is cubemap/IGM with IDW on. Spherical controls use the same raw or IGM channel and IDW setting as the corresponding cubemap arm.

| Dataset | Photo-off geometry | Cube raw C0 | Cube raw C1 | Sphere raw C1 | Cube IGM all-use | Cube IGM weakest |
|---|---:|---:|---:|---:|---:|---:|
| TunnelD | 152.831 | 129.922 | 126.161 | 138.785 | 152.304 | 159.089 |
| NTU `eee_01` | 0.117438 | 0.117444 | 0.120235 | 0.120936 | 0.115356 | 0.115284 |
| Shield4 | 9739.859 | 23838.603 | **90.394** | 35088.883 | 288.303 | 4773.974 |

Shield4 cubemap raw C1 with weakest selection produces 15079.570 m and fails. The raw C1 all-use result therefore passes without geometry weak-direction selection; in this Shield4 ablation, applying the frozen G1 selector makes the result much worse. The IGM weakest arm also fails. These selector effects vary by dataset: NTU's all-use and weakest IGM errors differ by only 0.000072 m, while both TunnelD CUBE+IGM selector variants have high ATE. The experiment supports a Shield4-specific all-use result, not a general claim that a geometry gate is never useful.

## What the results say about CUBE

The published CUBE-LIO contribution is the six-face angular representation paired with semi-dense IGM features and direct photometric optimization alongside geometry. IDW fills sparse angular raster holes so local image measurements can be formed; it does not add new LiDAR returns. The source review separates these paper-level facts from this repository's implementation choices in [`CUBE_DESIGN_REVIEW.md`](CUBE_DESIGN_REVIEW.md) and [`PROJECTION_DESIGN_REVIEW.md`](PROJECTION_DESIGN_REVIEW.md). The cited ICRA abstract and public slides are linked there.

This local ablation does **not** establish IGM as the reason for the Shield4 pass. On Shield4, cubemap/raw/C1 passes while cubemap/IGM/all-use is 288.303 m and fails. Removing IDW from the otherwise matched cubemap/raw arm changes ATE from 90.394 m to 23838.603 m. The result points to the combination of cubemap support and depth-gated IDW for this sequence, while the IGM channel is not robust under the current local implementation and frozen estimator.

Projection effects are sequence-dependent. With raw intensity and IDW enabled, cubemap is modestly better than equirectangular on TunnelD (126.161 vs 138.785 m) and effectively tied on NTU (0.120235 vs 0.120936 m); the gap is very large on Shield4 (90.394 vs 35088.883 m). TunnelD's two raw modes and both Shield4 spherical controls remain poor in absolute terms where no acceptance threshold was defined. No broad accuracy claim follows from those relative gaps.

The COIN control answers a different system-level question because its Ouster projection, preprocessing, patches, NCC lifecycle, and selector are part of its existing adapter. On TunnelD, the frozen COIN+G1 run gives 3.546 m; COIN gradient ranking without the G1 geometry gate gives 127.788 m. COIN's own `weakest` selector gives 3.136 m and is recorded separately because it is not the Super-native G1 gate. This contrast shows that direction selection can matter greatly for the COIN path, while the accepted Shield4 cubemap/raw/C1 path did not need the same weak-direction gate.

## Shield4 acceptance and baseline behavior

The photo-off geometry trajectory is a deterministic failure: **9739.859 m ATE**, a **55968.3 m** estimated path, and a maximum 94.37 m translation step per 10 Hz frame after frame 2047. An exact repeat has the same trajectory SHA. The bag uses Livox `CustomMsg` with per-point offsets and reflectivity; the gamma calibration confirms the frozen `/imu/data` topic and LiDAR-to-IMU transform. The `/livox/imu` stream is present but was not substituted. This experiment detects the divergence but does not isolate its cause inside the frozen geometry/ESKF path. Input assumptions and evaluator coverage are documented in [`SHIELD4_ADAPTATION.md`](SHIELD4_ADAPTATION.md).

The passing raw C1 estimate travels 677.8 m over the full bag, compared with the 501.4 m GT path. ATE uses the repository's fixed no-scale SE(3) alignment, gamma-to-Leica lever arm, and nearest timestamp association. The acceptance verdict follows the prompt's ATE-RMSE threshold and should be read with the limited matched interval and the longer estimated path in view. No calibration, time offset, estimator, or ESKF tuning was changed to obtain the result.

## Online/offline SHA consistency

The already-completed TunnelD G1 comparison answers the earlier online/offline question without another duplicate replay: the 4-thread P2-T reference and offline 32-thread run have byte-identical `trajectory.tum` SHA `d71c1ab0b0e3cc542aed184fa9d1e9489b8a7880598a7403a2eb8c3df847bf63`. COIN observations, timing audit, and evaluator output also match byte-for-byte. The compared estimator source hashes and shared inputs match. Evidence is in [`g1_cross_thread_parity.json`](../../artifacts/offline_infra/g1_cross_thread_parity.json).

P3's deterministic photo reduction uses fixed feature chunks accumulated in ascending order. The complete TunnelD CUBE+IGM all-use repeat pair and Shield4 cubemap/raw/C1 all-use pair each have identical trajectory SHAs; the Shield4 ATE is identical across its two passing runs. See [`determinism.json`](../../artifacts/p3/determinism.json).

## Implementation and verification

The COIN adapter implements the shared representation interface; its production measurement and residual path remains unchanged. Cubemap and generic equirectangular raster implementations expose projection, intensity/gradient sampling, residual, and validity operations. The offline CLI records projection, channel, selector, input hashes, source hashes, evaluator and binary hashes. The artifacts include the complete per-run identities and trajectory hashes; full raw runtime folders remain ignored under `runtime/`.

The build and targeted tests passed: `cube_offline_node`, `test_spherical_photo`, `test_cube_photo`, and `test_coin_feature_math`. The spherical test checks 2000 projection Jacobians, 300 residual pose Jacobians, seam sampling, periodic IDW, and pole rejection. The existing cubemap finite-difference suite reports maximum errors below `9e-9` for projection and `4e-9` for the residual pose Jacobian. The command ledger is [`commands.md`](../../artifacts/p3/commands.md), and compact results are available in [`baseline.json`](../../artifacts/p3/baseline.json), [`projection_results.json`](../../artifacts/p3/projection_results.json), [`selector_ablation.json`](../../artifacts/p3/selector_ablation.json), [`tunneld_results.json`](../../artifacts/p3/tunneld_results.json), [`ntu_results.json`](../../artifacts/p3/ntu_results.json), and [`shield4_results.json`](../../artifacts/p3/shield4_results.json).

P3 is complete at this boundary. No further optimization was started.
