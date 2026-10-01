# CUBE-LIO intensity reproduction on Super-LIO

Staged P0/P1 study. GPLv3; upstream copyright notices are retained.
The ROS package remains `super_lio`. This Noetic host uses the official ROS1
branch; the ROS2 estimator and interface were inspected before that choice.
Photometric code is a separate cubemap/IGM observation, with bounded landmarks.

```bash
source /opt/ros/noetic/setup.bash
catkin_make -j4 -DCMAKE_BUILD_TYPE=Release
source devel/setup.bash
python3 tools/cube_lio/run.py eee_01 --name my_geo
python3 tools/cube_lio/run.py eee_01 --name my_photo --photo
```

Use `shield1` or `tunnel_d` for the other frozen datasets. Each run needs a
unique output name and a free local ROS master port (default 11431).
Runs read bags without modification; all raw outputs live in ignored `runtime/`.
Python dependencies: NumPy, PyYAML, ROS rosbag, evo 1.31.1. C++ dependencies:
ROS Noetic, Eigen, PCL, TBB, glog, gflags; P1 additionally uses OpenCV.

Reports: `spec/p0/P0_SUPER_LIO_BASELINE_REPORT.md` and
`spec/p1/P1_CUBE_PHOTOMETRIC_V0_REPORT.md`. Machine-readable summaries are under
`artifacts/`. User correction: ENWIDE TunnelD replaces Runway in the master prompt.
