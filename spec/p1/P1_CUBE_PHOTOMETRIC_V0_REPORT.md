# P1 CUBE photometric V0 report

Verdict: **PARTIAL / NO-GO**. NTU passes its 5% regression gate, but ENWIDE
TunnelD becomes 15.01% worse, exceeding the 10% stress gate. A real cubemap/IGM
residual and correct local Jacobians add nonzero information, but this first
configuration does not establish reliable intensity-assisted stress localization.
Shield1 RMSE improves while its maximum error increases; its trajectory remains
unusable. No parameters were tuned after observing these outcomes. Stop for review.

User scope correction: TunnelD replaces Runway. No Runway result is claimed.
Only eee_01, Shield1 and TunnelD were evaluated; Shield4/5 were discovered and
not substituted or selected against GT scores.

Repository: `https://github.com/Scar-c/cube_lio.git`. P0 final:
`1b0698efe63fae09f9d498a285c3feb5b1b69cbd`, branch `p0-super-lio-baseline`.
Final experimental code: `71e92770102f00cc68002868447baee3f92df220`, branch
`p1-cube-photo-v0`; the final report/archive commit is its branch tip after
closure. Implementation commit before the IMU-gap fix: `61c97bc`.
Super-LIO ros1 base: `60b57aaac8dc397f80c56364e7ccb008c300cc29`;
ros2 inspected: `f89f48dc7aea6cfa262f18e4d03b319e04e0dbd2`.
COIN-LIO main reference: `76729cc4feb3649cbd79d28f82d9f62a2c82889b`.
GEODE evaluator/calibration reference: `c6e930623d4fed450d7fc50e16e3ffe0288b692b`.
NTU author benchmark: `194dd4595b1fb5e8ae2a5a0c01255f816ab4082f`.
Starting repository was empty, so starting HEAD was absent. `main` was not created
or modified. Both staged branches are pushed and their remote SHAs verified at
closure; commands are recorded in `artifacts/p1/commands.md`.

Detailed equations, design provenance, all adopted engineering policies,
face conventions, image construction, IDW, IGM, selection/lifecycle, analytic
Jacobian, sign, robust weighting and diagnostics are specified in
[ALGORITHM_AND_PROVENANCE.md](ALGORITHM_AND_PROVENANCE.md). That document is
part of this report. The [CUBE public slides](https://www.docswell.com/s/scomup/59N8N9-2026-03-22-173102)
are algorithm authority; no unpublished source details are claimed.
COIN inspired bounded world landmarks with fixed birth values and visibility
gating. All photometric C++ is original GPLv3 code, not copied BSD source;
Patrick Pfreundschuh/COIN attribution is explicit in code and the provenance
document. Upstream Super-LIO license and copyright notices remain intact.
No Ouster spherical projector, IKFoM, ikd-tree, complementary ranking, camera,
ML, raw-intensity residual or intensity-weighted geometry substitution is used.

Retained architecture: Super-LIO owns IMU propagation, its deskew, ESKF update,
geometry residuals, voxel downsampling, OctVox/HKNN and map updates. ESKF and
map/search sources retain their P0/upstream hashes; geometry configuration and
evaluator SHA stay frozen. Extra dense input and dense intensity deskew are
side channels. `Observe()` invokes photo preparation, adds Ap/bp to the existing
6×6/6 accumulator at every iteration, then replenishes after UpdateObserve.
The photo-off final executable produces byte-identical P0 trajectories on all
three sequences. No new geometry residual or estimator formulation is introduced.

Build: Ubuntu 20.04.6, ROS Noetic, GCC 9.4.0, C++17 Release/-O3, AMD Ryzen 9
7945HX, 32 logical CPUs. Catkin -j4; TBB concurrency 4 for every experiment,
OpenCV internal threads 1 on photo-on runs. ROS1 is used because ROS2 is absent
on this machine, as permitted by the prompt. P0 clean build and P1 rebuild
succeeded. Binary hashes and dependency versions are in
`artifacts/p1/build_identity.json`; invariant source/config hashes are in
`artifacts/p1/frozen_identity.json`.

Mandatory tests passed before any full A/B: actual implementation's projection
finite differences cover 6000 random points on all six faces, with maximum
absolute error **8.89473361632e-9** (tolerance 2e-7). Complete pose residual test
covers 1120 random points/all faces, three local rotation and three global
translation axes, nonidentity extrinsics, actual Gaussian/IGM and exact bilinear
sampling: maximum error **3.78524589451e-9** (tolerance 2e-6). Steps are 1e-6.
Face ties/one-sided discontinuities, invalid image masks, IDW depth discontinuity,
same-surface filling and information descent sign also pass. CTest 1/1 passed.
Raw test result: `artifacts/p1/jacobian_tests.json`. Integration assertions
verify bounded feature count, startup zero information, immutable frozen sigma,
no same-frame births, matching timestamp vectors, source/config/evaluator/GT
identities, and photo-off byte parity. Lifecycle checks pass for all runs.

| Dataset | Bag actually used | LiDAR topic / IMU topic | Type / intensity |
|---|---|---|---|
| NTU eee_01 | `/home/lc/super_livo/bag/NTU/eee_01/eee_01.bag` | `/os1_cloud_node1/points` / `/imu/imu` | ROS1 PointCloud2, OS1-16; intensity float, first scan 0–1820 |
| GEODE Shield1 | `/home/lc/algorithm_versa/bag/GEODE/Shield_tunnel1_gamma.bag` | `/livox/lidar` / `/imu/data` | ROS1 CustomMsg, Avia; reflectivity uint8 0–255 |
| ENWIDE TunnelD | `/home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag` | `/ouster/points` / `/ouster/imu` | ROS1 PointCloud2, OS1-128; intensity float, first scan 1–5018 |

Ranges are explicitly sampled, not full-sequence extrema. Point fields are
archived in P0. Geometry input's filter_rate remains 3; photo input preserves
all range/tag-valid points before that filter and uses its own dense deskew.
Mean dense points per processed frame are about 11829, 23629, 69613 respectively;
geometry continues using its normal voxel cloud.

Frozen sensor configs: `tools/cube_lio/config/{eee_01,shield1,tunnel_d}.yaml`.
NTU T_BL=(-0.05,0,0.055), identity rotation; Shield uses official gamma
T_IMU_LiDAR and external IMU; TunnelD sensor-frame points use inverse
IMU-to-sensor translation (-0.00625,0.011775,-0.007645), identity rotation,
confirmed by Ouster metadata. Exact rotation arrays are in the configs.
The calibration loader expects column-major arrays, accounted for in Shield.

GT conventions and evaluator are frozen in P0 and unchanged here:
NTU `/leica/pose/relative` uses header epoch seconds, author duplicate removal,
linear interpolation with strict <0.1 s bracket, and body→prism translation
(-0.293656,-0.012288,-0.273095). Shield `Shield_tunnel1.txt` is position-only
Leica GT; official gamma→Leica conversion applies T_device*inverse(T) to the
estimated poses. TunnelD uses `gt-tunnel_d.tum`, prism-in-IMU translation
(-0.006253,0.011775,0.10825), epoch seconds. Stress association follows evo
shorter-to-longer nearest matching, max difference 0.1 s and offset 0. All use
SE3 Umeyama alignment without scale. No time-offset or extrinsic fitting.
Evaluator SHA: `6fb3c28d376dd073b517204c249e092b63b305700e8a005b210087250c9b76bf`;
preserved NTU wrapper SHA:
`092beba2b99ac02cfbb1d30b1c0b1ec49cf2b41203090a81c66eb0d0824187dd`.

One official conservative configuration: N=96, IDW radius/k/power/support=3/6/2/3,
range gate 0.3 m+0.02*range, Gaussian sigma=1, high response>=5, max features
1200, suppression radius 2, max age 10. Noise is robust residual MAD calibrated
from the first 20 frames and frozen per sequence; weight=1, Huber delta=1.345,
hard gate=4.685 sigma. Frozen sigmas: NTU 2.373174018, Shield1 2.306278274,
TunnelD 4.311262820. No GT is used for those scales. All ablation switches
requested by the prompt are exposed in `photo.yaml`.

| Dataset | Geo ATE m | Photo ATE m | Delta m | Delta % | Photo info nonzero? | Gate |
|---|---:|---:|---:|---:|---|---|
| eee_01 | 0.118875639 | 0.114437817 | -0.004437822 | -3.7332% | Yes | PASS |
| Shield1 | 168.191541666 | 160.079633869 | -8.111907797 | -4.8230% | Yes | PASS RMSE; severe drift |
| TunnelD | 113.919295548 | 131.018887796 | +17.099592249 | +15.0103% | Yes | FAIL >10% |

A/B output frame counts are 3981/3981, 5418/5418, 1179/1179. Complete timestamp
vectors are identical within each pair. Matches are 3329, 1388, 1179. Processed
durations are 398.0009, 541.7029, 117.9130 s; every relevant input bag message
is delivered. Startup removes the same 6/9/10 scan outputs through upstream
initialization in A and B; no deliberate frame dropping occurs.
Per-run ATE mean/median/max and GT/trajectory hashes are in metrics.json.
Shield maximum error rises **304.363→438.318 m** despite RMSE improvement.
TunnelD maximum error rises 164.560→182.891 m. Neither stress trajectory is
acceptable as recovered localization; improved RMSE alone is insufficient evidence.

| Dataset | Geo wall s | Photo wall s | Wall overhead | Geo Observe ms | Photo Observe ms | Geo / photo peak RSS KiB |
|---|---:|---:|---:|---:|---:|---:|
| eee_01 | 13.6542 | 24.4293 | +78.91% | 2.35380 | 5.11599 | 164336 / 171508 |
| Shield1 | 12.7964 | 31.0310 | +142.50% | 1.50000 | 4.73105 | 109128 / 135952 |
| TunnelD | 11.0395 | 17.0421 | +54.37% | 1.01301 | 9.84242 | 108836 / 129540 |

These are serial full-run measurements from the same executable/source and
machine, one replicate per final mode. Wall processing includes bag reads and
ROS publications, excludes master startup, evaluation and build. Cached disk
I/O affects wall ratios, especially dense TunnelD; do not infer exact speedups
from a single run. Observe includes frontend/image construction/replenishment;
the separate photo `update_ms` measures the iterative geometry+photo update.
PSS was not measured. No later-stage performance tuning was attempted.

| Dataset | Active mean | Valid mean | Mean trace Ag | Mean trace Ap | Mean photo info in weak translation | Median Ap/Ag in weak translation |
|---|---:|---:|---:|---:|---:|---:|
| eee_01 | 15.45 | 9.06 | 1.0994e9 | 3.5089e5 | 416.22 | 0.000305 |
| Shield1 | 51.88 | 32.41 | 3.4102e8 | 2.0248e5 | 3816.42 | 0.009440 |
| TunnelD | 1170.89 | 701.34 | 4.4842e7 | 4.0771e7 | 802761.78 | 8.803205 |

Positive photo information occurs in 99.22%, 99.59%, 98.30% of frames,
including startup where the term is intentionally zero. In the weakest 10% of
geometry translational-block frames, nonzero contribution fractions are
95.24%, 99.26%, 99.15%, with median weak-direction Ap/Ag ratios 0.00248,
0.03327, 14.60124. This confirms implemented information injection. It does
not prove those constraints are unbiased or truly independent physical
measurements: neighboring IGM samples share image support. Geometry block
eigenvectors are local diagnostics, not full marginalized observability.

TunnelD has much larger effective photo influence than the two other sensors.
Together with 15% ATE degradation, this is evidence to review normalization,
correlated residuals and visibility/appearance consistency. It is an inference,
not a proven causal diagnosis. Analytic chain/sign tests and photo-off parity
are already verified; no failure is hidden by changing geometry or scale.

Mean frontend/photo times (deskew/raster/IDW/IGM/Jacobian/iterative update/
replenishment), in ms: NTU 0.245/0.606/1.075/0.157/0.030/2.753/0.248;
Shield1 0.575/1.211/0.934/0.160/0.047/1.552/0.266;
TunnelD 1.488/3.242/0.986/0.292/0.238/1.268/2.527.
Full rejection/coverage/eigenvalue statistics are archived in metrics.json.
Per-frame CSV is ignored runtime evidence. Mask coverage is a major rejection source:
mean invalid rejections 5.59/19.16/157.17. TunnelD also has mean 187.36 hard
outlier rejections and 122.16 FOV/seam rejections. Sparse NTU high-response
feature coverage is small despite the nominal 1200 cap, and is openly reported.

IDW-off NTU ablation: ATE 0.118875639 m, zero active features and zero photo
information, identical to geometry-only. Its input is too sparse to satisfy
the strict Gaussian/IGM masks without IDW. This validates the switch but is
not evidence that photo works without filling. `ablation_summary.csv` records
this alongside all official modes. No weight/feature/lifetime sweep was run.

Known failure corrected before final runs: initial TunnelD photo run aborted
at an inherited IMU gap because the side channel demanded >=2 propagation
states. The corrected module marks exactly one final frame unsupported,
sets photo information to zero, preserves all output rows and avoids births.
Both A/B retain one duplicate state timestamp, explicitly counted. Final full
TunnelD replay and lifecycle assertions cover this regression. All three
final A/B pairs were rerun from the same fixed code, and initial partial
results are excluded. Noise/geometry parameters were not changed by the fix.

Limitations: severe baseline and photo stress drift; stronger TunnelD photo
influence with worse ATE; no patch NCC or image brightness correction; short
bounded landmarks; restrictive sparse-mask coverage; seam rejection; static
fallback outside interpolation coverage; inherited state-time/scan-end
mismatch and unsorted point-time assumptions in the upstream geometry path;
float ESKF state with double photo reduction; one official run per mode; no
claim of exact parity with unpublished CUBE code. Intensity counts/info alone
cannot establish a reliable photometric model.

Required next step, **not executed**: review the frozen TunnelD residual,
visibility and geometry-weak direction evidence to determine whether the
large normalized/correlated photo contribution is consistent with a defensible
noise model, before authorizing any new parameter or frontend study. Current
P1 acceptance is rejected. Stop after archiving, committing, pushing and
remote verification. No complementary selection, long-term photo map,
multi-scale image, extra dataset, BIEVR integration or research stage follows.
