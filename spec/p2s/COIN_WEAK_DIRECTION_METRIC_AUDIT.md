# COIN weak-direction metric audit

## Source rule and reproduction

At pinned official COIN-LIO `76729cc4feb3649cbd79d28f82d9f62a2c82889b`, `h_geo_last` is the final post-geometry translation Jacobian block. The code eigendecomposes `H_tᵀH_t`; for each normalized row it adds `|h_i·v_d|` only when that value is greater than 0.5. A direction is weak when the accumulated value is less than `n_uninformative` (default 25). If no direction passes, feature selection receives LiDAR XYZ. The local implementation was applied without changing this rule.

Replaying the exact archived official row matrices reproduces weak directions in **1014/1185 frames** (85.6%), matching P2's shadow trace. The trace uses the official accepted geometry rows; row norms are effectively 1.0. The mean/p95 per-frame row counts, eigenvalues, absolute and per-row contributions are in `artifacts/p2s/official_geometry_signal.csv` and `geometry_signal_summary.json`.

The original rule is not invariant to row replication. The new fixed-matrix fixture confirms that for `H_dup=[H;H]`, eigenvalue ratios and eigenvector axes stay the same, every absolute contribution doubles, and contribution divided by row count is unchanged. The test also checks deterministic weakest-axis selection up to eigenvector sign and the pure-gradient selector's stable tie order and cap.

## Official and Super distributions

On final G1 geometry rows, Super has a median **971** rows per frame versus official COIN's **576** (1.69×). That difference contributes to the absolute scale, but does not explain the selector gap alone:

| Statistic, smallest-eigenvalue axis | Official COIN | Super G1 |
| --- | ---: | ---: |
| Median absolute contribution | 8.83 | 56.64 |
| Median contribution per row | 0.0192 | 0.0580 |
| Median `λ1/λ3` | 0.0712 | 0.1384 |
| Median 10° normal-bin entropy | 3.47 nats | 3.90 nats |

The Super weakest-axis contribution per row is about three times the official value, so dividing by `N_geo_rows` still leaves it above the official weak-direction range. The eigenvalue ratio and broader angular occupancy also indicate a less concentrated Super normal distribution. The evidence supports a **combination**: Super has more accepted rows and its rows support a different, more balanced translation information shape. The first fact raises the absolute sum; the second changes the per-row projection score. It is not accurate to attribute the 95% XYZ fallback to row count alone.

## Predeclared normalized threshold

Before the Super selector production runs, the candidate threshold was fixed to the median `contribution/N_geo_rows` over official direction/frame cases where the pinned source says `contribution < 25`. There are 1015 such direction samples across the 1014 official weak frames. The derived median is **0.0160959751**, rounded in the implementation to **0.01609598**. This threshold was not selected against GT or Super ATE.

The normalized detector selects a geometry direction on only **21/1180** Super G1 frames, versus 61 frames under the original absolute rule on those exact G1 rows. In the shadow selector it still falls back to XYZ on 1155 of 1176 accepted photo frames. Production S3 reaches **166.388 m**, only 3.0% below C1/S0 and remains above 10 m; its repeat gate does not trigger. Row normalization removes the direct duplicate-row scaling, but the official-derived threshold does not make the two frontends' directional distributions comparable enough to recover the missing directions.

This metric audit identifies non-portability of the threshold and a distribution difference. It does not claim identical weak axes should occur across separate estimators.
