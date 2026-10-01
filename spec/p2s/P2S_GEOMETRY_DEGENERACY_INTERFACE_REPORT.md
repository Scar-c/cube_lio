# P2-S geometry degeneracy interface report

**Verdict: P2-S PASS — DEGENERACY INTERFACE MISMATCH IDENTIFIED**

**Supported sublabel: WEAKEST-DIRECTION SELECTION SUPPORTED**

On TunnelD, using Super's smallest-eigenvalue translation direction on every supported frame reduces faithful COIN ATE from **171.553 m** to **3.136 m**. The three production trajectories are byte-identical. This identifies the weak-direction selection interface as a major cause of P2-R's failed rescue. It does not show that Super and official COIN have numerically matching geometry states, weak axes, or acquisition transforms.

## Answers to the prompt

1. **Why do the weak-direction frequencies differ?** The pinned absolute contribution threshold marks an official direction weak in 1014/1185 frames (85.6%). Recomputing the same source rule on Super G1 rows marks a direction in 61/1180 frames (5.2%); the P2-R C1/S0 stream had one weak direction in 60/1176 accepted frames and XYZ fallback in 1116. Super supplies 1.69× the median row count, but its weakest-axis contribution is also about 3× larger per row. Its median `λ1/λ3` is 0.138 versus 0.071 official, with higher normal-bin entropy. The difference comes from both accepted-row count and normal/correspondence distribution.

2. **Is `n_uninformative=25` sensitive to more than row count?** Yes. Absolute contribution scales with repeated rows, while the per-row projection distribution remains different between the frontends. The official-derived normalized threshold 0.01609598 still identifies a Super weak direction in only 21/1180 G1 rows. The original threshold's hidden sample-count dependence is real, but row count alone is not the full explanation.

3. **Does row normalization make the signals comparable?** It makes the score invariant to duplicating every row, as verified by a synthetic fixture. It does not make these measured distributions comparable: the Super weakest-axis median contribution/row is 0.0580, against 0.0192 official, and the predeclared S3 normalized selector mostly falls back to XYZ.

4. **Does bypassing the detector help?** Yes. Pure image-gradient S1 gives 127.788 m, a 25.5% improvement over C1/S0, and is better than G1's 152.831 m. It remains far above 10 m.

5. **Does always using Super's weakest eigenvector help?** Strongly. S2 gives 3.135795 m across three sequential runs, a 98.2% reduction from C1/S0. Each run has 1176 accepted photo frames, four genuine full-frame IMU-gap skips, zero accepted per-point fallback, and identical trajectory and diagnostic hashes.

6. **Does the official-derived normalized detector help?** No material rescue. S3 gives 166.388 m (3.0% better than C1/S0, still above 10 m) and selects a weak direction on only 21/1180 G1 frames. It therefore did not meet the predeclared 10% repeat threshold.

7. **Does the P2-R overlap rebase affect geometry?** The archived before/after audit shows a material trajectory change: corrected geometry-only ATE changes from 139.739 m to 152.831 m, and the maximum frame translation difference is 38.301 m. The older run cleared history and used the prior raw-point path for unsupported acquisition points; it is not the requested corrected-IMU reconstruction H1. Its binary/source state also differs. A clean Super-native corrected-anchor IMU reconstruction was not safe to implement without redesigning propagation, so the intended H0/H1 causal comparison remains unresolved. The result does not attribute all G1 degradation to the rebase.

8. **How should the remaining gap be classified?** The selector ablation supports a degeneracy-interface mismatch as a major cause: S2 changes only the supplied direction policy and improves C1 dramatically. S1 also improves, while S3 does not. The history-reconstruction issue remains a separate unresolved factor, and weaker NCC/survival statistics under S2 warn against treating its shadow image statistics as a localization metric. P2-S nevertheless meets its PASS gate through a non-GT selector adaptation with three-run deterministic production improvement.

## Frozen setup and controls

The branch starts at `9e03a7c48cd7631a7c5f367d36d012b6730b038c`. P2-R timing, Super geometry and ESKF, COIN image calibration/preprocessing, 5×5 patches, NCC/lifetime, photometric residual/Jacobian, scale 0.00095 and variance 0.001 remain fixed. The only production input varied is the direction set supplied to the complementary feature selector. The S3 threshold was derived from official traces before Super production results. GT is used by the existing evaluator only after each run.

The fixed-G1 shadows for S0–S3 all reproduce the same G1 trajectory SHA256 `9bfbc7d278f1177529b8a43b84f4c655e84a17625af144e68f29c0b1a44fcfff`. Thus the selector policy does not alter photo-off geometry. Selected-center Jaccard, image-region distribution, gradient scores, active-patch survival, NCC and residual statistics are archived in `selector_shadow_summary.csv` and `selector_overlap.json`. Shadow residual RMS alone is not treated as localization quality.

| Run | Selector | Full runs | ATE RMSE | Change vs C1/S0 |
| --- | --- | ---: | ---: | ---: |
| G1 | photo off | 1 | 152.831 m | baseline |
| C1/S0 | official threshold, XYZ fallback | 3 | 171.553 m | baseline |
| S1 | pure gradient | 3 | 127.788 m | −25.5% |
| S2 | always weakest eigenvector | 3 | 3.136 m | −98.2% |
| S3 | normalized official-derived threshold | 1 | 166.388 m | −3.0% |

The repeat gate was declared as at least 10% improvement over C1/S0 or ATE ≤10 m. S1 and S2 each received three sequential full runs; S3 stayed above 10 m and below the 10% improvement cutoff, so no extra S3 runs were triggered. S1 and S2 each match bitwise across frame count, timestamp vector, full trajectory and COIN diagnostics. Translation and quaternion-chord differences are zero. S3 has one production run; its production repeatability was not measured.

The inherited P2-R fixed-state fusion equivalence remains PASS. S1's first run also records a fresh PASS. No photo formula, scale or variance changed. The pure-gradient and weakest-axis variants keep the 60-feature cap, deterministic ordering, zero accepted fallback, and four explicit IMU-gap skips.

## History caveat and next boundary

The overlap-history rebase is associated with a material geometry trajectory change, but the available no-rebase run did not reconstruct acquisition states from corrected Super IMU propagation. Keep the size of that effect separate from the selector result. A later Super-native history reconstruction can resolve it without using official FAST-LIO2 states.

P2-S does not start projection ablation or Cubemap work. The measured outcome already answers the interface question: selecting Super's weakest translation direction can provide the missing complementary constraint in this sequence. No GT tuning or threshold sweep was performed.
