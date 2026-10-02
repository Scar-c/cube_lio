# P3-A commands

The commands below record the actual original execution. For a fresh replay,
first create a new native G1 baseline with the frozen runner, then invoke
`python3 tools/p3a/validate.py --baseline-run NEW_BASELINE --prefix NEW_PREFIX --output-dir runtime/NEW_ARCHIVE`.
The output directory and all new runtime names must not already exist.

All replays use the frozen offline runner and 32T.
The explicit environment switch is recorded separately because the frozen runner does not record it.
Source and binary hashes are in `source_manifest.json` and each result. No configurations are edited.

```bash
git switch -c p3a-tunneld-cubemap-validation
tools/offline/run_experiment.sh tunnel_d --name p3a_r0_coin_g1_run1 --port 11740 --threads 32 --coin --selector g1 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233 --fusion-audit-json runtime/p3a_r0_coin_g1_run1/fusion.json
source /opt/ros/noetic/setup.bash
source devel/setup.bash
catkin_make --pkg super_lio --make-args cube_offline_node test_coin_feature_math test_cube_photo -j2
devel/lib/super_lio/test_coin_feature_math
devel/lib/super_lio/test_cube_photo
python3 tools/p3a/validate.py
```

```bash
CUBE_P3A_REPRESENTATION=coin tools/offline/run_experiment.sh tunnel_d --name p3a_r0_g1_run2 --port 11745 --threads 32 --coin --selector g1 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233 --fusion-audit-json runtime/p3a_r0_g1_run2/fusion.json
```

```bash
CUBE_P3A_REPRESENTATION=coin tools/offline/run_experiment.sh tunnel_d --name p3a_r0_g1_run3 --port 11746 --threads 32 --coin --selector g1 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233 --fusion-audit-json runtime/p3a_r0_g1_run3/fusion.json
```

```bash
CUBE_P3A_REPRESENTATION=cube_raw_no_idw tools/offline/run_experiment.sh tunnel_d --name p3a_r1_g1_run1 --port 11741 --threads 32 --coin --selector g1 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233 --fusion-audit-json runtime/p3a_r1_g1_run1/fusion.json
```

```bash
CUBE_P3A_REPRESENTATION=cube_raw_idw tools/offline/run_experiment.sh tunnel_d --name p3a_r2_g1_run1 --port 11742 --threads 32 --coin --selector g1 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233 --fusion-audit-json runtime/p3a_r2_g1_run1/fusion.json
```

```bash
CUBE_P3A_REPRESENTATION=cube_igm_idw tools/offline/run_experiment.sh tunnel_d --name p3a_r3_g1_run1 --port 11743 --threads 32 --coin --selector g1 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233 --fusion-audit-json runtime/p3a_r3_g1_run1/fusion.json
```

```bash
CUBE_P3A_REPRESENTATION=cube_igm_idw tools/offline/run_experiment.sh tunnel_d --name p3a_r3_gradient_run1 --port 11744 --threads 32 --coin --selector gradient --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233 --fusion-audit-json runtime/p3a_r3_gradient_run1/fusion.json
```
