# Super-LIO offline implementation audit

## Source inspected

The local checkout is `/home/lc/prob_lio/src/Super-LIO`, currently at
`60b57aaac8dc397f80c56364e7ccb008c300cc29`. Its active source tree does not
contain the offline reader; the local build directory still has stale offline
target metadata. The offline implementation was therefore audited from the
workspace's P0 baseline commit `63f97ea` and its tracked files, rather than
treating generated build files as source. The P0 runner entry was
`tools/prob_lio/run_baseline.sh --offline`, which launched
`super_lio_offline_node` after loading the ROS config and setting the bag and
output directory.

## Replay path and time semantics

`src/super_lio/offline/OfflineReader.{h,cpp}` is a transport adapter. It opens a
ROS1 bag, queries the configured LiDAR and IMU topics, and dispatches messages
in rosbag record order. Optional crop offsets are based on bag record time;
the reader does not rewrite message stamps, reorder sensor messages, or build
estimator measurement groups. Sensor time remains each message's
`header.stamp`. The offline node calls the production `ROSWrapper` callbacks
and `SuperLIO::process()` after each message, then performs a bounded five-step
EOF drain. It uses in-process ROS publishers/subscribers, avoiding paced
`rosbag play` and external recording during offline runs.

## TBB sections and thread controls

The audited `super_lio.cpp` contains TBB parallel loops for point transforms
during map initialization, scan undistortion, and geometry correspondence /
Jacobian accumulation. The geometry loop uses
`tbb::enumerable_thread_specific<ThreadACC>` for per-worker Hessian and
gradient sums, then merges the worker-local accumulators. Livox point
conversion in `ROSWrapper.cpp` also uses `std::execution::par_unseq`.

The historical offline node does not set a TBB thread limit with
`task_scheduler_init` or another explicit scheduler control; concurrency is
left to the TBB runtime defaults. The P0 host metadata reports 32 logical CPUs.
The CUBE-LIO integration now exposes an explicit positive thread count through
`--threads`, passes it to the offline node's TBB scheduler, and defaults to 32.

## Determinism and comparison controls

The reference replay's main determinism controls are direct bag iteration,
stable topic selection, one estimator step per message arrival, authoritative
sensor header stamps, and no wall-clock playback pacing. The P0 comparison
utility `eval/prob_lio/extract_and_compare.py` matches timestamps rounded to
nanoseconds (with a `1e-6` second fallback) and checks position and quaternion
differences against a `1e-6` tolerance. The archived online/offline check
matched 294 poses and reported zero position difference and a maximum
quaternion difference of `2.98e-8` rad.

That reference checker is a numerical-tolerance comparison, not a bitwise
checksum gate. The audited P0 runner does not write SHA-256 checksums for
trajectories. The CUBE-LIO parity validator closes that gap by comparing raw
file bytes and recording SHA-256 values for trajectories, timing diagnostics,
and evaluator metrics; it separately extracts and hashes the timestamp
column.

## Trajectory serialization

The P0 offline node subscribes to `/lio/odom` in-process and writes
`trajectory.tum` with `std::setprecision(17)`. Each row is
`stamp tx ty tz qx qy qz qw`, using the odometry message header stamp and pose.
The format is standard TUM ordering with 17 significant digits, which is
sufficient to round-trip doubles. CUBE-LIO's replay adapter writes the same
TUM column order and precision from the estimator state timestamp and pose.

## Integration implications

CUBE-LIO keeps its own replay adapter: `rosbag::View` selects the configured
LiDAR/IMU topics, messages go directly through `ROSWrapper::replay()`, and the
production estimator is stepped synchronously. This retains the reference's
transport-only replay and message-stamp semantics without copying its
publisher/subscriber bridge. Thread count, output identity, timing diagnostics,
and byte-level parity are controlled by the unified CUBE-LIO runner.
