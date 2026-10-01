# P2A reference authority and independent oracle audit

The exact official COIN-LIO build and TunnelD run succeeded without source or
parameter changes. Its ATE under the frozen P1 evaluator is **0.500161363 m**.
The COIN-BIEVR archive independently reproduces **0.585353897 m** with that same
evaluator and has three bitwise-identical post-fix TunnelD trajectories.
These establish viable external photometric reference paths on the actual bag;
neither number selects a P2A parameter.

## Provenance

| Authority | Exact inspected identity | Inspection status |
|---|---|---|
| Production Super-LIO + P1 CUBE implementation | `8db5c84281c35e848f2212fc482b60bf4e2a694b` | Frozen starting point |
| Official `ethz-asl/COIN-LIO` | `76729cc4feb3649cbd79d28f82d9f62a2c82889b` | Existing ignored `refs/COIN-LIO` clean before/after build/run |
| `Scar-c/BIEVR-LIO`, `coin_bievr` | `a518cec38a80b29154b6e70994cf0d3763141434` | Local read-only `/home/lc/algorithm_versa/src/BIEVR-LIO`; all required source/config files match HEAD |
| Author's CUBE slides | Docswell deck `59N8N9`, March 22, 2026 | Slides 10/13/15 text inspected; slide 18 original image visually inspected |

The external BIEVR worktree has unrelated pre-existing changes to
`interfaces/ros1/launch/mapping_avia.launch`, `mapping_mid360.launch`, and
`interfaces/ros1/rviz/config.rviz`. They do not affect this archived audit;
no external file was written. The archive trajectories and CSVs are empirical
artifacts, separately fingerprinted in
[oracle_bievr_archive.json](../../artifacts/p2a/oracle_bievr_archive.json), not
assumed to be fresh runs at current HEAD. Reference identities are in
[reference_shas.json](../../artifacts/p2a/reference_shas.json).

## Official CUBE facts and limits

The [author's slides][slides] disclose cubemap projection and IDW (slide 10),
Gaussian-derivative IGM with all high-response semi-dense features (slide 13),
and direct IGM reprojection with a chained image/projection/pose Jacobian
(slide 15). They do not establish the official weighting/noise model or an
information budget.

Slide 18's [original image][slide18] was inspected directly: TunnelD is the
second sequence row, and the COIN-LIO/CUBE-LIO columns respectively show
**0.485 m / 0.317 m**; sequence length is **180 m**. This association was
verified visually, not inferred from flattened OCR order. These are external
reported performance, not reproductions here or optimization targets.

P2A C60/C100 normalization and K100 are therefore our engineering ablations;
they are not claimed to be official CUBE behavior. The maintained production
identity is Cubemap → IDW → IGM → semi-dense high-response features → direct
IGM residual, not COIN raw-intensity patches or BIEVR voxel-intensity maps.

## Official COIN-LIO source audit

All links below are pinned to the exact requested commit.

| Requested fact | Independently inspected source evidence |
|---|---|
| Official ENWIDE Ouster support | [mapping_enwide.launch][coin_launch] loads official `params.yaml`, `line_removal.yaml`, and `os_enwide.json`; uses `/ouster/points`, `/ouster/imu`, and `image/u_shift=0`. [params.yaml][coin_params] sets Ouster type 3, 128 lines, timestamp unit 3. [projector.cpp][coin_projector] loads beam/pixel metadata and builds pixel/index lookup. |
| `num_features=60`, `patch_size=5`, `max_lifetime=25`, `photo_scale=0.00095` | [params.yaml lines 39–49][coin_params] explicitly sets these values. C++ fallback values differ for some fields; the official launch loads YAML. |
| Weak geometry directions | [laserMapping.cpp lines 329–354 and 1198–1210][coin_mapping] takes eigenvectors of `H_geo_translation.T * H_geo_translation`, counts normal-aligned contributions, and marks directions with contribution below `n_uninformative`. This is not simply an eigenvalue-threshold test. |
| Complementary ranking | [feature_manager.cpp lines 118–268][coin_features] initially proposes candidates by image response with spatial suppression, then evaluates the image patch's dominant direction and projection Jacobian against each weak direction; score is `abs(dI_du * normalized(du_dp * v))`, followed by descending ranking and per-direction selection. |
| Scale before common IEKF fusion | [laserMapping.cpp lines 808–840][coin_mapping] multiplies both photo Jacobian and residual by `photo_scale`, concatenates them with geometry terms, and passes the common observation to the existing iterated EKF. This gives lambda-squared Hessian scaling in its own residual domain. |
| Intensity processing | [image_processing.cpp lines 34–116][coin_images] includes line removal, brightness filtering, Gaussian smoothing, truncation and finite-difference intensity gradients; [line_removal.yaml][coin_lines] supplies official high/low-pass kernels. |

Important budget distinction: **60 tracked patches are not 60 scalar residuals**.
With 5×5 patches the pre-validity allocation is up to 1500 scalar photo terms
([laserMapping.cpp lines 677–702][coin_mapping]). C60 borrows the track budget as
an engineering reference; it does not reproduce COIN's measurement count or
covariance. Similarly, literal `0.00095` cannot be transplanted to an IGM
residual model.

## A1: unmodified official COIN executable oracle

An isolated catkin workspace under `/tmp/cube_p2a_coin_oracle` symlinks the
clean upstream checkout. No source edits, compatibility patches, or tuning
were made. Build: ROS Noetic, Release, C++14 as upstream, `-j2`, system
`/usr/bin/python3`; mapping and calibration executables linked successfully.
Upstream warnings include the existing DIM_STATE redefinition and PCL type
deprecations; neither required a patch.

The first runtime attempt inside the sandbox could not bind ROS local
sockets. The authorized outside-sandbox retry succeeded, so there is no
remaining environmental oracle blocker. It used an isolated ROS master,
the official `mapping_enwide.launch` with `rviz:=false`, and the exact full bag:
`/home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag` at 1×.

The external recorder subscribes to `/Odometry` and saves timestamp and
pose at 17 significant digits. Upstream [laserMapping.cpp lines 484–506][coin_mapping]
publishes `state_point.pos/rot` as child `body` (IMU), so the frozen evaluator's
IMU-to-prism offset applies directly. The evaluator was not modified:
`python3 eval/evaluate.py tunnel_d runtime/p2a_coin_oracle`, using nearest
timestamp association ≤0.1 s, zero offset, the P1 prism-in-IMU lever
`[-0.006253, 0.011775, 0.10825]`, and SE3 alignment without scale.

| Metric | Exact observed value |
|---|---:|
| ATE RMSE | 0.5001613629532796 m |
| Mean / median / max error | 0.459517999 / 0.489948602 / 0.736667339 m |
| Output frames / matched frames | 1185 / 1184 |
| Estimated IMU path length | 176.556141547 m |
| Output timestamp duration | 118.614315748 s |
| Driver wall time, before final process shutdown | 129.060299487 s |
| GNU time total wall time | 130.33 s |
| GNU time maximum child RSS | 1,009,064 KiB |

These timing values include master/setup, real-time bag playback, drain and
shutdown; they are not estimator-only processing time. GNU time RSS is the
maximum child-process RSS, not summed ROS memory. A single mandatory oracle run
was made; it is not a COIN repeated-run determinism study. All oracle processes
were reaped before local benchmark timing was authorized to begin.

[oracle_coin_official.json](../../artifacts/p2a/oracle_coin_official.json)
records binary/config/trajectory/evaluator/GT hashes and metrics.
[oracle_commands.md](../../artifacts/p2a/oracle_commands.md) records the command
and recorder protocol. Logs and full trajectory stay ignored.

## A2: independent COIN-BIEVR archived audit

[intensity_sampling.cpp][bievr_sampling] constructs observed intensity voxels,
computes a weak direction from observed voxel normal information, ranks
directional intensity contribution, selects top voxels, and downsamples their
intensity points. [ENWIDE preset][bievr_params] specifies top **100 voxels**, point
downsampling **0.1 m**, and conservative working `photometric_scale=0.001`.
Round 11 `config_used.yaml` independently agrees. A voxel budget permits many
scalar residuals; it is not a 100-point/100-residual selector.

[ls_optimizer.cpp lines 363–435][bievr_optimizer] shares
`evaluatePhotometricTerm()` between serial diagnostics and production, scales
residual/Jacobian by lambda, and performs deterministic parallel reduction.
Huber weights depend on scaled residuals, so blanket division by lambda²
does not undo robustification (lines 673–688). This differs from P2A scaling
already robustified `Ap` and `bp` together and supports not importing lambda.

The [handoff][bievr_handoff] contains historical, superseded stages. Round 10.7
[summary][bievr_r107] has pre-race-fix multi-thread results
1.2704 / 0.5891 / 168.4711 m, while single-thread gives 0.5854 m; these are
not the authoritative repeatability result. Its text also contains the old
N+1/5× stride-description error; the corrected definition is indices
0,4,8,…, one in four points, as documented in Round 11.

Direct inspection of Round 11 `results/round11_vectorbool_fix/tunneld/run{1,2,3}`
confirms all three trajectories have 1185 frames, path **176.240920839 m**, and
the same complete SHA256:
`a8ea08e009f813c03cad94ad43d5297b24b1ce8bfd0703efbe925ddac78caebf`.
Their archived evaluation text is identical: RMSE **0.585354 m**, max
**0.882452 m**. Re-evaluating one archived trajectory with the current frozen
P1 `eval/evaluate.py` gives **0.5853538973824002 m**, 1184 matches. Thus this
archived number and the official COIN oracle share the same evaluator/GT
convention; no BIEVR estimator was rerun.

The Round 16 `results/round16_r3_parity/tunneld_c1/trajectory.tum` and `photo.csv`
are both exactly byte-identical to Round 11 run1, not merely close in ATE.
Both CSVs contain 1185 valid-row directional observations. Directly computing
`q_photo_v1/q_geo_v1` for finite valid-match rows with `q_geo_v1 > 1e-12` gives:

| Statistic | Observed ratio |
|---|---:|
| P10 | 0.544559306 |
| P50 | **3.010557530** |
| P90 | 6.698627218 |
| P99 | 9.737850677 |

The handoff's historical `~1.7` is not the all-frame median of this successful
post-fix archive. The supported conclusion is an order-one median (~3), not
an exact ratio of 1.7. Moreover these BIEVR directions arise from observed
voxel normal information and are mapped into the local pose block
([ls_optimizer.cpp lines 710–738][bievr_optimizer]); the P1/P2A weakest
translation-Hessian direction is a different diagnostic convention. This is
context for authority, not a numerical equivalence or full marginalized
observability proof.

The [Round 11 determinism lesson][bievr_lesson] explains the introduced
`std::vector<bool>` parallel write race: disjoint logical indices shared packed
storage words. Byte-addressable masks removed the race, yielding three
bitwise-identical runs and serial parity. Current HEAD has since extracted
the [shared point-voxel association primitive][bievr_assoc], which writes
exclusive POD entries and sorts with `(hash, point_idx)` total order; the old
mask is no longer in current `intensity_sampling.cpp`. The defect was in the
reproduction's added path, not original BIEVR or the COIN-BIEVR mathematics.

FlatSurfaces is supported by more than a handoff assertion: archived Round12
B0/C0 RMSE **1.523154 / 1.654686 m**, C1_A/C1_B both **0.059469 m**, with
identical trajectory SHA256
`6920bc2bb7c198500eb5be96852c2dcc344547e97c81679e5c14009caa8e4e22`.
Round16 FlatSurfaces C1 also matches that trajectory exactly. The
[handoff Round 9][bievr_handoff] documents the earlier production/diagnostic
Jacobian mismatch (duplicate inverse-pixel-size factor), its shared-helper
repair, and invalidation of the earlier misleading results. Together with
the current shared production helper, this establishes the lineage's
photometric rescue after parity and determinism repairs; it does not validate
P2A residual quality automatically.

## Implications for the bounded P2A experiment

The two successful references use bounded selection but different residual
domains and sensor/map pipelines. They justify **testing** aggregate
information control, not presuming it will cure the P1 trajectory. Preserve
the P1 robust residual/Jacobian semantics and scale A and b together after
robustification; check diagnostics against actual production and gate every
TunnelD ATE on repeatability. Any remaining P2A failure is to be reported,
not repaired by transplanting code, tuning against oracle ATE, or switching
datasets/evaluation conventions.

[slides]: https://www.docswell.com/s/scomup/59N8N9-2026-03-22-173102
[slide18]: https://bcdn.docswell.com/page/PEXQP555JX.jpg
[coin_launch]: https://github.com/ethz-asl/COIN-LIO/blob/76729cc4feb3649cbd79d28f82d9f62a2c82889b/launch/mapping_enwide.launch
[coin_params]: https://github.com/ethz-asl/COIN-LIO/blob/76729cc4feb3649cbd79d28f82d9f62a2c82889b/config/params.yaml#L39-L49
[coin_projector]: https://github.com/ethz-asl/COIN-LIO/blob/76729cc4feb3649cbd79d28f82d9f62a2c82889b/src/projector.cpp#L7-L95
[coin_mapping]: https://github.com/ethz-asl/COIN-LIO/blob/76729cc4feb3649cbd79d28f82d9f62a2c82889b/src/laserMapping.cpp
[coin_features]: https://github.com/ethz-asl/COIN-LIO/blob/76729cc4feb3649cbd79d28f82d9f62a2c82889b/src/feature_manager.cpp#L118-L268
[coin_images]: https://github.com/ethz-asl/COIN-LIO/blob/76729cc4feb3649cbd79d28f82d9f62a2c82889b/src/image_processing.cpp#L34-L116
[coin_lines]: https://github.com/ethz-asl/COIN-LIO/blob/76729cc4feb3649cbd79d28f82d9f62a2c82889b/config/line_removal.yaml
[bievr_sampling]: https://github.com/Scar-c/BIEVR-LIO/blob/a518cec38a80b29154b6e70994cf0d3763141434/BIEVR/src/intensity_sampling.cpp#L29-L225
[bievr_optimizer]: https://github.com/Scar-c/BIEVR-LIO/blob/a518cec38a80b29154b6e70994cf0d3763141434/BIEVR/src/ls_optimizer.cpp
[bievr_params]: https://github.com/Scar-c/BIEVR-LIO/blob/a518cec38a80b29154b6e70994cf0d3763141434/config/params_coin_bievr_ouster_enwide.yaml#L60-L75
[bievr_handoff]: https://github.com/Scar-c/BIEVR-LIO/blob/a518cec38a80b29154b6e70994cf0d3763141434/COIN_BIEVR_HANDOFF.md
[bievr_lesson]: https://github.com/Scar-c/BIEVR-LIO/blob/a518cec38a80b29154b6e70994cf0d3763141434/docs/ROUND11_PARALLEL_DETERMINISM_LESSONS.md
[bievr_assoc]: https://github.com/Scar-c/BIEVR-LIO/blob/a518cec38a80b29154b6e70994cf0d3763141434/BIEVR/include/bievr_lio/point_voxel_association.h#L14-L60
[bievr_r107]: /home/lc/algorithm_versa/src/BIEVR-LIO/results/round10_7_pointfilter4/summary.md
