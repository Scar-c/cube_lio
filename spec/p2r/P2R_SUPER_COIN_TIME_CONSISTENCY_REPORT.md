# P2-R Super–COIN time consistency report

**Verdict: P2-R NO-GO — TIMING FIXED BUT COIN DOES NOT RESCUE SUPER.** Ouster time support, accepted-frame acquisition coverage, fixed-state fusion algebra, and three-run determinism pass. Faithful COIN raises TunnelD ATE from the corrected geometry baseline of **152.831403 m** to **171.552635 m**. This is a completed negative result, not a reason to advance to P3 or Cubemap.

## Scope and reference

The branch starts at `b493d593cb56bd07adb255779aaaf20f551b3621`. The same pinned TunnelD bag, GT, evaluator, COIN source, and configurations are identified by SHA in `artifacts/p2r/reference_identity.json` and `build_identity.json`. Evaluation uses SE(3) alignment without scale and nearest timestamps within 0.1 s. CUBE and P2A information scaling are off; the pinned COIN `photo_scale=0.00095`, measurement variance `0.001`, 5×5 patches, NCC, lifecycle, weak-direction selection, raw-intensity residual and approximate source Jacobian remain in use.

**Algorithm-semantic parity** means constructing COIN observations by the pinned frontend and feature rules. **State/trajectory parity** would require the two independent estimators to produce numerically identical states. Super-LIO owns its IMU propagation, geometry map, covariance, and ESKF, whereas official COIN-LIO runs on FAST-LIO2. Their production acquisition-time poses and `T_Li_Lk` are therefore not expected to match numerically. C1 applies COIN's measurement construction to Super's own propagated history. A different production `T_Li_Lk` is not by itself a defect.

## Timing repair and gate

Historical Super set Ouster `lidar.end_time` from the last geometry point visited under `filter_rate: 3`. On R0, the full valid raw maximum was later by a median **3.900 ms**; the propagated history ended a median **5.567 ms before even that old end**. All 1179 normal R0 frames had unsupported acquisition points, median **11,335** and total **12,807,677** candidate per-point fallbacks. The defect came chiefly from last-visited-point semantics; 29 frames also lost the raw maximum through geometry subsampling.

The corrected Ouster rule takes the maximum timestamp over all raw points with non-NaN XYZ and range ≥ `/preprocess/blind` (0.65 m here), independently of geometry sampling. IMU synchronization waits to the corrected end and interpolates the end sample only between real ordered samples no more than 50 ms apart. Overlapping scan acquisition intervals use retained, rigidly rebased Super propagation history. [Timing semantics](TIMING_SEMANTICS.md) states the exact rule and limitation. Other LiDAR types retain their original deskew path.

The final G1 audit has zero scan-end offset on all 1180 normal frames. Of those, 1176 have complete IMU support and zero unsupported candidate points. Four have real wide IMU brackets (135.8–192.7 ms), explicitly marked `imu_bracket_gap_or_order_invalid`. The geometry-only diagnostic counts 22,380 unsupported candidate points **on those four gaps only**; C1 skips their entire photo observation. C1's 1176 accepted frames have zero per-point fallback, finite supported acquisition transforms, and median COIN-minus-Super scan end of about −1 ns (range about ±0.12 µs). All full runs finish without abort.

## Geometry and faithful COIN outcome

| Run | Photo | Frames | ATE RMSE, m | Trajectory SHA256 |
| --- | --- | ---: | ---: | --- |
| G0, frozen historical timing | off | 1179 | 113.919295548 | `a07250898a5bd75fe247554edaaad786f5e6aa8ccf77d24767ae5068bc6d5b24` |
| G1, corrected timing and overlap history | off | 1180 | 152.831403200 | `9bfbc7d278f1177529b8a43b84f4c655e84a17625af144e68f29c0b1a44fcfff` |
| C1, G1 plus faithful COIN | on | 1180 | 171.552634930 | `29300418ccbc2331acaf15850d39ae7a93ef47cfafd8289e459344116ab984f5` |
| Official COIN oracle, context | on | — | 0.500161363 | — |

G1 is **38.912108 m worse** than G0. C1 is **18.721232 m worse** than G1, so it is `FAIL` under the prompt's C1 classification. G1 path length is 585.520 m; C1 is 669.835 m. Both use the same 1180-timestamp vector. This also shows that fixing the support bug does not by itself solve Super's TunnelD geometry degeneracy.

Photo-off regressions: NTU `eee_01` is **Ouster**, so an Ouster-only timing fix may change it. It changes from 0.118875639 m to 0.115755773 m over the same 3981 frames. Shield1 is **Livox** and retains its 168.191541666 m ATE, 5418 frames and byte-identical historical trajectory. The regression record is in `g1_geometry.json`.

## Fusion and repeatability

The first accepted fixed-state photo audit contains 675 scalar rows from 27 valid patches. Explicit `H_photo, r_photo` gives the production information form `A += (0.00095² / 0.001) HᵀH`, `b += (0.00095² / 0.001) Hᵀr`, factor **0.0009025**. Column order is right-local rotation then global position. The positive `Hᵀr` sign follows the pinned COIN correction row, rather than analogy with Super's geometry residual. Relative differences between explicit and production photo A/b are **1.02×10⁻¹⁵ / 1.50×10⁻¹⁵**; the float accumulator differences remain within the documented rounding bounds. The gate passes.

Three sequential, complete C1 runs each produce 1180 poses, 1176 used photo frames, four identical gap skips, the same ATE and the same trajectory and diagnostics SHA256. Timestamp vectors are identical; maximum translation and quaternion-chord differences are both **0**. There was no parameter sweep or COIN-default change after the first accepted result. Earlier exploratory C1 runs with unsupported overlapping scan starts were excluded from the three-run validation; the retained-history fix was applied before this final G1 and these three C1 runs.

## D1–D6 diagnosis and remaining gap

- **D1 fusion algebra:** PASS on the same accepted rows, with the source-derived correction order, sign, scale and variance. This establishes internal accumulator equivalence; it cannot prove that Super's broader estimator has the same behavior as FAST-LIO2.
- **D2 photo quality:** 1176 accepted frames, 1175 with positive photo rows, median 60 active patches, 50 valid patches, 1250 scalar rows, 53 finite NCC samples, median frame NCC 0.8165 and residual RMS 33.66 intensity units. Median NCC rejects: 12. Four full-frame skips are the IMU gaps; no accepted fallback occurs.
- **D3 state convention:** Super's ESKF updates the first six state components as right-local rotation and global additive position; the COIN row is reordered to match. The extrinsic includes the pinned COIN sensor-origin offset correction. P2's local row matched the pinned official source formula to about 1.82×10⁻¹². The separate strict residual finite-difference gate remains **failed** because that pinned formula approximates the global vertical projection and central image gradient. No exact Jacobian replacement or sign tuning was made here. This audit does not establish numerical production pose parity.
- **D4 iterative update:** Super calls `coin_->add(pose, A, b)` inside each ESKF observation callback, so rows are relinearized at each current pose while the image/features remain fixed during the update. Feature lifecycle advances after the KF update. This is internally consistent with the current Super callback, but its iteration trajectory and covariance differ from the official estimator; a causal attribution needs a separate controlled test.
- **D5 geometric weak directions:** Final Super point-to-plane world normals feed the translation-row eigenanalysis and COIN threshold rule. In 1116/1176 accepted frames the selector reports three directions, its fallback to LiDAR XYZ after no weak global direction was selected; only 60 frames select one weak direction. This is a concrete mechanism-gap candidate because the intended complementarity is usually absent, but these counts alone do not prove it causes the ATE failure. The threshold and source selection rule were kept frozen.
- **D6 time gaps:** Four real IMU-gap frames are skipped with an explicit reason. Their count is too small to explain away the 1176 accepted frames and poor C1 result. The retained/rebased overlap history is an internal approximation that merits a focused ablation before attributing the remaining gap to COIN itself.

The timing, fusion and determinism gates pass; the mechanism outcome does not. Stop at P2-R NO-GO. No P3 projection ablation, Cubemap integration, or GT-driven tuning was performed.
