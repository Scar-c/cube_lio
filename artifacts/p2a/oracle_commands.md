# Official COIN executable oracle commands

Build (source checkout remained unmodified):

```bash
mkdir -p /tmp/cube_p2a_coin_oracle/src
ln -s /home/lc/cube_lio/refs/COIN-LIO /tmp/cube_p2a_coin_oracle/src/coin_lio
source /opt/ros/noetic/setup.bash
catkin_make -C /tmp/cube_p2a_coin_oracle -j2 -DCMAKE_BUILD_TYPE=Release -DPYTHON_EXECUTABLE=/usr/bin/python3
source /tmp/cube_p2a_coin_oracle/devel/setup.bash
```

Runtime: isolated `ROS_MASTER_URI=http://127.0.0.1:11538`, ROS_HOME/log paths
under ignored `runtime/p2a_coin_oracle`; a rospy subscriber to `/Odometry`
records message header timestamp and IMU body position/quaternion with `.17g`.
The external recorder does not modify estimator measurements or parameters.

```bash
roscore -p 11538
roslaunch coin_lio mapping_enwide.launch rviz:=false
rosbag play --clock --quiet --queue=1000 /home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag
python3 eval/evaluate.py tunnel_d runtime/p2a_coin_oracle
```

The driver starts the master, waits 3 s, starts the odometry subscriber and
official launch, waits 3 s, plays the complete bag at 1x, waits 3 s after player
completion, and SIGINTs/waits for its own process groups. GNU `/usr/bin/time -v`
wraps the driver. Thus wall time includes setup/playback/drain/shutdown;
maximum RSS is GNU time's maximum child RSS, not a sum of process RSS.
The initial sandbox attempt could not bind ROS sockets and yielded no trajectory;
the authorized outside-sandbox retry completed. No failed attempt is included
in the successful oracle metrics.

Raw logs, trajectory, recorder and catkin build stay ignored in
`runtime/p2a_coin_oracle` and `/tmp/cube_p2a_coin_oracle`.
