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
