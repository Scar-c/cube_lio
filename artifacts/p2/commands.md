# P2.1 frontend parity commands

Run from `/home/lc/cube_lio` with the pinned COIN checkout at `refs/COIN-LIO`, its isolated oracle build at `/tmp/cube_p2a_coin_oracle/devel`, and the frozen TunnelD bag available.

Build the production library and the standalone comparison tool:

```bash
catkin_make -j4 --pkg super_lio test_coin_frontend_parity
```

Start an isolated ROS master. `ROS_HOME` is under `/tmp` because the managed workspace does not permit writes to the default home ROS directory:

```bash
ROS_HOME=/tmp/ros-p2 ROS_LOG_DIR=/tmp/ros-p2/log roscore -p 11540
```

In another shell, load the exact oracle parameters and run the ten predefined scans (indices 0, 132, …, 1188):

```bash
export ROS_HOME=/tmp/ros-p2
export ROS_LOG_DIR=/tmp/ros-p2/log
export ROS_MASTER_URI=http://127.0.0.1:11540
export ROS_IP=127.0.0.1
rosparam load refs/COIN-LIO/config/params.yaml
rosparam load refs/COIN-LIO/config/line_removal.yaml
rosparam load refs/COIN-LIO/config/os_enwide.json
rosparam set image/u_shift 0
mkdir -p runtime/p2
./devel/lib/super_lio/test_coin_frontend_parity \
  /home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag \
  runtime/p2/frontend_frames.csv
python3 tools/p2/summarize_frontend_parity.py \
  runtime/p2/frontend_frames.csv artifacts/p2/reference_identity.json \
  artifacts/p2/frontend_parity.json
```

The parity executable compares both image frontends on the identical cloud produced by the pinned official COIN `Preprocess`. It uses identity scan-distortion transforms to isolate current-frame behavior. It exits nonzero if any owner/range/mask/u8/gradient/index mismatch occurs, if any compared float image differs by more than `1e-6`, or if sampled UV differs by more than `1e-12` pixels. The summarizer writes the compact machine-readable result and enforces ten frames at 128×1024.

The ten point clouds produce expected PCL warnings that their ROS message schema has no `ring` field; COIN preprocessing uses its configured Ouster organized-point fields and all ten frames completed. This is not a parity mismatch.

## P2.2 feature manager shadow

The shadow uses the frozen P2A trajectory and exact scan-end timestamps. COIN defines `lidar_end_time = PointCloud2.header.stamp + max(Ouster point.t)`. The 1185 oracle rows pair one-to-one with bag clouds at zero timestamp error; four sparse/unpaired cloud messages are interleaved, so cloud ordinal matching is not used. A temporary copy of the pinned oracle exports the official `T_Li_Lk`/`vec_idx`, final geometry translation rows, and global/LiDAR weak directions. The official checkout remains untouched.

The committed diagnostic patch can recreate those traces without changing `refs/COIN-LIO`:

```bash
cp -a refs/COIN-LIO /tmp/p2_coin_trace_src
git -C /tmp/p2_coin_trace_src apply --unidiff-zero "$PWD/spec/p2/COIN_TRACE_INSTRUMENTATION.patch"
mkdir -p /tmp/p2_coin_trace_ws/src
ln -sfn /tmp/p2_coin_trace_src /tmp/p2_coin_trace_ws/src/coin_lio
cd /tmp/p2_coin_trace_ws/src && catkin_init_workspace
cd /tmp/p2_coin_trace_ws && catkin_make -j2
# Start an isolated roscore on port 11541, then:
source /tmp/p2_coin_trace_ws/devel/setup.bash
rosparam set /p2/motion_trace_path /home/lc/cube_lio/runtime/p2/coin_motion_trace.bin
rosparam set /p2/weak_trace_path /home/lc/cube_lio/runtime/p2/coin_weak_trace.bin
rosparam set /p2/geometry_trace_path /home/lc/cube_lio/runtime/p2/coin_geometry_trace.bin
roslaunch coin_lio mapping_enwide.launch rviz:=false \
  bag_file:=/home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag
```

```bash
catkin_make -j4 --pkg super_lio
./devel/lib/super_lio/test_coin_feature_math
ROS_HOME=/tmp/ros-p2 ROS_LOG_DIR=/tmp/ros-p2/log \
ROS_MASTER_URI=http://127.0.0.1:11540 ROS_IP=127.0.0.1 \
  ./devel/lib/super_lio/test_coin_feature_parity \
  /home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag \
  runtime/p2a_coin_oracle/trajectory.tum \
  runtime/p2/coin_motion_trace.bin runtime/p2/coin_weak_trace.bin \
  runtime/p2/coin_geometry_trace.bin runtime/p2/feature_frames_real.csv \
  runtime/p2/jacobian_fd.csv \
  2>runtime/p2/feature_stderr.log
python3 tools/p2/summarize_feature_parity.py \
  runtime/p2/feature_frames_real.csv artifacts/p2/reference_identity.json \
  runtime/p2a_coin_oracle/trajectory.tum artifacts/p2/feature_parity.json \
  --repeat-csv runtime/p2/feature_frames_real_repeat.csv
```

The shadow reconstructs each scan-end point with the inverse of its official acquisition transform, then gives the original `T_Li_Lk` table and `vec_idx` to both managers for tracking. It compares complete projected-index maps, feature order/lifetime/world patches/reference intensities/current UVs, add/remove counts, and rejection/NCC statistics. The local weak-direction helper independently processes the captured official geometry rows and matches direction count and orientation on every frame. Two sequential 1185-frame feature CSV outputs are byte-identical with SHA256 `80583c838b78c73d406e2bfdec1160cf05da9b226d63602722fe3cee3571855c`.

## P2.3 photometric Jacobian audit

The same run compares local `round=true` projection/residual/H rows to the pinned source formula, then finite-differences the actual bilinear residual with fixed acquisition-point correspondence. Samples use TunnelD frames every 23 poses, first/middle/last active features, patch points `[0,12,24]`, and exclude UVs within 0.04 pixels of integer boundaries. The tool writes CSVs before returning exit status 3 when the P2.3 finite-difference gate fails.

```bash
python3 tools/p2/summarize_jacobian_tests.py \
  runtime/p2/jacobian_fd.csv artifacts/p2/jacobian_tests.json
```

The current source-formula row matches COIN to `1.82e-12`, while finite differences fail by a wide margin. See the JSON and final report before attempting P2.4.

The feature-parity executable writes the audit CSV before returning exit status 3 for this hard gate; this is the recorded P2.3 result, not a harness crash. The summarizer above separately returns 1 when the compact FD gate is blocked.

## P2 closure checks

The existing P1/P2A regression tests and local COIN math test were rerun:

```bash
./devel/lib/super_lio/test_coin_feature_math
./devel/lib/super_lio/test_cube_photo
./devel/lib/super_lio/test_information_budget
```

All three return success. The COIN frontend parity and complete TunnelD feature-shadow runs are recorded in `frontend_parity.json` and `feature_parity.json`; their large CSV/debug outputs stay under ignored `runtime/p2/`. No P2.4 fusion or C2 TunnelD run was performed because the P2.3 finite-difference gate failed.

Closure artifacts:

- `spec/p2/P2_FAITHFUL_COIN_ON_SUPER_REPORT.md`
- `artifacts/p2/fusion_equivalence.json`
- `artifacts/p2/tunneld_runs.json`
- `artifacts/p2/determinism.json`
- `artifacts/p2/metrics.json`
- `artifacts/p2/build_identity.json`
