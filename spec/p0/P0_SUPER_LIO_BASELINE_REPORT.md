# P0 Super-LIO baseline

P0 gate: **PASS** for pristine NTU behavior. Stress trajectories are complete
but have severe geometry-only drift; this is retained and reported, without
geometry tuning. ENWIDE TunnelD replaces Runway by explicit user correction.

Starting HEAD: none. `origin` was an empty repository. Initial import commit:
`02b4587`. The P0 final commit is the tip of `p0-super-lio-baseline`; its SHA is
also recorded by the P1 report after branch creation.

Base: official Super-LIO `ros1` at
`60b57aaac8dc397f80c56364e7ccb008c300cc29`. Default `ros2` inspected at
`f89f48dc7aea6cfa262f18e4d03b319e04e0dbd2`. This host is Ubuntu 20.04.6 with
ROS Noetic only; upstream explicitly provides ros1 for that environment.
ESKF.cpp is identical between inspected branches; core differences concern
timer labels and messages. The imported estimator, geometry map and HKNN files
remain byte-identical to the ROS1 reference. GPLv3 and author notices retained.
No package rename was performed.

COIN-LIO main inspected: `76729cc4feb3649cbd79d28f82d9f62a2c82889b`.
GEODE official evaluator repository: `c6e930623d4fed450d7fc50e16e3ffe0288b692b`.
[Super-LIO upstream](https://github.com/Liansheng-Wang/Super-LIO),
[COIN-LIO upstream](https://github.com/ethz-asl/COIN-LIO),
[CUBE slides](https://www.docswell.com/s/scomup/59N8N9-2026-03-22-173102).

Build: GCC 9.4.0, C++17, Release/-O3, upstream LTO settings, AMD Ryzen 9 7945HX,
32 logical CPUs, TBB limited to 4 threads. Clean `catkin_make -j4` succeeded.
Only changes needed for reproducibility are rosbag build dependency, explicit
generated-message target dependency, thin replay entry points invoking upstream
callbacks, and an output subclass recording upstream IMU poses at full precision.
All selected bag messages are delivered in record-time order, with `process()`
after every message and five EOF drain calls. No queue-driven playback loss.

| Dataset | Bag path | LiDAR / IMU | Format / sensor |
|---|---|---|---|
| NTU eee_01 | `/home/lc/super_livo/bag/NTU/eee_01/eee_01.bag` | `/os1_cloud_node1/points` / `/imu/imu` | ROS1 bag v2 / Ouster OS1-16 PointCloud2 |
| GEODE Shield1 | `/home/lc/algorithm_versa/bag/GEODE/Shield_tunnel1_gamma.bag` | `/livox/lidar` / `/imu/data` | ROS1 bag v2 / Livox Avia CustomMsg |
| ENWIDE TunnelD | `/home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag` | `/ouster/points` / `/ouster/imu` | ROS1 bag v2 / Ouster OS1-128 PointCloud2 |

Shield1 selected before evaluation from available Shield1/4/5; no score-based
selection. No other dataset replaces any requested sequence. Input first-scan
intensity ranges: NTU 0–1820 float, Shield reflectivity 0–255 uint8, TunnelD
1–5018 float. These are sampled ranges, not claimed full-sequence extrema.
All fields and samples are in `artifacts/p0/dataset_sample.json`.

Frozen configs: `tools/cube_lio/config/{eee_01,shield1,tunnel_d}.yaml`.
NTU uses upstream parameters unchanged except disabling cloud visualization;
T_BL translation (-0.05,0,0.055), identity rotation. Shield uses official gamma
T_IMU_LiDAR from its calibration YAML, flattened column-major for upstream's
Eigen constructor, with external `/imu/data`. TunnelD points are already in
`os_sensor`; inverse IMU-to-sensor translation is (-0.00625,0.011775,-0.007645),
identity rotation, confirmed from Ouster metadata and existing sensor config.
No camera processing. Geometry noise, voxel/map configuration are not A/B tuned.

Evaluation frozen before P1: `eval/evaluate.py`, SHA recorded in metrics. NTU
author wrapper is preserved from local known baseline with SHA
`092beba2b99ac02cfbb1d30b1c0b1ec49cf2b41203090a81c66eb0d0824187dd`;
upstream benchmark `ntu-aris/viral_eval` revision
`194dd4595b1fb5e8ae2a5a0c01255f816ab4082f` is pinned in the wrapper.
NTU GT `/leica/pose/relative` is extracted using header epoch seconds; estimate
IMU/body pose is shifted by R_WB*(-0.293656,-0.012288,-0.273095).
GT duplicates are removed using the author rule and linearly interpolated
with strict bracket <0.1 s. SE3 Umeyama without scale.

Shield GT is `Shield_tunnel1.txt` (position-only, zero quaternion ignored).
Official gamma2GT_leica transform is applied to estimates using T_device*inv(T)
with its unchanged published quaternion/translation. TunnelD GT is
`gt-tunnel_d.tum` and prism translation (-0.006253,0.011775,0.10825) is applied
to estimated IMU poses. Stress sets use evo 1.31.1's shorter-to-longer nearest
association, max difference 0.1 s, offset 0, SE3 no scale. No time offset search.
Full commands and mean/median/max/runtime/memory are in artifacts.

| Dataset | ATE RMSE m | Frames | Matched | Status |
|---|---:|---:|---:|---|
| eee_01 | 0.118875639282 | 3981 | 3329 | Historical baseline agrees |
| Shield1 | 168.191541666 | 5418 | 1388 | Severe geometry drift |
| TunnelD | 113.919295548 | 1179 | 1179 | Severe geometry drift |

Historical local pristine NTU result is 0.118875639 m, 3981 frames and 3329
matches at the same upstream source SHA; agreement is below 1e-9 m. Independent
parallel and serial runs gave identical NTU ATE and stress metrics. Initial
parallel timings are excluded from official overhead comparisons.

TunnelD has one repeated exported propagation timestamp. A 135.75 ms IMU
gap near 1691509856.91–1691509857.05 spans scan updates. Upstream ESKF state
time does not advance when no usable IMU arrives. All 1179 rows are preserved;
stress evaluator permits nondecreasing timestamps and reports duplicates.
Negative-time trajectories still fail. This is an inherited limitation, not
hidden resampling or frame removal. No ESKF/geometry patch was applied.

P0 photo counts/information are identically zero (no photometric module).
Peak RSS is captured via getrusage; PSS unavailable. Timing is recorded by
upstream timers. Full trajectories and logs remain ignored under `runtime/`.
Next authorized step: bounded current cubemap/IGM side channel and mandatory
projection/residual finite-difference tests, followed by one conservative A/B
configuration. P0 does not claim successful geometry-only stress localization.
