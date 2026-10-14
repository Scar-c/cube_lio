# P4 faithful CUBE reproduction report

**BLOCKED — PAPER/IMPLEMENTATION SEMANTICS NOT YET RECONCILED**

The workspace, historical baseline and two full TunnelD controls are recovered.
A separate CUBE mathematical module implements the published projection, inverse,
range/intensity raster, nonrecursive IDW, first-order Gaussian derivative IGM,
seam operator, Eq. (16) image gradient and Eq. (10) residual/state chain.
The **actual residual Jacobian gate fails** under the explicitly chosen bilinear
sampling implementation. Following prompt §23(3), this phase stops before
persistent feature integration, CUBE accuracy replay, ablations or Livox work.
This is not a completed photometric frontend or evidence against the published
CUBE-LIO system's reported performance.

## Source and implementation boundary

Complete local paper: `MI -- 2026 - ICRA - Cubemap-Based LiDAR-Inertial Odometry
with Intensity Assistance.pdf`, eight pages, pp. 8446–8453,
DOI 10.1109/ICRA57385.2026.11697017.
SHA-256: `6bfb0996747ebba3625634cc01ce3c8cc2f09b9b79c94a5c7964f90c70c8289c`.

Repository `https://github.com/Scar-c/cube_lio`, recovered baseline
`f3060768abee4d2ee2343b07edf62346c7cd0f87`, branch
`p4-faithful-cube-paper-reproduction`. P3A/P3R reports were read through remote
Git blobs; neither was used as the base. Recovery logs and clean pre-edit HEAD
are in `runtime/p4_review`. Final commit identities are in the generated review
bundle; a report cannot contain its own commit SHA without a self-reference.

`paper_cube.hpp/.cpp` is isolated from production `lio` by an explicit CMake
exclusion. Only `test_paper_cube` links it. It uses body-frame points, paper face
order +X,-Y,-X,+Y,+Z,-Z, all six exact Eq. (5) bases, r/2 projection scale and
Euclidean range. It has no COIN observation, feature-manager, NCC, weak-direction
selection, information budget, adaptive weighting or thesis-specific correction.
Existing geometry, propagation, estimator, parser and COIN source files are
byte-identical to f306.

The offline runner gained explicit `--bag` and `--gt` paths, GT identity and
library hash. The evaluator gained only a GT path override: lever arm, time
association and SE3 alignment calculations are unchanged. Recovered COIN config
is pinned at `76729cc4feb3649cbd79d28f82d9f62a2c82889b`; all three config hashes
match archived historical identities.

## Mathematical gate and attribution

Tests were compiled with Release C++17, GCC/catkin, Eigen/OpenCV/TBB. All values
below come from executable output; full per-component data is in
`artifacts/p4/residual_jacobian.csv` (3,456 rows: 144 points × 6 state components
× 4 difference steps).

| Check | Result |
|---|---:|
| Projection FD, 6,000 points/all six faces, h=1e-6 | max absolute error 6.7243e-9 |
| Projection/inverse/range round trip | max 8.1403e-15 m |
| Inverse pose transform Jacobian, right rotation/world position | max 3.1634e-10 |
| Seam remapping direction, 144 edge/corner cases | max 1.6653e-16 |
| Supported IGM central stencils crossing boundaries | 24/24 valid |
| Gaussian derivative linear-ramp response | max error 3.9524e-14 |
| IDW plane/hole/layers/boundary/no-support/nonrecursive checks | PASS |
| Eq. (16) H versus actual residual FD, h=1e-6 | **860/864 components fail** |
| Residual H maximum absolute error | 20.898944 |
| Residual H relative error P50/P95 | 0.058353 / 2.023977 |
| Residual H sign disagreements | 37/864 |
| Same transform/projection chain with local image derivative (diagnostic only) | max error 1.4680e-8 |

Residual FD tolerance was declared before results: absolute 1e-4 plus relative
1e-4 times |FD|. **This tolerance is a local acceptance choice, not a number
specified by the paper or by the prompt.** The paper specifies Eq. (16), but
does not specify the continuous subpixel interpolant or approximation tolerance.
The requested actual residual FD gate and faithful Eq. (16) have not been
reconciled under this discretization. No tolerance was enlarged after failure.

Separating seams from interiors rules out a seam-only explanation. At h=1e-6,
716/720 interior components fail (relative P95 0.622939) and 144/144 boundary
stencil components fail (relative P95 98.051756; near-zero FD denominators can
inflate relative errors). Maximum errors are 15.933191 and 20.898944,
respectively. Results converge at h=1e-4,1e-5,1e-6,1e-7. Repeated complete
FD CSVs are byte-identical, SHA-256
`29a7682052fdd4c24c0a84ceb13e5efac840e4b79a85b72118be6a3148d29c9e`.

The minimal counterexample removes geometry, seams, IDW, missing masks, motion,
noise and GT. For positive cubic intensity I(j)=(j+16)^3, Gaussian derivative
response is M(j)=3(j+16)^2+C. At x=j+alpha, bilinear M has local derivative
6j+99, whereas Eq. (16) produces 6j+96+6alpha. At j=20, alpha=0.37:

* Eq. (16): 218.22;
* actual bilinear residual derivative: 219.00;
* persistent difference: -0.78, also observed under all four FD steps.

`python3 tools/p4/minimal_gradient_gate.py` reproduces this and exits 2.
`test_paper_cube --jacobian` drives the actual frame/residual/state chain and
exits 2. The diagnostic local-image derivative restores agreement without
changing the projection or transform, eliminating column/sign and numerical-step
explanations for the measured disagreement. It is **not** substituted into the
published implementation. The finding is a stencil/interpolant mismatch, not a
claim that the authors' method cannot work with an approximate derivative.

Another boundary convention remains explicit: Eq. (4) has overlapping X/Y tie
conditions, and literal Eq. (15) can return coordinate r on an exact positive
boundary. Fourteen of eighteen tested exact positive edge/corner remaps are
outside the stated half-open [0,r) domain. Direction remains correct. The audit
sampler uses the nearest boundary bin to avoid recursion/out-of-bounds access;
this is an engineering extension, not a paper-proven half-open remap. No claim
of exact seamless interpolation follows from direction round trips alone.

## Recovered controls and timestamps

Both controls read 1,189 LiDAR / 11,861 IMU messages, output 1,180 poses, and
match 1,180 GT pairs under the existing no-scale evaluator. They use 32 TBB
threads, unchanged TunnelD geometry YAML and the relocated bag/GT below.

| Control | ATE RMSE (m) | Trajectory SHA-256 | Historical parity |
|---|---:|---|---|
| Super photo off | 152.8314032003 | `9bfbc7d278f1177529b8a43b84f4c655e84a17625af144e68f29c0b1a44fcfff` | exact f306 P3 archived SHA |
| COIN/Super G1 | 3.5462094452 | `d71c1ab0b0e3cc542aed184fa9d1e9489b8a7880598a7403a2eb8c3df847bf63` | exact P2T/offline archived SHA |

Bag `/home/lc/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag`:
`afa448cd2ee32921cd514bb7d4c2e139f642bb164f66b1d556cf48c0c798406e`.
GT `/home/lc/bag/ENWIDE/gt-tunnel_d.csv`, already space-separated TUM bytes:
`25c7a20513b3c41e7a5f517119ff41bcf07329b6d87f3aeb8f5ed7725f5c922e`.
Both match historical hashes; no invented trajectory equivalence is used.

The inherited timestamp audit observes 148,215,754 valid raw Ouster queries over
1,180 normal scans, no raw queries beyond the full acquisition end, and 22,380
queries beyond available propagated history in four input-gap frames. Its column
`current_motion_fallback_points` counts unsupported *potential* dense queries;
it is not proof that COIN accepted those points. COIN correctly skips four
`imu_bracket_gap_or_order_invalid` frames, accepts 1,176 frames and has **zero
accepted motion fallback points**. The underlying data has real support gaps.
P4 dense deskew and input-policy validation have not been implemented, so zero
unsupported CUBE inputs is **not demonstrated**. Geometry controls continue by
their frozen policy. Livox has not been repaired or replayed.

The full build succeeds. CTest runs six registered tests: five pass and the P4
actual residual Jacobian acceptance test fails. The additional COIN feature math
executable passes. The failed gate is intentionally left visible as a failing
test; it is not marked expected-success. First sandbox replay failed before
estimator start because ROS XML-RPC sockets were denied; the authorized replay
outside that sandbox succeeded. Both failures and rerun logs are archived.

## Required answers and deferred work

| Question | Answer |
|---|---|
| 1. Official algorithm | All points deskewed; optional COIN Ouster prefilter; six-face intensity/range IDW; Gaussian derivative IGM; IGM-threshold pixels plus neighbors; inverse pixel/range reconstruction; persistent global points/IGM; joint IESKF residual Eq. (10), gradient Eq. (16), projection/transform chain; validate occupancy/range/occlusion/photo, refresh references and discover uncovered candidates. |
| 2. Wrong old assumptions | COIN bridge is not CUBE lifecycle; raw-owner requirement excludes paper-valid filled features; seam rejection differs; birth references are stale; blur/central derivative is a local approximation; weak-direction ranking is not primary CUBE selection. |
| 3. Reusable f306 components | Super propagation/geometry/ESKF, dense parser concepts, time-support helpers, 32T runner/evaluator, IDW source-only/depth-check concept and Ouster prefilter. |
| 4. Rewritten components | Independent published-order projector/inverse; range+intensity raster with support metadata; sampled Gaussian derivatives; W/seam sample; Eq. (16) gradient; scalar state residual audit. No production feature manager rewritten yet. |
| 5. Eq. (3)–(16) match | (3)–(8),(10)–(16) represented in isolated module; (9) used by synthetic global construction. Feature management and estimator path incomplete. Eq. (16) matches the stencil but actual interpolated derivative fails. Exact boundary W domain needs a convention. |
| 6. FD tests | Projection/transform pass; actual photometric residual gate fails. |
| 7. Seam remapping | Direction equivalence passes; 24 supported seam stencils sampled; exact half-open positive boundaries require an explicit extension. Derivative consistency fails, especially near seams. |
| 8. Semi-dense feature selection | Not implemented, gated before lifecycle work. No COIN selector used by P4 math. |
| 9. IGM constancy | Yes in isolated residual module; not enabled in production estimator. |
| 10. Supported photo inputs | COIN accepted inputs: zero fallbacks. CUBE dense path: not established. |
| 11. TunnelD CUBE ATE | **NOT RUN**: Gate A fails. Gates B (<10 m) and C (<1 m) untested. |
| 12. Comparisons | Exact recovered controls above; old f306/P3 IGM ~152–159 m and P3A ~146–170 m are historical local failures, not P4 results. Paper CUBE 0.317 m uses a different geometry backend. |
| 13. Ablations | Not run because Gate B has not passed. Raw ablation must preserve IGM feature locations in any later authorized continuation. |
| 14. Local choices | See parameter provenance: resolution, IDW settings, Gaussian discrete normalization/truncation, masks, boundary bin/interpolation and FD tolerance. Production selection/covariance/robust parameters are not frozen yet. |
| 15. Ready for Livox | No; math gate, CUBE input support and TunnelD Gate B remain unresolved, and known Livox history defect is unrepaired. |

Pending after resolving this gate: semi-dense selector, refreshed persistent
feature manager, supported dense prefilter/deskew pipeline, joint estimator
integration, production CUBE diagnostics CSV, full TunnelD accuracy and
post-viability ablations. These components and results are marked unavailable
in the review bundle. Finishing them now would cross the explicit stop condition.

For continuation, the research specification must reconcile how Eq. (16)'s
approximation is judged against the actual residual derivative, and fix the
boundary/interpolation convention. Accepting that approximation and retaining
the published stencil would differ from requiring an exact bilinear derivative;
changing the stencil would require an explicitly documented departure from
the published equation. This phase does not choose either silently.
