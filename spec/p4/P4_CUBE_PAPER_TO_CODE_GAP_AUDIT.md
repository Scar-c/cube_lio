# P4 paper-to-code gap audit (before production edits)

Baseline: `f3060768abee4d2ee2343b07edf62346c7cd0f87`, recovered from
`https://github.com/Scar-c/cube_lio`. Work branch:
`p4-faithful-cube-paper-reproduction`.

Primary source: the complete eight-page local PDF, **Cubemap-Based LiDAR-Inertial
Odometry with Intensity Assistance**, Liu et al., ICRA 2026,
DOI 10.1109/ICRA57385.2026.11697017. Pages 8446–8453 were read before
production edits. Equations (3)–(9) are on pp. 8448–8449; (10)–(16) and feature
validation are on pp. 8449–8450. PDF identity is archived in the review bundle.

Evidence labels: **paper-proven** means stated by this PDF; **repository-proven**
means demonstrated by the checked-out source or identified historical report;
**implementation choice** means an additional local rule; **hypothesis** means an
unverified explanation. Classification below refers to the unmodified baseline.

| Component | Baseline status | Source evidence and required action |
|---|---|---|
| 1. Face assignment | PARTIAL | Paper Eq. (4) dominant signed axis, paper order +X,-Y,-X,+Y,+Z,-Z. Header-only `cube_projector.hpp` uses +X,-X,+Y,-Y,+Z,-Z. Axis dominance compatible, ordering differs. Adopt published order in a separate P4 projector. |
| 2. Pixel projection | LOCAL APPROXIMATION | Eq. (3) uses r/2; baseline uses (r-1)/2 and different image axes. Adopt r/2; explicitly handle exact positive boundaries without changing the equation. |
| 3. Face axes | CONFLICTS WITH PAPER | Eq. (5) basis differs from header implementation, e.g. +X uses u=-Y,v=-Z in paper versus u=+Y,v=+Z. Copy all six bases exactly. |
| 4. Intensity raster | EXACT/COMPATIBLE | `CubeImage::build` builds six intensity faces. Nearest-range collision arbitration is an implementation choice, not an author setting. |
| 5. Range raster | EXACT/COMPATIBLE | `Face::depth` is Euclidean range, populated beside intensity. Retain this storage concept, not axial depth. |
| 6. IDW | PARTIAL | `CubeImage::fill` interpolates both channels from immutable raw support; depth-discontinuity rejection is local. Missing per-pixel support counts and fill distances. Radius/k/depth gates not specified in paper. |
| 7. Gaussian derivative IGM | LOCAL APPROXIMATION | `gradients` uses 3x3 Gaussian blur followed by central differences. Eq. (6) calls for first-order Gaussian derivative responses. Implement sampled derivative-of-Gaussian kernels and report normalization/truncation. |
| 8. Semi-dense selection | CONFLICTS WITH PAPER | `PhotoObservation::replenish` ranks original point owners, applies suppression radius/cap/optional weak-direction ranking. Paper III-D retains IGM-threshold pixels plus immediate neighbors. Implement pixel masks including filled pixels. |
| 9. Inverse projection | MISSING | No Eq. (7) API in baseline projector. Implement and test each face. |
| 10. Pixel/range reconstruction | MISSING | Baseline landmarks come from `points_[index]`, not Eq. (8). Reconstruct every selected pixel direction times interpolated range, no raw owner. |
| 11. Persistent global feature | PARTIAL | `Feature::world/reference` has Eq. (9)-like semantics, but lifetime and unrefreshed references differ. New CUBE manager stores world point plus current reference IGM. |
| 12. IGM residual | EXACT/COMPATIBLE | `PhotoObservation::observe` can use current-minus-reference IGM, but raw mode changes feature policy. New residual must be independent of COIN. |
| 13. Transform Jacobian | EXACT/COMPATIBLE | `residualJacobian` uses right local rotation/global translation and extrinsics; mathematically Eq. (13) with column permutation and LiDAR-frame extension. Paper transforms input to body before projection; P4 should project body points directly. |
| 14. Projection Jacobian | PARTIAL | Baseline quotient derivative structurally Eq. (14), with different scale/bases. Derive using published bases and r/2. |
| 15. Seam operator W | MISSING | `CubeImage::sample` and observation reject `q.seam` and margins. Implement inverse/reprojection Eq. (15) and cross-face interpolation. |
| 16. IGM central gradient | CONFLICTS WITH PAPER | Residual uses exact derivative of bilinear interpolant, while `grad_u/v` diagnostic Sobel is unused. Implement Eq. (16), then measure differences from residual finite differences honestly. |
| 17. Feature validation | PARTIAL | Range, validity, robust checks exist; occupancy suppression is different, no low-current-IGM check. Add occupancy, range, occlusion, low IGM and residual checks with separate counts. |
| 18. Feature refresh | CONFLICTS WITH PAPER | Retained baseline landmarks keep birth reference and expire after 10 frames. Paper III-F refreshes current IGM then discovers uncovered candidates. Refresh references, remove stale features on failed validation. |
| 19. Joint estimator | EXACT/COMPATIBLE | `SuperLIO::Observe` adds photo normal equations to geometry within ESKF iterations. Retain geometric equations and 18-state propagation; add independent opt-in CUBE rows. Paper uses F3 geometry; retaining Super is the explicitly authorized experimental boundary. |
| 20. Ouster prefilter | PARTIAL | Independent COIN `CoinImageProcessor` implements intensity scale, line-removal FIR, brightness normalization, blur and truncation. Baseline `PhotoObservation` receives unfiltered DN. Reuse image preprocessing only; do not reuse COIN features/NCC/fusion. |
| 21. Motion compensation | PARTIAL | Dense `pc_intensity` is built before geometry stride and deskewed in `prepare`. Unsupported individual queries silently use raw points. Ouster support helpers exist; require actual brackets for all accepted P4 points and record rejected input/frames. Livox remains gated. |

`src/super_lio/src/intensity/cube_projector.cpp` does not exist at f306: the
projector is header-only. This is verified, not an omitted audit source.
`intensity_representation.hpp` supplies scalar sampling/projection but no inverse,
pixel candidates, seam remapping or feature lifecycle. P4 will use an independent
CUBE frontend rather than stretching this legacy interface.

`intensity/coin/*` is an independent control: calibrated Ouster projection,
COIN-specific 5x5 patches, NCC lifecycle, geometric direction selection, fixed
fusion scaling. Reuse of its preprocessing is compatible with paper III-A;
reuse of its feature/observation lifecycle for primary CUBE conflicts with III-D–F.

Tests audited: `test_cube_photo.cpp` verifies baseline projection and exact
bilinear residual derivatives and explicitly requires seam rejection; it cannot
prove Eq. (16). `test_spherical_photo.cpp` similarly checks exact bilinear
derivatives and periodic sampling. `test_coin_feature_math.cpp` checks NCC,
weak-direction and ranking semantics; these are COIN control tests, not CUBE
acceptance. Keep legacy tests intact and add distinct P4 acceptance tests.

Offline tools: `run_experiment.sh`/`run.py` use direct rosbag dispatch, default 32
TBB threads, isolated master and identity files. Paths reference missing old
workspaces; recover paths using explicit overrides under `/home/lc/bag`. Frozen
evaluator uses nearest timestamps <=0.1 s, prism lever arm and no-scale SE3.
Relocated GT must be compared by hash before claiming parity. `validate.py`
compares trajectories and stable metrics; wall time is not a parity requirement.

Historical reports read: P2T, P3 final/design/projection, offline integration at
f306; P3A and P3R obtained with `git show` from their remote branches, without
changing the baseline. P3A cube bridge is negative evidence (146–170 m), not
algorithm authority. P3R proves 3,991,596 unsupported Shield4 photo inputs and
loss of its passing result when excluded. No Livox accuracy claim is permitted.

## Mathematical review risk identified before implementation

Eq. (16) is a one-pixel finite-difference approximation to an image derivative.
It is generally different from the infinitesimal derivative of a bilinear image
sample. Across seams the coordinate chart itself changes. The paper does not
specify a continuous image interpolant or FD acceptance tolerance. This is a
**paper-proven equation** plus a **mathematical inference**, not evidence that
the algorithm fails. P4 tests must separately report exact projection/transform
derivatives, faithful Eq. (16) stencil, and actual residual state finite differences
on smooth and textured data. A failed residual-Jacobian gate must not be hidden
by substituting the legacy bilinear derivative or relaxing a threshold after
observing results. The user's stop condition applies if these semantics cannot
be reconciled.

No sigma, resolution, feature cap, range/occlusion gate, photo covariance, robust
loss or exact interpolation settings are given by the paper. Any choices must
be in `P4_PARAMETER_PROVENANCE.md` before accuracy results. The hypothesis that
frontend corrections improve TunnelD remains untested at audit creation.
