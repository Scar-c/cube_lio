# P2A — reference-guided photometric information consistency

**P2A NO-GO — AGGREGATE AUTHORITY NOT THE MAIN CAUSE.** All safety and determinism gates pass, but C60/C100/K100 worsen TunnelD despite lowering its weak-direction information to order one. NTU remains safe; Shield1 improves substantially but retains tens of metres of error. No P2B work is started.

Experimental source: `cd8f8e3c4f10f2691f49d2041faac64eab94f373`, branched from
exact P1 `8db5c84281c35e848f2212fc482b60bf4e2a694b`. The same binary was used
for every shadow and actual run. P0/P1 branches are frozen. Final report commit
is identified by the branch/closing response, avoiding a self-referential SHA.

## Reference sanity and authority

The unmodified official COIN-LIO executable works on the exact TunnelD bag:
**0.500161363 m ATE**, 1185 output frames, 1184 matches, 176.556 m estimated
IMU path. Archived post-race-fix COIN-BIEVR evaluates to **0.585353897 m** under
the identical P1 evaluator, with three byte-identical trajectories. Thus the
bag/GT convention admits a successful photometric reference; neither oracle
was used to choose budgets or perform GT tuning.

| Reference | Exact inspected SHA |
|---|---|
| Frozen production P1 | `8db5c84281c35e848f2212fc482b60bf4e2a694b` |
| Frozen P0 | `1b0698efe63fae09f9d498a285c3feb5b1b69cbd` |
| Super-LIO ROS1 baseline authority | `60b57aaac8dc397f80c56364e7ccb008c300cc29` |
| Official COIN-LIO main | `76729cc4feb3649cbd79d28f82d9f62a2c82889b` |
| COIN-BIEVR coin_bievr | `a518cec38a80b29154b6e70994cf0d3763141434` |

The [CUBE slides](https://www.docswell.com/s/scomup/59N8N9-2026-03-22-173102)
disclose Cubemap → IDW → Gaussian-derivative IGM → semi-dense high-response
features → direct reprojection residual. Their slide 18 reports TunnelD
COIN/CUBE **0.485/0.317 m** (original image visually verified). They do not
establish CUBE's weighting, noise or information-budget model. C60/C100/K100
are this repository's engineering ablations, not official CUBE behavior.

The exact source/config/archive audit and pinned citations are in
[REFERENCE_AUDIT.md](REFERENCE_AUDIT.md). COIN uses **60 patches of 5×5**, not
60 scalar measurements; BIEVR selects **100 intensity voxels**, not 100 scalar
residuals. Their literal residual scales 0.00095/0.001 are not transplanted.
The archive's supported all-valid directional median is **3.01056**, rather
than an unverified universal 1.7. Its voxel-normal direction differs from our
translation-Hessian eigenvector, so the ratios are contextual, not equivalent.

## Frozen scope and implemented equations

ESKF/IMU propagation/reset, geometry residual/weight, OctVox/HKNN, evaluators,
dataset/extrinsic YAMLs, Cubemap/IDW/IGM construction, residual/Jacobian,
feature lifecycle and robust gates are unchanged. The only geometry-site edit
passes the existing pose prior covariance into the photo diagnostic; no ESKF
equation or covariance computation is modified. Photo remains in the same
iterated observation as geometry.

At each current linearization, retain the P1 valid set, residual
`r_i = M_current(pi(p_i)) - M_reference_i`, analytic `J_i`, frozen MAD sigma,
hard robust gate, Huber factor and weight
`w_i = photo_weight * Huber(|r_i|/sigma) / sigma²`.

```text
Araw = sum_i w_i J_i^T J_i
braw = -sum_i w_i J_i^T r_i
C0:    Ap=Araw, bp=braw                         (legacy P1 accumulation)
C60:   alpha=min(1,60/Nvalid);  Ap=alpha*Araw; bp=alpha*braw
C100:  alpha=min(1,100/Nvalid); Ap=alpha*Araw; bp=alpha*braw
K100:  Ap=sum_selected w_i J_i^T J_i; bp=-sum_selected w_i J_i^T r_i
```

For empty sets alpha=1 and A/b=0. C60/C100 retain every valid semi-dense
residual, with alpha=1 below budget; sparse sensors are never boosted.
Scaling **both A and b after robustification** changes aggregate information
authority without changing individual normalized residuals, gates or Huber
classification. It is equivalent to scaling residual/Jacobian by sqrt(alpha)
*after* the fixed robust weights, not re-running robustification at a different
residual-domain scale.

K100 is explicitly a sparse diagnostic control. Sort currently valid features
by their retained birth/reference IGM response (the existing P1 high-response
ordering), descending; break ties by stable feature index. Greedily apply the
existing square radius-2 pixel suppression in the current projected face,
then stop at 100. Original weights remain; alpha=1. The feature cache itself
and replenishment are unchanged, so fewer than 100 can be used.

C0 retains the legacy P1 thread-local accumulation. Candidate policies use
the identical valid contributions with deterministic 64-row block sums and
a fixed block reduction order. Serial and parallel modes do exactly the same
arithmetic. This is algebraically robustified P1 accumulation; its double
rounding differs from the legacy sum by at most the measured ~1e-14 scale.
No production source or binary changed after shadow validation began.

## Shadow diagnostics and local-solve convention

Each frame and each IEKF observation produces four shadow rows. Full ignored
`runtime/p2a_shadow_DATASET/information.csv` records active/valid/used counts,
per-face counts, median/MAD/RMS, six pose and three translation eigenvalues
for geometry/photo, and every geometry translation eigenvector. For each
`v_k` it records `qg=v^T Ag_tt v`, `qp=v^T Ap_tt v`, `qp/max(qg,1e-9)`,
`sg=v^T bg_t`, `sp=v^T bp_t`. These are local directional diagnostics,
not a full marginalized observability proof.

For diagnostic increments, use the predicted pose prior covariance `P6` from
the unchanged ESKF, rotation displacement delta_theta and translation delta_t,
`G6=diag(I-0.5*hat(delta_theta),I)`, `P6k=G6*P6*G6^T`, and
`delta_prior=G6*[delta_theta,delta_t]`. Symmetrize P6k and, if needed, add
`max(0,1e-12-min_eig(P6k))*I` (the jitter is logged). Then:

```text
Iprior = inverse(P6k)
d_geo   = solve(Iprior+Ag,    bg-Iprior*delta_prior)
d_joint = solve(Iprior+Ag+Ap, bg+bp-Iprior*delta_prior)
d_photo = d_joint-d_geo
```

Eliminating the other 12 state dimensions yields this marginal pose-prior
form for pose-only measurements. Numerical approximations remain: this
diagnostic uses double 6×6 LDLT and Eigen AngleAxis rotation displacement;
production uses float 18×18 inversions and its own SO3 logarithm. It is a
local counterfactual at the *same* linearization, not an independent propagated
geometry trajectory or a replacement for the production ESKF. Translation
norm is in metres; rotation-vector norm is radians (small-increment angle).

The fixed support grid is 8×8 per face. Occupied faces/cells, used residuals
per occupied cell and the trace fraction in the top ceil(10%) occupied cells
are logged. C0/C60/C100 use all valid support; K100 uses selected support.
Residual median/MAD/RMS describe the full P1 valid set even for K100. The
existing upper middle order statistic is used for even-count medians. This
grid is a concentration/redundancy proxy, not an effective-sample-size proof.

Shadow runs contain 15,006 / 4,693 / 21,566 iterated observations for
NTU / TunnelD / Shield1. **All three trajectories are byte-identical to P1.**
Diagnostic versus production valid-count mismatch is zero; maximum raw A/b
relative error is below `7e-15`. Frozen ESKF/map/config/evaluator/image/test
hashes are recorded in [build_identity.json](../../artifacts/p2a/build_identity.json).

## Shadow safety gate

The gate script reads information CSVs and test results only; it never reads
GT/evaluation/ATE. All-iteration finite/PSD and no-authority-increase checks
passed for all datasets and policies, including each translation direction.
The relative PSD tolerance is `1e-10*max(1,trace(Araw))`; tiny negative subset
difference eigenvalues (worst about `-3.1e-9` absolute) are rounding at a
millions-scale Hessian, not increased information. C60/C100 are scalar
contractions and K100 is a nonnegative weighted subset. Serial/parallel A/b
and selected IDs are exact in the fixture.

“Substantial” is operationalized as at least 50% reduction of the last-iteration
TunnelD weakest-geometry-decile median ratio. Decile membership is frozen from
C0's last-iteration qg0, not reselected separately per shadow policy. All three
pass: reductions **90.03% / 83.38% / 69.68%**. NTU is unchanged for C60/C100
and unchanged/weaker for K100. Approval was fixed before actual estimator runs.
Full distributions for both all-iteration and last-iteration scopes are in
[shadow_summary.csv](../../artifacts/p2a/shadow_summary.csv); the audit decisions
and CSV/trajectory hashes are in [shadow_gate.json](../../artifacts/p2a/shadow_gate.json).

### Frozen-trajectory information and support

| Dataset | Policy | Mean Nvalid | Mean Nused | Median alpha | Weak-decile qp/qg | Median trace Ap/Ag | Median cells* | Residuals/cell* | Top10% trace* |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| eee_01 | C0 | 9.06 | 9.06 | 1.00000 | 0.002481 | 0.000186 | 5 | 1.667 | 0.544 |
| eee_01 | C60 | 9.06 | 9.06 | 1.00000 | 0.002481 | 0.000186 | 5 | 1.667 | 0.544 |
| eee_01 | C100 | 9.06 | 9.06 | 1.00000 | 0.002481 | 0.000186 | 5 | 1.667 | 0.544 |
| eee_01 | K100 | 9.06 | 8.56 | 1.00000 | 0.002481 | 0.000176 | 5 | 1.625 | 0.547 |
| tunnel_d | C0 | 701.34 | 701.34 | 1.00000 | 14.601239 | 0.889092 | 125 | 5.648 | 0.489 |
| tunnel_d | C60 | 701.34 | 701.34 | 0.08547 | 1.455893 | 0.078887 | 125 | 5.648 | 0.489 |
| tunnel_d | C100 | 701.34 | 701.34 | 0.14245 | 2.426489 | 0.131478 | 125 | 5.648 | 0.489 |
| tunnel_d | K100 | 701.34 | 99.75 | 1.00000 | 4.427702 | 0.343010 | 54 | 1.852 | 0.503 |
| shield1 | C0 | 32.41 | 32.41 | 1.00000 | 0.033266 | 0.000481 | 13 | 2.343 | 0.427 |
| shield1 | C60 | 32.41 | 32.41 | 1.00000 | 0.031178 | 0.000480 | 13 | 2.343 | 0.427 |
| shield1 | C100 | 32.41 | 32.41 | 1.00000 | 0.033266 | 0.000481 | 13 | 2.343 | 0.427 |
| shield1 | K100 | 32.41 | 29.49 | 1.00000 | 0.031110 | 0.000442 | 13 | 2.182 | 0.426 |


*Support statistics use positive-information frames. Ratio/alpha statistics include all last-iteration frames. TunnelD C0 all-active-frame ratio median is 8.80320; 14.60124 is specifically the weakest geometry decile. These scopes must not be conflated.

C60/C100 preserve all 125 median occupied TunnelD cells and 5.648 residuals/cell; K100 reduces support to 54 median cells and 1.852 residuals/cell. The top 10% cells still contribute about half the information (0.489 C0, 0.503 K100). This establishes substantial concentration but does not prove spatial correlation or its causal role.

### Same-linearization predicted photo influence on TunnelD

| Policy | Trans P50 m | P90 m | P99 m | Max m | Rot P50 rad | P90 rad | P99 rad | Max rad |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C0 | 0.003668 | 0.006944 | 0.011503 | 0.028981 | 0.002763 | 0.008620 | 0.016289 | 0.027811 |
| C60 | 0.000920 | 0.001993 | 0.003849 | 0.016233 | 0.001631 | 0.005773 | 0.010706 | 0.021631 |
| C100 | 0.001316 | 0.002772 | 0.005177 | 0.020808 | 0.001943 | 0.006690 | 0.012498 | 0.024068 |
| K100 | 0.002468 | 0.004822 | 0.008127 | 0.020727 | 0.002512 | 0.008023 | 0.014904 | 0.026759 |


Smaller local incremental influence is not a guarantee of a better closed-loop trajectory; changed poses also change later associations and valid residuals.

### Actual fixed ATE matrix

| Policy | NTU eee_01 RMSE m | TunnelD RMSE m | Shield1 RMSE m |
| --- | --- | --- | --- |
| GEO | 0.118875639 | 113.919295548 | 168.191541666 |
| C0 | 0.114437817 | 131.018887796 | 160.079633869 |
| C60 | 0.114437817 | 134.074228840 | 58.819561395 |
| C100 | 0.114437817 | 144.928082330 | 69.504373464 |
| K100 | 0.117523019 | 134.369935179 | 69.136550038 |


GEO reproduces frozen P0; C0 reproduces frozen P1 on every dataset, both **byte-identically**. NTU C60/C100 also remain byte-identical to P1 and improve 3.733% versus geometry; K100 improves 1.138% versus geometry (2.696% worse than P1), still passing the +5% regression gate.

Every TunnelD candidate is **FAIL**: C60/C100/K100 worsen RMSE by **17.69% / 27.22% / 17.95%** versus geometry and **2.33% / 10.62% / 2.56%** versus P1. The respective maximum errors are **185.80 / 199.25 / 187.17 m**. Their paths are **571.89 / 651.74 / 623.45 m**, versus 176.56 m from the viable official oracle. None meets even BOUNDED_IMPROVEMENT. Repeated ATEs are identical, not averaged.

The photo channel is not accidentally zeroed: all three actual TunnelD candidates have positive information on **98.304%** of frames, mean valid sets **699.09 / 670.22 / 682.01**. Their actual weakest-decile ratio medians are **1.47788 / 2.39337 / 4.47696** (each trajectory's own last-iteration geometry decile); all-active medians **0.80920 / 1.40815 / 2.84110**. The same frozen sigma **4.31126282047** is retained. Thus controlled authority persists in production despite the worse ATE. K100's logged valid count is the full P1 valid set, not the retained subset; its <=100 subset is established by the policy implementation/tests and shadow Nused.

Shield1 improves substantially in this single-run matrix, but remains catastrophically inaccurate:

| Policy | RMSE m | Max error m | Estimated path m | BBox diagonal m | Weak-decile ratio |
| --- | --- | --- | --- | --- | --- |
| GEO | 168.192 | 304.363 | 1144.170 | 430.261 | 0.000000 |
| C0 | 160.080 | 438.318 | 1438.771 | 560.761 | 0.033266 |
| C60 | 58.820 | 103.431 | 795.775 | 306.850 | 0.032942 |
| C100 | 69.504 | 157.724 | 748.733 | 389.697 | 0.034789 |
| K100 | 69.137 | 127.512 | 903.432 | 473.559 | 0.030792 |


C60 cuts Shield1 RMSE by 65.03% versus geometry and bounds its max error below the P1 control, but **58.82 m RMSE / 103.43 m maximum** is not successful localization recovery. Shield policies each have one authorized run, so no repeatability claim is made for their large improvement. Their weak-direction medians remain order 0.03, and early scale changes can alter a severely drifting closed loop; the modest shadow median change does not explain the full trajectory difference by itself.

### Processing time and peak memory

| Dataset | Policy | Repeat | Node wall s | Peak RSS KiB |
| --- | --- | --- | --- | --- |
| eee_01 | GEO | 1 | 15.3758 | 164564 |
| eee_01 | C0 | 1 | 27.4074 | 172376 |
| eee_01 | C60 | 1 | 27.9324 | 172192 |
| eee_01 | C100 | 1 | 28.5698 | 172356 |
| eee_01 | K100 | 1 | 28.2416 | 172176 |
| tunnel_d | GEO | 1 | 10.3765 | 108560 |
| tunnel_d | C0 | 1 | 16.7181 | 128764 |
| tunnel_d | C0 | 2 | 16.6027 | 128548 |
| tunnel_d | C60 | 1 | 17.1580 | 128408 |
| tunnel_d | C60 | 2 | 18.6365 | 127920 |
| tunnel_d | C100 | 1 | 18.6468 | 127608 |
| tunnel_d | C100 | 2 | 17.7990 | 128128 |
| tunnel_d | K100 | 1 | 18.3616 | 129624 |
| tunnel_d | K100 | 2 | 18.3953 | 127896 |
| shield1 | GEO | 1 | 12.6969 | 109572 |
| shield1 | C0 | 1 | 31.2729 | 137436 |
| shield1 | C60 | 1 | 31.4453 | 117536 |
| shield1 | C100 | 1 | 31.9536 | 115800 |
| shield1 | K100 | 1 | 31.4919 | 117388 |


### TunnelD determinism and interpretation

| Policy | Frames each | Timestamps | Trajectory bytes | Max trans delta m | Max quaternion chord | SHA256 prefix |
| --- | --- | --- | --- | --- | --- | --- |
| C0 | 1179 | exact | exact | 0.0 | 0.0 | a3b774e4afd3a100 |
| C60 | 1179 | exact | exact | 0.0 | 0.0 | a0e23a4532e06577 |
| C100 | 1179 | exact | exact | 0.0 | 0.0 | 3842ead576b3efa0 |
| K100 | 1179 | exact | exact | 0.0 | 0.0 | 68cbb4978cc9d9e0 |


All four repeats are byte-identical, hence rotation difference is also zero. Full hashes and all photo-off/C0 paired comparisons are in [determinism.json](../../artifacts/p2a/determinism.json). All 19 authorized runs are in [ablation_summary.csv](../../artifacts/p2a/ablation_summary.csv); detailed evaluator identities, paths, diagnostics and timings are in [metrics.json](../../artifacts/p2a/metrics.json). No divergent-run average or post-ATE rerun occurred.

**P2A NO-GO — AGGREGATE AUTHORITY NOT THE MAIN CAUSE.** The measured dense-Ouster authority imbalance is real, but neither keeping all features with substantially lower information nor selecting the high-response <=100 control rescues TunnelD. Within this bounded policy family, aggregate authority alone is not supported as the primary explanation or a sufficient cure. The evidence motivates reviewing individual residual quality, lifetime/appearance consistency and reconstruction in an external next-round review; it does not identify which one is causal. The strong Shield1 improvement shows authority can matter on another sequence and must be retained in the record, but does not override the specified TunnelD gate.


## Repeatability, regression and tests

The new parallel validity mask is `vector<uint8_t>`; writes are to exclusive
slots. No `vector<bool>` parallel writes are introduced. Reduction writes
exclusive fixed blocks and adds them in a fixed order. K100 uses a total
response/index ordering. The synthetic 1200-row fixture exercises all four
policies for 20 repeats each: **serial/parallel relative A error=0, b error=0,
selected IDs exact**. It checks alpha below/above budget, identical scaling
of A/b, sparse no boost, K100 exact order/count/suppression, PSD/contraction,
invalid NaN/Inf/negative weights, finite diagnostic solves and empty-photo
zero influence.

P1 tests are unchanged and pass: 6000 cubemap projection points,
max finite-difference error `8.89473361632e-9`; 1120 residual-Jacobian points,
max error `3.78524589451e-9`, six faces/six degrees of freedom, boundaries,
IDW visibility/mask and information sign. CTest passes **2/2**. Required actual
photo-off and C0 regressions are separately verified, not inferred from unit
tests. Tests: [jacobian_tests.json](../../artifacts/p2a/jacobian_tests.json),
[policy_tests.json](../../artifacts/p2a/policy_tests.json).

The actual matrix is single-use: NTU → TunnelD → Shield1; C0 → C60 → C100 →
K100 per dataset; one run per NTU/Shield policy, two consecutive runs per
Tunnel policy, plus one geometry regression per dataset. Every timing run
disables the expensive shadow audit. No oracle, build or other benchmark
runs concurrently; normal desktop/browser services remain. Exact commands
and run order: [commands.md](../../artifacts/p2a/commands.md),
[matrix_plan.json](../../artifacts/p2a/matrix_plan.json).

All local policies use the same P1 evaluator/config/GT hashes and the same
output-frame timestamps: NTU 3981 frames/3329 matches, TunnelD 1179/1179,
Shield1 5418/1388. TunnelD retains the single inherited duplicate timestamp
and unsupported-deskew frame caused by the known IMU gap; no frame or photo
measurement is invented across the gap. The official COIN output count is
1185 because its synchronization/initialization path differs; that oracle
is contextual, while all local policies are frame-paired.

Timing is offline-node wall processing (including bag reading, excluding ROS
startup and GT evaluation); RSS is node peak resident KiB. C60/C100/K100 still
compute legacy photo diagnostics plus the deterministic contribution pass,
so this audit implementation has duplicate photo sampling overhead. Runtime
does not represent an optimized policy-only kernel. The COIN oracle's
130.33 s / 1,009,064 KiB includes real-time playback/setup/drain and maximum
child RSS, so it is not a fair estimator-speed comparison.

The optional COIN-inspired complementary selector was not added. This round
does not introduce P2B selection, new reconstruction, feature-lifecycle changes
or GT-driven reruns. Stop for external review after commit/push/remote verification.
