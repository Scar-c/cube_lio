# Shield4 evaluator and path-length audit

Audited baseline: `f3060768abee4d2ee2343b07edf62346c7cd0f87`; source-review package: `71822c76fb3b2a4d4ddad69c536862ede24388a3`. The evaluator is frozen for this audit. Machine-readable calculations and source/input hashes are in [`evaluator_analysis.json`](../../artifacts/p3r/evaluator_analysis.json).

## Where the length and threshold come from

[`eval/evaluate.py`](../../eval/evaluate.py) computes position ATE and timestamp coverage. It does **not** calculate trajectory length or apply a 20% acceptance rule. The P3 result archiver, [`tools/offline/archive_p3_results.py`](../../tools/offline/archive_p3_results.py), uses the literal `501.43951893190064` as Shield4's GT path length, and compares ATE with `0.2 * length` using a strict `<` test. The archiver separately reports the estimated full trajectory length, but does not use that estimated length as the acceptance denominator.

This audit independently loads all 911 rows of `Shield_tunnel4.txt` and calculates:

`L_GT = sum(norm(GT_position[i+1] - GT_position[i])) = 501.43951893190064 m`.

The literal used by P3 exactly matches this sampled GT path length. It is the original, complete GT position path, not the estimated trajectory or a scale-aligned estimate. A rigid SE3 alignment preserves the length of a fixed sequence of points; restricting the sequence to timestamp matches can change its length, which is also checked below.

## ATE association and alignment

For Shield4, the evaluator transforms each estimated body pose to the Leica prism position using the frozen gamma-device calibration lever arm. It associates the shorter timestamp array to the nearest timestamp in the longer array with `max_diff=0.1 s`, inclusive, zero time offset. There is no GT interpolation for Shield4. Nearest matching is not constrained to be one-to-one: this run has 898 pairs, 898 unique GT indices, and 897 unique estimate indices.

One proper rigid rotation and translation are fitted to all associated position pairs using the SVD/Kabsch formulation in `ntu_author.align_se3`. Scale is fixed at one. ATE RMSE is the root mean square of Euclidean position errors after this single global alignment. GT quaternions are unused; estimated orientation affects the prism lever-arm correction. This is position ATE, not a rotational-error test or segmentwise drift measure.

The audit script independently implements nearest association, prism conversion, and no-scale Kabsch fitting; it does not call `evaluate.py` or `ntu_author.align_se3`. Its RMSE differs from the saved P3 result by only `-1.42e-14 m`.

## Manual Shield4 calculation

| Quantity | Value |
|---|---:|
| Complete GT rows / duration | 911 / 328.420000 s |
| Estimated output frames | 3394 |
| Timestamp match pairs | 898 |
| Full sampled GT path | 501.439518932 m |
| Matched GT path | 501.438018932 m |
| Raw estimated full path | 677.827033173 m |
| Prism-corrected estimated full path | 684.758265575 m |
| Prism-corrected matched estimate path, before / after rigid alignment | 639.212481851 / 639.212481851 m |
| Independent ATE RMSE | 90.394224597 m |
| Primary limit: `0.2 × full GT path` | 100.287903786 m |
| ATE / full GT path | 18.026944663% |
| Margin below primary limit | 9.893679189 m |
| Sensitivity limit: `0.2 × matched GT path` | 100.287603786 m |
| Estimate tail after GT end | 13.447298 s |

The complete-GT and matched-GT denominators differ by only 1.5 mm; both produce PASS. Thus neither estimated-path inflation nor the complete-versus-matched GT length explains the P3 PASS. The last 13.447 seconds of the estimate extend beyond GT coverage and are unscored. The PASS describes the evaluated overlap, not demonstrated accuracy throughout that tail.

## Validity and cross-dataset interpretation

The 20% test is arithmetically valid for the Shield4 prompt criterion and has a fixed, estimate-independent denominator. It is a relatively permissive acceptance rule: the passing ATE remains about 90 m. Passing it cannot establish that Cubemap alone caused the benefit.

The repository does not apply a single 20% benchmark rule across all datasets. NTU uses dataset-author interpolation with strict brackets and duplicate-position removal, while stress datasets use nearest timestamp matching and different fixed sensor-to-prism corrections. GT sampling, coverage, noise and trajectory shape also differ. ATE/GT-length may be reported descriptively with those details, but a nominal 20% cutoff is not a calibrated cross-dataset equivalence of accuracy.

Reproduction command, already executed for the archived P3 trajectory:

```bash
python3 tools/offline/audit_p3r_attribution.py evaluator
```

It asserts agreement with the saved RMSE, timestamp-match count, and P3 GT-length literal. None of the evaluator source, GT, association threshold, lever arm, or acceptance denominator is modified by this audit.
