# CUBE-LIO Reproduction — CLI/Codex Master Prompt (P0–P1)

You are the implementation agent for a new LiDAR–Inertial Odometry repository:

- **Working repository:** `https://github.com/Scar-c/cube_lio.git`
- **Local workspace:** `/home/lc/cube_lio`
- Current workspace shape starts with: `/home/lc/cube_lio/src`
- **Dataset root (read-only):** `/home/lc/super_livo/bag`

The project goal is to reproduce the **intensity-assisted part of CUBE-LIO** on top of **Super-LIO**, while using **COIN-LIO as the primary implementation reference for the photometric observation pipeline**.

This is a staged project. **For this task, complete P0 and P1 only, then STOP and report. Do not continue into later optimization/research stages without a new prompt.**

---

# 0. Non-negotiable architecture decision

Use the following roles exactly:

## BASE: Super-LIO

Official repository:

- `https://github.com/Liansheng-Wang/Super-LIO.git`
- Use its **ROS2/default `ros2` branch** as the implementation base unless the local machine clearly requires otherwise.
- At execution time, record the exact branch and commit SHA used.

Super-LIO owns:

- IMU propagation
- point-cloud deskewing
- ESKF / iterated observation update
- geometry scan-to-map registration
- OctVox / compact geometry map
- HKNN correspondence search
- geometry map update
- ROS2 input/output and dataset playback plumbing

Do **not** replace Super-LIO's geometry map with ikd-tree/FAST-LIO2 structures.
Do **not** replace its ESKF with IKFoM.
Do **not** rewrite its geometry pipeline just to look like COIN-LIO.

Relevant Super-LIO source checkpoints to inspect before modifying anything:

- `SuperLIO::Propagation_Undistort()`
- `SuperLIO::Observe()`
- `SuperLIO::UpdateMap()`
- `ESKF::UpdateObserve()`
- `OctVoxMap`

Important implementation property already established from source review:
Super-LIO's observation callback directly accumulates the 6-DoF information-form terms

```text
H^T R^-1 H
H^T R^-1 r
```

Therefore the CUBE photometric observation should be added in the same style; **do not allocate an unnecessary giant stacked H matrix just to imitate COIN-LIO.**

## PRIMARY PHOTOMETRIC REFERENCE: COIN-LIO

Official repository:

- `https://github.com/ethz-asl/COIN-LIO.git`
- Use `main` as reference.
- At execution time, record the exact commit SHA inspected.

COIN-LIO is a **reference**, not the code base.

Study and reuse/adapt the *design* of:

- `FeatureManager::updateFeatures()`
- `FeatureManager::trackFeatures()`
- feature lifetime / replenishment
- world-frame 3D photometric landmarks
- fixed reference photometric value at feature birth
- FOV gating
- range / occlusion consistency gating
- NCC / appearance consistency concepts where appropriate
- `h_share_model_photometric()`
- `h_share_combined()`
- `Projector::projectionJacobian()` as a reference for how projection Jacobians are integrated into a pose Jacobian

Do **not** import these COIN-LIO-specific parts as the final CUBE implementation:

- Ouster-only/equirectangular projector
- Ouster beam lookup tables
- FAST-LIO2 / IKFoM estimator
- ikd-tree map
- COIN's raw-intensity patch residual as the final residual
- COIN's complementary sparse feature selection as the first CUBE implementation

COIN-LIO photometric code is BSD-licensed in the relevant files. Preserve attribution/license headers for any code that is directly adapted or copied.

## AUTHORITATIVE CUBE-LIO ALGORITHM REFERENCE

CUBE-LIO public technical slides:

- `https://www.docswell.com/s/scomup/59N8N9-2026-03-22-173102`
- Paper title: **Cubemap-Based LiDAR-Inertial Odometry with Intensity Assistance**

Treat the official CUBE-LIO slides as the algorithm authority for:

- six-face cubemap projection
- intensity cubemap construction
- sparse-image filling / IDW concept
- IGM (intensity gradient magnitude)
- semi-dense high-response intensity features
- photometric residual on current IGM
- analytic projection Jacobian chain
- geometry + photometric joint state update

Do not claim unknown details from the unpublished/unavailable official source code as facts.
When CUBE leaves an implementation detail unspecified, explicitly label the adopted policy as either:

- `COIN-derived engineering choice`, or
- `our conservative implementation choice`.

## OPTIONAL NON-AUTHORITATIVE REFERENCE

`rsasaki0109/localization_zoo` contains a CUBE-style prototype, but source review has already established that its `cube_lio` implementation uses intensity mostly as a correspondence weight and **does not implement the full image-space CUBE photometric residual/Jacobian**.

It may be consulted for trivial cubemap/rasterization ideas only.
It must **not** be treated as the algorithm authority.

---

# 1. License / repository hygiene

Super-LIO is GPLv3. Since this repository is derived from Super-LIO, preserve the GPLv3 license and all required copyright/attribution notices.

Before code changes:

1. Inspect `/home/lc/cube_lio` and verify git status/remotes.
2. Record starting HEAD.
3. Do not modify any files under `/home/lc/super_livo/bag`.
4. Do not commit downloaded datasets, bags, build trees, logs, generated maps, or large experiment artifacts.
5. Add suitable `.gitignore` entries for:
   - `build/`
   - `install/`
   - `log/`
   - local reference clones
   - experiment outputs
   - generated PCD/maps
   - rosbag outputs
6. External reference repositories should be cloned/read outside the tracked source tree or into a gitignored `refs/` directory.
7. Record exact reference SHAs in the final report.

No camera, no visual frontend, no ML/deep learning.
This project is LiDAR intensity + IMU only.

---

# 2. Dataset discovery — do not guess filenames

Dataset root:

```bash
/home/lc/super_livo/bag
```

Find and identify the existing local data corresponding to:

1. **NTU `eee_01`**
2. **GEODE `shield`**
3. **ENWIDE `runway`**

Do not download replacement datasets if these local bags exist.
Do not assume exact filenames.
Use filesystem discovery and ROS bag metadata to identify them.

For each selected dataset, record:

- exact bag path
- ROS1/ROS2 bag format
- LiDAR topic
- IMU topic
- LiDAR model/message type
- whether intensity/reflectivity exists and its numeric range
- ground-truth topic/file and timestamp convention
- sensor extrinsics/config used

If a dataset cannot be run because a required local topic/config/GT is genuinely absent, document the blocker precisely and continue with all datasets that are runnable. Do not fabricate data or silently substitute another sequence.

---

# 3. Evaluation protocol — freeze before adding photometric terms

The experiments are A/B tests. Freeze one evaluator and use it for both geometry-only and photo-on runs.

For every sequence record at minimum:

- trajectory file
- aligned ATE RMSE
- ATE mean / median / max if evaluator supports them
- frame count
- processed duration
- wall-clock runtime
- mean frontend/update time if instrumentable
- peak RSS/PSS if easy to capture
- number of active photometric features per frame
- number of valid photometric residuals per frame
- photometric information contribution statistics

Use the same trajectory alignment, time association and GT preprocessing for baseline and photo-on.
Document the exact evaluation command.

Do **not** repeatedly tune parameters against ground-truth ATE.
Use NTU `eee_01` as the non-degenerate regression sequence and Shield/Runway as stress sequences.

---

# 4. Branch / phase policy

Work in explicit staged branches.

## P0 branch

```text
p0-super-lio-baseline
```

Purpose: establish a clean, reproducible Super-LIO-derived baseline inside `cube_lio`.

## P1 branch

```text
p1-cube-photo-v0
```

Create this only after P0 passes its baseline gate.

Purpose: implement the first real CUBE-style intensity observation and evaluate it.

Push both branches and all final commits to `origin`.
Keep `main` untouched unless it is only the empty initial repository and importing the base is necessary; still prefer a staged branch.

---

# 5. P0 — Super-LIO baseline import and reproducibility

## P0.1 Import the base

Import/adapt Super-LIO ROS2 into `/home/lc/cube_lio/src` while preserving its behavior.

Prefer minimal structural changes.
Avoid gratuitous renaming/refactoring before parity is established.
If the ROS package is renamed from `super_lio` to `cube_lio`, do that in one isolated commit and verify it changes no estimator behavior.

The initial objective is not elegance; it is a trustworthy baseline.

## P0.2 Build

Build from a clean workspace.
Record:

```text
OS / ROS distro
compiler
CMake build type
CPU model
thread count / TBB settings
Super-LIO reference SHA
cube_lio HEAD
```

## P0.3 Baseline runs

Run **geometry-only** on:

- NTU `eee_01`
- GEODE `shield`
- ENWIDE `runway`

Use appropriate configs for each sensor, making the minimum dataset adapter changes necessary.

### Mandatory P0 result

NTU `eee_01` must run end-to-end with a valid trajectory and evaluable ATE.

If the imported baseline differs from an existing known local Super-LIO `eee_01` result, investigate before P1.
Do not proceed if the baseline is obviously broken.

For Shield/Runway, obtain geometry-only baseline trajectories and ATE whenever GT is available.

## P0.4 Archive

Create:

```text
spec/p0/P0_SUPER_LIO_BASELINE_REPORT.md
artifacts/p0/metrics.json
artifacts/p0/commands.md
```

Do not commit huge raw logs. Small CSV/JSON summaries are fine.

Commit and push P0.

---

# 6. P1 — CUBE-style intensity observation V0

P1 must keep Super-LIO's geometry pipeline untouched unless an interface change is strictly necessary.

The photometric pipeline is a side channel.

Target architecture:

```text
                    Super-LIO
              +-------------------+
LiDAR + IMU ->| deskew / predict  |
              +---------+---------+
                        |
              +---------+----------+
              |                    |
              v                    v
       geometry branch       intensity branch
          OctVox/HKNN           Cubemap
              |                   IDW
       point-to-plane             IGM
              |              semi-dense features
          Jg, rg             local photo cache
              |                   Jp, rp
              +---------+----------+
                        v
                 information form
                 A = Ag + Ap
                 b = bg + bp
                        |
                 Super-LIO ESKF
```

## P1.1 Preserve intensity through Super-LIO preprocessing

Verify raw LiDAR intensity/reflectivity survives:

- input conversion
- deskew
- any downsampling used for the intensity frontend

Do not use the geometry-downsampled cloud if it destroys the image density needed for the cubemap. It is acceptable to use the full deskewed scan for the intensity image while geometry continues to use its normal downsampled cloud.

## P1.2 Cubemap projector

Implement a sensor-centric six-face cubemap.

Required API concepts:

```text
3D direction -> face id + continuous (u,v)
continuous (u,v) -> sample current image/IGM
analytic projection Jacobian d(u,v)/d(p)
```

Requirements:

- deterministic face selection
- continuous subpixel coordinates
- explicit validity mask
- no atan2/asin spherical projection in the final cubemap path
- configurable face resolution
- unit tests for all six faces and boundaries

### Mandatory Jacobian test

Compare analytic cubemap projection Jacobians against finite differences on randomized valid 3D points.
Fail the test if errors exceed a tight numerical tolerance appropriate for double precision away from face discontinuities.

Boundary/discontinuity points must be tested separately and handled explicitly rather than hidden.

## P1.3 Cubemap intensity construction

From the **deskewed current scan**, construct six intensity faces.

Keep separate arrays for at least:

- intensity
- range/depth or equivalent visibility support
- validity
- IGM

Do not turn this into a global high-resolution intensity map.
The cubemap is current-frame/sensor-centric.

## P1.4 IDW sparse filling

Implement the CUBE-style IDW concept for pixels that need interpolation from sparse LiDAR samples.

Because exact unpublished author parameters are unknown:

- expose neighborhood radius / k / power / validity threshold in YAML
- use conservative defaults
- document them as `our implementation choice`
- never fill across invalid geometry indiscriminately
- use range/depth consistency so IDW does not smear across foreground/background discontinuities

Provide a switch to disable IDW for ablation.

## P1.5 IGM

Build intensity gradient magnitude from the filtered cubemap intensity image.

Conceptually:

```text
M(u,v) = ||grad I(u,v)||
```

Use a Gaussian derivative or an equivalent mathematically documented implementation.
Do not substitute raw intensity as the final CUBE residual.

Store image gradients needed for the photometric Jacobian.

## P1.6 Semi-dense feature detection

Follow CUBE's high-response/semi-dense idea first.
Do **not** start with COIN's degeneracy-aware complementary feature ranking.

Feature candidate should contain at minimum:

```text
world-frame 3D position p_G
reference IGM value m_ref
birth frame/time
lifetime
optional face/reference metadata for debug
```

Feature-count thresholds and suppression rules must be configurable.

## P1.7 Photometric feature lifecycle — borrow from COIN-LIO

CUBE's public slides do not fully specify the feature-cache lifecycle, so use a bounded **COIN-derived engineering policy**:

- store 3D feature points in world coordinates
- keep the reference photometric value fixed from feature birth
- project old landmarks into the current cubemap each frame
- reject out-of-FOV landmarks
- reject invalid current cubemap samples
- reject range/depth inconsistent landmarks (occlusion/disappearance)
- use a bounded maximum lifetime
- use a bounded maximum active feature count
- replenish removed/dead features *after the current state update*

Do not let a feature detected from the current image immediately constrain the same image/state update.

Do not create an unlimited trajectory-length photometric map.

All lifecycle parameters must be YAML-configurable.

## P1.8 CUBE photometric residual

For a historical photometric feature `(p_G, m_ref)`, use the current estimated pose to transform it into the current LiDAR frame and project it into the current cubemap/IGM.

Core residual:

```text
r_photo = M_current( Pi_cube( T_LG * p_G ) ) - m_ref
```

Use the exact transform convention of the Super-LIO state.
Do not guess sign/order.

The pose Jacobian must follow the chain:

```text
dM/du * dPi_cube/dp * dp/dxi
```

where `dM/du` is the local gradient of the current IGM image.

### Mandatory residual Jacobian unit test

Numerically differentiate the complete photometric residual with respect to the 6-DoF pose perturbation and compare against the analytic Jacobian.

Test:

- translations x/y/z
- rotations roll/pitch/yaw
- several cubemap faces
- random points away from face discontinuities

This test must pass before running full dataset evaluation.

## P1.9 Fusion into Super-LIO ESKF

Do not rewrite `ESKF::UpdateObserve()`.

During each iterative observation evaluation, compute:

```text
Ag = sum J_geo^T W_geo J_geo
bg = sum J_geo^T W_geo r_geo

Ap = sum J_photo^T W_photo J_photo
bp = sum J_photo^T W_photo r_photo
```

then return:

```text
A = Ag + Ap
b = bg + bp
```

using Super-LIO's existing sign convention exactly.

Prefer TBB-compatible accumulation, consistent with Super-LIO's current parallel observation architecture.
Do not introduce an avoidable serial photometric loop if the feature count is large.

### Photometric scaling

Do **not** blindly copy COIN-LIO's `photo_scale=0.00095`, because CUBE uses IGM and therefore different units/statistics.

Expose a `photo_weight` / noise model in config.
Initialize it using residual statistics rather than GT ATE tuning.
A reasonable first method is robust normalization from early valid residual distributions and then freeze the scale for each run.

Use a robust loss/gate for gross photometric outliers.
Document the exact formula.

## P1.10 Debug/diagnostic instrumentation

Log enough information to diagnose whether the photometric term actually adds independent constraints:

Per frame or summarized:

- active features
- valid photo residual count
- photo residual mean/RMS/median
- rejected by FOV
- rejected by invalid/IDW mask
- rejected by range/occlusion
- `trace(Ag)` and `trace(Ap)`
- eigenvalues/eigenvectors of geometry-only pose information when useful
- projection of `Ap` onto geometry-weak translational directions
- time spent in cubemap / IDW / IGM / photo Jacobian / total update

For FSS-like planar degeneracy, the important mechanism check is that the photometric term adds non-zero information in geometry-weak planar directions.

---

# 7. P1 experiments and gates

Run paired A/B experiments with **identical geometry settings**:

```text
A = Super-LIO geometry-only
B = same system + CUBE photometric observation
```

Do not change geometry/map parameters between A and B.

## 7.1 NTU eee_01 — regression/control sequence

This is the first gate.

Goal: adding intensity should not materially damage an already well-behaved LIO trajectory.

### PASS gate

Photo-on aligned ATE RMSE must not be more than **5% worse** than the frozen geometry-only baseline.

Report:

```text
ATE_geo
ATE_photo
absolute delta
relative delta %
```

If photo improves ATE, report it but do not overclaim.

If regression is >5%:

- mark P1 `NO-GO / HOLD`
- diagnose sign/Jacobian/scaling/outlier issues
- do not proceed into aggressive parameter tuning against GT
- do not hide the failure by changing geometry parameters

## 7.2 GEODE shield — degeneracy stress test

Run the exact same frozen geometry configuration in A/B mode.

Report ATE and photometric observability diagnostics.

Desired result:

- photo-on improves or stabilizes ATE vs geometry-only
- geometry-weak directions receive meaningful `Ap` information

## 7.3 ENWIDE runway — intensity-assisted stress test

Same A/B protocol.

Report ATE and photometric observability diagnostics.

Desired result:

- photo-on improves or stabilizes ATE vs geometry-only
- no catastrophic photometric drift

## Stress-sequence P1 acceptance rule

For this first implementation:

- neither Shield nor Runway may become **>10% worse** in ATE from photo-on
- at least **one** of Shield/Runway should improve in ATE **or** show a clearly documented recovery/stability improvement with matching photometric information evidence

If this rule fails, mark P1 `PARTIAL / NO-GO` and stop for review.
Do not compensate by GT-driven tuning sweeps.

---

# 8. Required ablations for P1

At minimum produce these switches/configs so later work is reproducible:

```text
photo.enable: true/false
cubemap.idw_enable: true/false
photo.weight: configurable
photo.max_features: configurable
photo.max_lifetime: configurable
photo.robust_gate/kernel: configurable
```

Do not launch a large hyperparameter search in P1.
Only one conservative default configuration is the official P1 result.

---

# 9. Explicitly forbidden shortcuts

Do NOT:

- use camera images
- use ML/deep learning
- call intensity merely a correspondence weight and claim CUBE photometric residual is reproduced
- use `w(intensity) * r_geo` as the final CUBE formulation
- replace Super-LIO OctVox with COIN/FAST-LIO2 ikd-tree
- replace Super-LIO ESKF with IKFoM
- claim `localization_zoo` is a complete CUBE-LIO reproduction
- tune dozens of parameters against GT until ATE looks good
- silently drop frames or evaluate different frame ranges between A/B
- use different trajectory alignment settings between A/B
- modify/delete the user's bag data
- commit build/install/log/bag/PCD artifacts

---

# 10. Expected code organization

Keep photometric code modular and separable from geometry. A suggested structure is:

```text
src/
  basic/                     # Super-LIO base dependency
  cube_lio/
    include/cube_lio/
      ... existing Super-LIO-derived headers ...
      intensity/
        cube_projector.hpp
        cube_image.hpp
        idw_interpolator.hpp
        igm.hpp
        photo_feature.hpp
        photo_feature_manager.hpp
        photo_observation.hpp
    src/
      ...
      intensity/
        cube_projector.cpp
        cube_image.cpp
        idw_interpolator.cpp
        igm.cpp
        photo_feature_manager.cpp
        photo_observation.cpp
    test/
      test_cube_projection.cpp
      test_cube_projection_jacobian.cpp
      test_photo_residual_jacobian.cpp
```

The exact paths may follow the imported Super-LIO package conventions, but maintain this separation conceptually.

---

# 11. Required P1 report

Create:

```text
spec/p1/P1_CUBE_PHOTOMETRIC_V0_REPORT.md
artifacts/p1/metrics.json
artifacts/p1/ablation_summary.csv
artifacts/p1/commands.md
```

The report must contain:

1. exact repo/reference SHAs
2. precise provenance of copied/adapted code
3. Super-LIO baseline architecture retained
4. COIN-LIO ideas reused
5. CUBE slide-defined parts implemented
6. implementation choices that are not specified by CUBE
7. cubemap equation/convention
8. IDW implementation
9. IGM implementation
10. feature lifecycle
11. residual equation
12. analytic Jacobian and finite-difference test result
13. Super-LIO information-form fusion equation
14. dataset/topic/config discovery table
15. A/B results for `eee_01`, Shield, Runway
16. runtime overhead
17. photometric information diagnostics
18. known failures/limitations
19. PASS / PARTIAL / NO-GO conclusion
20. exact next recommended step, but **do not execute it**

Include a compact final table like:

```text
Dataset         Geo ATE     Photo ATE    Delta      Photo info valid?    Verdict
NTU eee_01      ...         ...          ...        ...                  PASS/FAIL
GEODE shield    ...         ...          ...        ...                  ...
ENWIDE runway   ...         ...          ...        ...                  ...
```

---

# 12. Git requirements

At the end of each phase:

```bash
git status
git diff --stat
git log --oneline --decorate -n 10
```

Working tree must be clean after the final commit.

Push branches to:

```text
origin/p0-super-lio-baseline
origin/p1-cube-photo-v0
```

Final response to the user must include:

- P0 branch + final commit SHA
- P1 branch + final commit SHA
- remote SHA verification
- build status
- dataset paths actually used
- `eee_01` ATE A/B result and regression percentage
- Shield A/B result
- Runway A/B result
- Jacobian finite-difference test status
- photometric information-mechanism status
- report paths
- concise list of unresolved issues
- whether P1 is `PASS`, `PARTIAL`, or `NO-GO`

---

# 13. Stop condition

After P1 report, commit, push, and remote verification:

**STOP.**

Do not proceed to:

- COIN complementary feature selection
- long-lived photometric mapping
- multi-scale cubemap
- performance tuning
- further CUBE paper reverse engineering
- BIEVR integration
- extra datasets

Those are later stages and require a new prompt after review.

The current task is complete only when we have a trustworthy answer to this first question:

> Starting from a reproducible Super-LIO baseline, can a correctly implemented CUBE-style cubemap/IGM photometric residual be injected into Super-LIO's ESKF without materially regressing NTU `eee_01`, while producing useful intensity constraints on GEODE Shield and ENWIDE Runway?
