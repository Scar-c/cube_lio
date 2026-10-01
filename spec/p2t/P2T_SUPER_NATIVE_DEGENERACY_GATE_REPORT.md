# P2-T Super-native degeneracy gate report

**Verdict: P2-T PASS — G1 REPRODUCES S2-LEVEL TUNNELD PERFORMANCE; G2 REJECTED**

The geometry-only G1 gate reproduces the always-weakest S2 result within 0.411 m and is bitwise deterministic over three TunnelD runs. The stricter geometry-derived G2 threshold is also deterministic, but reaches 214.184 m ATE and is not a viable selector. The gate uses Super eigensignals and sign-invariant temporal stability; it does not use COIN's absolute contribution threshold to decide activation.

## TunnelD selector results

| Mode | Gate policy | Runs | ATE RMSE | Gate active frames | Result |
| --- | --- | ---: | ---: | ---: | --- |
| S2 | Always use the smallest-eigenvalue direction | 3 | 3.135795 m | 1176/1176 supported frames | Reference reproduced |
| G1 | Activate weakest direction above NTU geometry q95 | 3 | 3.546209 m | 1171/1176 (99.57%) | Pass; within 0.411 m of S2 |
| G2 | Activate weakest direction above NTU geometry q99 | 3 | 214.183521 m | 1146/1176 (97.45%) | Rejected; deterministic regression |
| P2-R C1/S0 | Original COIN rule with XYZ fallback | 3 | 171.552635 m | — | Comparison baseline |

S2 matches P2-S exactly: all three P2-T runs have the same 3.135795 m ATE and trajectory SHA256 `1f72919449989a7b14918fa456015fc21ae470af239915a89096b369b669b311`. G1's three ATEs are identical and its trajectory SHA256 is `d71c1ab0b0e3cc542aed184fa9d1e9489b8a7880598a7403a2eb8c3df847bf63`. G2's three ATEs are also identical; its trajectory SHA256 is `3b7b7876179f0688cf407c4eb4752c65b6ce9ebb580b0923bbe945b8272e8a3c`.

All modes processed 1180 frames, accepted 1176 COIN frames, skipped the same four genuine IMU bracket gaps, used zero accepted per-point motion fallbacks, and kept the active feature count at 60. The G1 first-run fusion audit passed. The existing P2-R fixed-state fusion equivalence remains unchanged. Scan-end timing support remained within the P2-R tolerance.

G2 is nominally more conservative than G1, but it activates on 25 fewer accepted TunnelD frames in its own production run. G2's median newly selected centers rises to 34 versus 26 for G1, and its median per-frame NCC statistic is 0.753 versus 0.832 for G1. The runs associate the regression with the changed selector decisions but do not isolate a per-frame counterfactual. This is a deterministic failure of the frozen gate; no threshold was changed after observing it.

## Geometry-only signal and frozen thresholds

The confidence score is computed from ascending eigenvalues of the final Super translation information matrix `H_t^T H_t`:

```text
anisotropy = clamp(1 - lambda1/lambda3, 0, 1)
eigengap   = clamp((lambda2 - lambda1)/lambda3, 0, 1)
stability  = abs(dot(v1[t], v1[t-1])) when the prior frame is within 0.25 s
confidence = anisotropy * eigengap * stability
```

The first frame without temporal support gets stability and confidence zero. Eigenvector sign is removed by the absolute dot product. The formulas use geometry rows, timestamps, eigenvalues and eigenvectors only.

Before either gated production result was inspected, G1 was frozen at the 95th percentile of valid eee_01 photo-off confidence samples (**0.31913064578672057**) and G2 at the 99th percentile (**0.359189249724233**). eee_01 was the normal-scene calibration sequence; no GT, ATE or COIN contribution statistic entered either threshold.

| Photo-off geometry sequence | Median `lambda1/lambda2` | Median `lambda1/lambda3` | Median weakest-axis step | G1 activation | G2 activation |
| --- | ---: | ---: | ---: | ---: | ---: |
| TunnelD | 0.1522 | 0.1384 | 1.00° | 86.86% | 85.85% |
| NTU eee_01 | 0.5022 | 0.4219 | 1.61° | 5.02% | 1.00% |
| Shield1 | 0.8169 | 0.4206 | 7.83° | 8.73% | 7.03% |

On the S2 production trajectory, median `lambda1`, `lambda2`, and `lambda3` are 69.64, 444.99, and 532.50; median ratios are `lambda1/lambda2=0.1596` and `lambda1/lambda3=0.1350`. Median sign-invariant weakest-axis stability is 0.99990 (0.81° median step over supported adjacent frames). S2 fills all 60 feature slots; the median newly selected center count is 25 and median selected gradient magnitude is 40.41.

The production G1/G2 activation fractions differ from the photo-off estimates because COIN feedback changes Super's later geometry rows. The selector decision itself continues to use the current frame's geometry signal only. When a gate is inactive, G1/G2 deliberately use the original COIN weak-direction behavior as requested; the nonportable `contribution < 25` rule is not part of the gate confidence score.

## NTU and Shield1 checks

COIN image selection is currently scoped to TunnelD by the calibrated Ouster frontend and runner. NTU and Shield1 were therefore checked as photo-off Super geometry regressions with the G1 mode configured but COIN disabled. On NTU, three P2-T runs and a same-build original-selector control have identical trajectory SHA256 `50c981fef0e3e635786fbb86049acccdc01cdda0a0e5d9caecb076fa3fbb5f7e` and ATE 0.117438 m. This proves the selector configuration does not alter that supported photo-off path. Relative to the older P2-R archived run, ATE is higher by 0.001682 m (1.45%); that older binary was built from an earlier source state, so it is recorded as a reference difference rather than attributed to the P2-T gate.

Shield1's P2-T photo-off trajectory is byte-identical to the P2-R reference: ATE 168.191542 m and SHA256 `75d075506b8b5316c5b73972c432fcd8171c5257418008f03bb21c0273236adc`. The G1-configured, COIN-disabled run matches it exactly. These checks cover the shared estimator path; they do not claim an active COIN selector test on NTU or Livox Shield1.

## Projection interface boundary

P2-T adds the abstract `IntensityRepresentation` contract with `project(point)`, `sample_intensity(pixel)`, `sample_gradient(pixel)`, and `compute_residual(pixel, reference)`. `CoinIntensityRepresentation` documents how existing COIN projection and photometric sampling map to that contract. The adapter is not used by production COIN, so COIN projection, preprocessing, residual/Jacobian, NCC, lifecycle, scale, ESKF and fusion remain on their existing path. No Cubemap or other projection implementation was added.

## Interpretation and boundary

The weakest Super direction is stable and useful on TunnelD. G1 provides a compact geometry-only activation score and reproduces S2-level performance, while the more conservative q99 gate does not. The G1 gate activates on nearly every accepted TunnelD frame after production feedback, so this result does not establish a broadly selective gate across other COIN-capable normal image scenes. NTU and Shield1 currently exercise only the shared photo-off path. P2-T ends here; it does not begin Cubemap or projection ablation.
