# Executed P4 commands and outcomes

Working directory: `/home/lc/cube_lio/src/cube_lio` unless stated otherwise.
Environment for catkin/replay:

```bash
source /opt/ros/noetic/setup.bash
source /home/lc/ws_livox/devel/setup.bash
# Replay additionally uses:
source devel/setup.bash
```

Recovery:

```bash
git clone https://github.com/Scar-c/cube_lio.git /home/lc/cube_lio/src/cube_lio
git fetch --all --tags
git show --no-patch --format=fuller f3060768abee4d2ee2343b07edf62346c7cd0f87
git switch -c p4-faithful-cube-paper-reproduction f3060768abee4d2ee2343b07edf62346c7cd0f87
git clone https://github.com/ethz-asl/COIN-LIO.git refs/COIN-LIO
git -C refs/COIN-LIO checkout 76729cc4feb3649cbd79d28f82d9f62a2c82889b
```

The initial sandbox clone failed DNS resolution; network recovery then succeeded
with authorized escalation. Remote/branch/history and pre-implementation status
are archived. No existing historical remote branch was modified.

Paper extraction, complete reading before production edits:

```bash
pdftotext -layout '/home/lc/cube_lio/dis/MI -- 2026 - ICRA - Cubemap-Based LiDAR-Inertial Odometry with Intensity Assistance.pdf' /tmp/p4_cube_paper.txt
pdftotext -raw '/home/lc/cube_lio/dis/MI -- 2026 - ICRA - Cubemap-Based LiDAR-Inertial Odometry with Intensity Assistance.pdf' /tmp/p4_cube_paper_raw.txt
```

Builds (all successful):

```bash
catkin_make -DPYTHON_EXECUTABLE=/usr/bin/python3 -j2
catkin_make --pkg super_lio --make-args test_paper_cube -j2
catkin_make -j2
```

The first baseline replay name `p4_baseline_super_off` failed before estimator
start because the sandbox denied ROS XML-RPC server socket creation. A new output
name was used for the successful authorized replay outside that sandbox.

Successful sequential controls, default 32 TBB threads:

```bash
python3 tools/offline/run.py tunnel_d --name p4_control_super_off --bag /home/lc/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag --gt /home/lc/bag/ENWIDE/gt-tunnel_d.csv --audit-csv runtime/p4_control_super_off/time_audit.csv
python3 tools/offline/run.py tunnel_d --name p4_control_coin_g1 --bag /home/lc/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag --gt /home/lc/bag/ENWIDE/gt-tunnel_d.csv --coin --selector g1 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233 --audit-csv runtime/p4_control_coin_g1/time_audit.csv
```

Checks:

```bash
devel/lib/super_lio/test_cube_photo
devel/lib/super_lio/test_spherical_photo
devel/lib/super_lio/test_coin_feature_math
devel/lib/super_lio/test_ouster_time_support
devel/lib/super_lio/test_paper_cube
devel/lib/super_lio/test_paper_cube --jacobian runtime/p4_review/residual_jacobian.csv
devel/lib/super_lio/test_paper_cube --jacobian runtime/p4_review/residual_jacobian_repeat.csv
python3 tools/p4/minimal_gradient_gate.py
git diff --check
```

Legacy tests and P4 component tests exit 0. Both full-residual checks and the
minimal derivative gate exit 2 (FAIL). Repeat CSVs have identical SHA. Full
stdout, stderr and exit codes are recorded in `checks.json`; the component and
Jacobian JSONs are captured separately. Thresholds were fixed in parameter
provenance before testing. No CUBE accuracy, secondary sensor or ablation run
was executed after the failed Gate A.

This host has an older CTest: `ctest --test-dir build/super_lio` did not select
the directory and found no tests. The actual registered suite was then run with
working directory `/home/lc/cube_lio/src/cube_lio/build/super_lio`:

```bash
ctest --output-on-failure
```

Six tests executed; five pass, P4 actual residual Jacobian fails; suite exits 8.
The failure is preserved, not marked as an expected passing test.

Review bundle generation, after final commit:

```bash
python3 tools/p4/build_review_bundle.py
```

The bundle records target commit, full baseline patch, complete sources, recovered
configs, identities, full FD CSV, control trajectories and diagnostics, all
reports, build/check logs, and the supplied paper. It explicitly lists components
and experiments unavailable because of the mathematical stop condition.
