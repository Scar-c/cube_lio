# CUBE-LIO P2A Prompt — Reference-Guided Photometric Information Consistency Audit

## 0. Role and hard stop

You are working as the implementation/benchmark CLI agent for:

- workspace: `/home/lc/cube_lio`
- repository: `https://github.com/Scar-c/cube_lio.git`
- authoritative starting branch: `p1-cube-photo-v0`
- authoritative starting SHA: `8db5c84281c35e848f2212fc482b60bf4e2a694b`

Create a new branch:

`p2a-photo-information-consistency`

Do **not** modify `p0-super-lio-baseline` or `p1-cube-photo-v0`.

This round is intentionally narrow. The purpose is to determine why the P1
semi-dense IGM observation becomes over-authoritative on dense Ouster TunnelD
and to validate one small family of reference-derived information-control
policies.

Do **not** start a new mapping backend, do not replace Super-LIO, do not add
learning, do not add vision, do not add BIEVR maps to this repository, and do
not perform broad GT-driven parameter tuning.

At the end:
1. produce the required report/artifacts,
2. commit,
3. push the branch,
4. verify remote SHA,
5. ensure the worktree is clean,
6. STOP for external review.

---

# 1. Reference authority and provenance

Use the following references explicitly and record their exact inspected SHAs.

## 1.1 BASE — Super-LIO inside this repository

The actual production estimator remains the current Super-LIO-based
implementation already frozen by P0/P1.

Do not change:
- ESKF equations/state reset,
- IMU propagation,
- geometry residual formulation,
- OctVox/HKNN map/search,
- geometry weights,
- geometry configs,
- GT evaluators,
- P0/P1 dataset/extrinsic conventions.

Photo-off must remain byte-identical to P1/P0.

## 1.2 CUBE-LIO algorithm authority — public slides

Public slides:
`https://www.docswell.com/s/scomup/59N8N9-2026-03-22-173102`

Relevant slides:
- slide 10: Cubemap projection + IDW for sparse/irregular LiDAR
- slide 13: Gaussian-derivative IGM and **semi-dense high-response features**
- slide 15: direct IGM reprojection residual and chained Jacobian
- slide 18: ENWIDE benchmark

Important:
- Preserve the P1 CUBE identity: current-frame Cubemap -> IDW -> IGM ->
  semi-dense high-response features -> direct IGM residual.
- Do **not** replace the main method with COIN raw-intensity patches.
- Do **not** claim that the official CUBE implementation uses any information
  scaling proposed in this round. The public slides do not disclose enough
  weighting/noise details to establish that.

The public slide table reports approximately:
- TunnelD COIN-LIO ATE: 0.485 m
- TunnelD CUBE-LIO ATE: 0.317 m

Treat these only as external reference performance, not as a target to tune
against.

## 1.3 REF A — official COIN-LIO

Repository:
`https://github.com/ethz-asl/COIN-LIO`
Inspect exact main SHA:
`76729cc4feb3649cbd79d28f82d9f62a2c82889b`

Required files:
- `config/params.yaml`
- `config/line_removal.yaml`
- `launch/mapping_enwide.launch`
- `src/feature_manager.cpp`
- `src/image_processing.cpp`
- `src/projector.cpp`
- `src/laserMapping.cpp`

Relevant facts to verify from source, not assume:
- ENWIDE Ouster support is official.
- `num_features: 60`
- `patch_size: 5`
- `max_lifetime: 25`
- `photo_scale: 0.00095`
- geometry weak directions are detected from the geometric Jacobian.
- candidate image patches are ranked by their complementary information along
  weak geometry directions.
- photometric terms are scaled before being fused into the same iterated EKF.

Do not copy COIN-LIO source code. It is a design/reference oracle.

## 1.4 REF B — user's COIN-BIEVR reproduction

Repository:
`https://github.com/Scar-c/BIEVR-LIO`
Branch:
`coin_bievr`
Current reference SHA:
`a518cec38a80b29154b6e70994cf0d3763141434`

Required files:
- `COIN_BIEVR_HANDOFF.md`
- `BIEVR/src/intensity_sampling.cpp`
- `BIEVR/src/ls_optimizer.cpp`
- `config/params_coin_bievr_ouster_enwide.yaml`
- `results/round10_7_pointfilter4/summary.md` if present
- Round 11+ TunnelD determinism evidence
- `results/round16_r3_parity/tunneld_c1/*`

Important already-recorded reference observations that must be independently
verified in the repository:
- weak-geometry-guided sampling uses selected intensity voxels;
- default/frozen design used a top-100 intensity-voxel budget and 0.1 m point
  downsampling in the reproduction lineage;
- photometric scale around `lambda=0.001` became the conservative working scale;
- after the `std::vector<bool>` parallel race was fixed, TunnelD repeated runs
  became bitwise deterministic with APE RMSE about `0.5854 m`;
- the successful TunnelD mechanism showed photo information exceeding geometry
  along the one weak tunnel direction at roughly order-one ratio, not the
  P1 cube_lio ratio of ~14.6x;
- FlatSurfaces photometric rescue was also verified after production/diagnostic
  Jacobian parity was fixed.

These are reference evidence, not code to transplant wholesale.

---

# 2. Frozen P1 facts that define the problem

P1 report:
`spec/p1/P1_CUBE_PHOTOMETRIC_V0_REPORT.md`

Frozen paired results:

| Dataset | Geometry ATE m | P1 photo ATE m |
|---|---:|---:|
| NTU eee_01 | 0.118875639 | 0.114437817 |
| GEODE Shield1 | 168.191541666 | 160.079633869 |
| ENWIDE TunnelD | 113.919295548 | 131.018887796 |

P1 local Jacobian tests passed and photo-off is byte-identical to P0.

The most important P1 diagnostic is the huge cross-sensor variation in photo
authority:

| Dataset | mean valid photo | median Ap/Ag in weakest geometry translation |
|---|---:|---:|
| eee_01 | ~9 | ~0.00248 |
| Shield1 | ~32 | ~0.03327 |
| TunnelD | ~701 | ~14.60 |

This round must treat the following as the main hypothesis:

> CUBE semi-dense features are not inherently wrong; the P1 implementation
> likely lets aggregate photo information grow strongly with the number and
> correlation of valid residuals. Dense Ouster therefore receives much more
> effective photo authority than sparse sensors.

Do not assume this hypothesis is true. Test it.

---

# 3. Dataset paths — frozen

Use exactly the P1 datasets/evaluators unless a file is truly absent.

NTU:
`/home/lc/super_livo/bag/NTU/eee_01/eee_01.bag`

GEODE Shield1:
`/home/lc/algorithm_versa/bag/GEODE/Shield_tunnel1_gamma.bag`

ENWIDE TunnelD:
`/home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag`

Do not substitute Shield4/5, TunnelS, Runway, or any other sequence in P2A.

---

# 4. Phase A — establish external TunnelD reference sanity

Before changing production fusion semantics, inspect the two known successful
reference paths.

## A1. Official COIN-LIO executable oracle

Attempt to build/run the official COIN-LIO SHA above on the exact same TunnelD
bag using its official ENWIDE configuration (`mapping_enwide.launch`,
`params.yaml`, `line_removal.yaml`, Ouster metadata).

Requirements:
- keep the upstream repository unmodified;
- use a disposable ignored checkout/worktree or `/tmp`;
- no parameter tuning;
- evaluate with the same TunnelD GT/evaluator convention frozen by P1;
- record ATE, frame count, path length, runtime if successful.

If host/environment incompatibility prevents an exact run:
- document the exact blocker,
- do not patch COIN-LIO to make the number look good,
- continue P2A using source inspection + public slide/reference evidence.

This is a sanity oracle only. Its ATE must not be used to select P2A parameters.

## A2. COIN-BIEVR archived oracle

Do not rerun it unless needed.
Audit the existing `coin_bievr` artifacts and record:
- exact branch SHA,
- deterministic post-race-fix TunnelD ATE evidence,
- feature/voxel budget,
- lambda,
- weak-direction q_photo/q_geo evidence,
- determinism lesson (`std::vector<bool>` packed-bit race).

This is especially important because fully degenerate TunnelD amplifies tiny
implementation nondeterminism.

---

# 5. Phase B — P1 shadow information audit (NO estimator change)

Add diagnostics that can be enabled in P1 without changing `A` or `b`.

For each frame and each iterated observation, retain the current P1 robust
residual/Jacobian semantics and compute:

## B1. Current production quantities
- `N_active`
- `N_valid`
- per-face valid counts
- residual median / MAD / RMS
- `trace(Ag)`, `trace(Ap)`
- eigenvalues of geometry pose 6x6 and translation 3x3 blocks
- eigenvalues of photo pose 6x6 and translation 3x3 blocks

## B2. Directional information
For each geometry translation eigenvector `v_k`:
- `qg_k = v_k^T Ag_tt v_k`
- `qp_k = v_k^T Ap_tt v_k`
- ratio `qp_k / max(qg_k, numerical_epsilon)`
- `sg_k = v_k^T bg_t`
- `sp_k = v_k^T bp_t`

Do not call this a full marginalized observability proof. It is a local
directional diagnostic.

## B3. Predicted photo influence
At the same linearization point, compute diagnostic-only:
- geometry-only local solve increment `d_geo`
- joint local solve increment `d_joint`
- `d_photo = d_joint - d_geo`
- translation norm and rotation angle of `d_photo`

Use the same regularization/information convention as the production ESKF local
pose block as closely as possible. Clearly document any approximation.

## B4. Correlation/support proxy
Because semi-dense neighboring IGM residuals are not independent, add a cheap
support-concentration diagnostic without changing feature selection:

For valid residuals:
- count occupied Cubemap faces;
- count occupied coarse cells using a fixed diagnostic grid per face
  (e.g. 8x8; diagnostic only);
- compute number of valid residuals per occupied coarse cell;
- compute fraction of total `trace(J^T W J)` contributed by the top 10% most
  informative occupied cells;
- report `N_valid / N_occupied_cells`.

This is a proxy for redundancy/concentration, not an exact statistical
effective sample size.

Run shadow diagnostics on all three datasets with the P1 production trajectory.
The trajectories must remain byte-identical to P1.

---

# 6. Phase C — reference-guided information-budget policies

Do **not** alter Cubemap, IDW, IGM, P1 residual, P1 Jacobian, feature lifetime,
robust gate, or geometry in this round.

We want to separate:
1. too much aggregate authority,
2. too many correlated residuals,
3. wrong individual residuals.

Implement the following policies as explicit ablations.

## C0 — P1 RAW SUM (control)

Exact P1:
`Ap = sum(w_i J_i^T J_i)`
`bp = -sum(w_i J_i^T r_i)`

Must reproduce P1.

## C60 — semi-dense + COIN-budget information normalization

Keep **all** P1 valid semi-dense residuals.

Let:
`alpha60 = min(1.0, 60.0 / N_valid)`

After the existing P1 robust accumulation:
`Ap' = alpha60 * Ap`
`bp' = alpha60 * bp`

Rationale:
- official COIN-LIO maintains about 60 tracked image features;
- this does NOT claim CUBE uses 60 features;
- unlike hard subsampling, it keeps CUBE's semi-dense spatial coverage while
  preventing a 128-line LiDAR from gaining arbitrarily larger total authority.

Do not boost sparse datasets: when `N_valid <= 60`, alpha=1.

## C100 — semi-dense + COIN-BIEVR-budget information normalization

Keep all valid semi-dense residuals.

`alpha100 = min(1.0, 100.0 / N_valid)`

Then:
`Ap' = alpha100 * Ap`
`bp' = alpha100 * bp`

Rationale:
- the COIN-BIEVR reproduction uses a top-100 weak-direction intensity-voxel
  budget;
- again, this is a reference-derived engineering ablation, not claimed official
  CUBE behavior.

Do not boost sparse datasets.

## K100 — fixed-100 high-response control

As a diagnostic ablation only:
- use the existing CUBE IGM response ordering;
- among currently valid P1 photo residuals, keep at most the 100 highest-response
  spatially suppressed features;
- use the **same original P1 weight/noise model** on those retained residuals;
- no extra alpha normalization.

Purpose:
- distinguish "many correlated residuals" from "aggregate scale only".
- This is not the main CUBE method and must be reported as a sparse control.

Do not implement COIN raw-intensity patches or BIEVR voxel-intensity maps.

---

# 7. Why alpha scales A and b together

Document this explicitly.

P1 already performs residual-domain robust gating/Huber/MAD normalization.
P2A's `alpha` is an **information-budget scale after robustification**:

`A_photo <- alpha A_photo`
`b_photo <- alpha b_photo`

This is equivalent to changing the aggregate photo measurement covariance /
information authority while preserving:
- which residuals are considered valid,
- their robust inlier/outlier classification,
- their individual normalized residual values.

Do not silently multiply only A or only b.

Do not reuse COIN's literal `0.00095` or COIN-BIEVR's literal `0.001` as a
CUBE IGM residual scale because the residual domains are different.

---

# 8. Phase D — safety gates before actual estimator runs

For C60/C100/K100, first compute them in shadow mode on the frozen P1
trajectory. No estimator change yet.

Report for each dataset/policy:
- N_valid distribution
- alpha distribution
- weak-direction `qp/qg`
- global `trace(Ap)/trace(Ag)`
- predicted photo increment P50/P90/P99/max
- support-concentration proxy

## Mandatory qualitative gate

A candidate may enter actual estimator runs only if:
1. it does not increase photo authority relative to C0 on any frame;
2. it leaves NTU sparse behavior unchanged or weaker;
3. it substantially reduces TunnelD's P1 weak-direction authority from the
   current order-of-magnitude `~14.6 median`;
4. no NaN/Inf/negative-information failures occur;
5. serial-vs-parallel accumulation parity passes.

Do not use GT ATE to decide whether a candidate passes this shadow gate.

---

# 9. Determinism gate learned from COIN-BIEVR

Before trusting any TunnelD ATE:

- inspect all new parallel masks/containers;
- **do not use `std::vector<bool>` for parallel writes**;
- add a deterministic synthetic fixture comparing serial vs default-parallel
  `Ap`, `bp`, valid count and selected feature IDs for C0/C60/C100/K100;
- require relative A/b differences near machine precision for deterministic
  reduction semantics, or document the exact expected floating reduction
  tolerance;
- run each final TunnelD candidate **twice sequentially** on an otherwise idle
  machine;
- compare output frame count, timestamps, trajectory hashes and max pose
  difference.

If repeated TunnelD runs bifurcate materially, classify as
`NONDETERMINISM_BLOCKER` and stop. Do not average divergent runs.

---

# 10. Actual A/B evaluation matrix

Only candidates that pass Phase D may enter production.

Run, in fixed order:

1. NTU eee_01
2. TunnelD
3. Shield1

For each authorized candidate:
- exactly one run on NTU and Shield1;
- exactly two sequential runs on TunnelD for determinism.

Always include:
- geometry-only frozen P0 number,
- P1 C0 control reproduction,
- C60,
- C100,
- K100 if it passed shadow gate.

No GT-driven reruns or parameter changes after seeing ATE.

---

# 11. Evaluation interpretation / gates

The purpose is mechanism validation, not paper-number chasing.

## NTU regression safety
Compared with P0 geometry:
- PASS: photo candidate ATE <= +5%
- strong preference: retain P1's no-regression/improvement behavior

## TunnelD
Reference context:
- geometry-only Super-LIO P0: ~113.9 m
- P1 CUBE V0: ~131.0 m
- public slide COIN-LIO: ~0.485 m
- public slide CUBE-LIO: ~0.317 m
- user's deterministic COIN-BIEVR reproduction: ~0.5854 m

Classify:
- `REFERENCE_LEVEL`: <= 1.0 m
- `STRONG_RESCUE`: >1.0 m and <=2.0 m
- `CLEAR_RESCUE`: >2.0 m and <=10.0 m
- `BOUNDED_IMPROVEMENT`: >10 m but materially better than P0/P1
- `FAIL`: >= P0 geometry or unstable/divergent

These are reporting classes, not tuning targets.

## Shield1
Do not demand paper-level ATE from this round because the frozen Super-LIO
geometry baseline is already ~168 m.

Report:
- whether photo improves or worsens RMSE,
- max error,
- trajectory boundedness,
- weak-direction information.

Do not call a small RMSE decrease a successful localization recovery if the
trajectory is still catastrophically wrong.

---

# 12. Optional official COIN weak-direction diagnostic (shadow only)

If time permits after all mandatory work:

Using the current P1 Cubemap/IGM candidate features, compute a COIN-inspired
complementary score in **shadow diagnostics only**:

`score_i(vweak) = |grad_M_i * J_cube_i * vweak|`

where `vweak` comes from the weakest geometry translation direction.

Report:
- overlap between top complementary candidates and top raw-IGM candidates,
- their directional information distributions.

Do NOT use this selector in the estimator in P2A.

This determines whether P2B should pursue:
- CUBE semi-dense only, or
- a CUBE + COIN complementary selection hybrid.

---

# 13. Required tests

Keep all P1 tests passing.

Add:
1. information-budget scaling test:
   - alpha=1 for N<=budget
   - alpha=budget/N for N>budget
   - A and b scaled identically
2. no-boost sparse test
3. C60/C100 deterministic parallel accumulation test
4. K100 exact-count/order determinism test
5. serial-vs-parallel A/b parity
6. photo-off byte-parity regression
7. P1 C0 reproduction regression
8. NaN/Inf/PSD sanity for photo A

Do not remove or weaken P1 finite-difference Jacobian tests.

---

# 14. Artifacts

Create:

`spec/p2a/P2A_PHOTO_INFORMATION_CONSISTENCY_REPORT.md`

`spec/p2a/REFERENCE_AUDIT.md`

`artifacts/p2a/reference_shas.json`

`artifacts/p2a/shadow_summary.csv`

`artifacts/p2a/ablation_summary.csv`

`artifacts/p2a/determinism.json`

`artifacts/p2a/commands.md`

`artifacts/p2a/build_identity.json`

The report must contain:
- exact reference SHAs,
- exact equations implemented,
- explicit separation between CUBE official facts and our engineering ablations,
- official COIN-LIO oracle outcome/blocker,
- COIN-BIEVR archived TunnelD evidence,
- P1 reproduction,
- shadow information ratios,
- support concentration,
- ATE matrix,
- timing/RSS,
- determinism result,
- verdict.

---

# 15. Verdict logic

Use one of:

## `P2A PASS — PHOTO AUTHORITY ROOT CAUSE SUPPORTED`
Use only if at least one reference-budget policy:
- passes NTU regression,
- is deterministic,
- materially improves TunnelD relative to P0/P1,
- and diagnostics show the improvement corresponds to controlled photo
  information rather than accidental zeroing of the photo channel.

## `P2A PARTIAL — AUTHORITY CONTROL HELPS BUT RECOVERY INCOMPLETE`
Use if information ratios become sane and TunnelD improves materially but stays
well above 10 m or remains fragile.

## `P2A NO-GO — AGGREGATE AUTHORITY NOT THE MAIN CAUSE`
Use if C60/C100/K100 control the Hessian but TunnelD does not improve, implying
the main remaining issue is likely residual quality / feature lifecycle /
appearance consistency / Cubemap reconstruction rather than aggregate scale.

## `P2A BLOCKED — NONDETERMINISM OR REFERENCE INVALID`
Use if repeatability fails or a foundational reference/evaluator problem is
found.

Do not hide a negative result.

---

# 16. Git discipline

Start from:
`8db5c84281c35e848f2212fc482b60bf4e2a694b`

Branch:
`p2a-photo-information-consistency`

Keep runtime bags/large logs/trajectories ignored unless a compact artifact is
explicitly required.

At closure:
- commit report + source + compact artifacts,
- push branch,
- verify remote SHA,
- `git status` clean,
- print final short summary with:
  - verdict,
  - branch,
  - final SHA,
  - key ATEs,
  - TunnelD determinism,
  - which hypothesis is supported,
  - STOP.

Do not start P2B automatically.
