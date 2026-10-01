# Offline infrastructure integration commands

Baseline: branch `p2t-super-native-degeneracy-gate`, SHA
`4c940821235da858c788b6b42861e525b765bf0b`.

The host has 32 online logical CPUs. The tested eee_01 bag was
`/home/lc/super_livo/bag/NTU/eee_01/eee_01.bag` (SHA-256 is recorded in each
run's ignored `runtime/.../identity.json`). ROS Noetic and the workspace
environment were already available in the shell. ROS master creation requires
local loopback networking.

```bash
catkin_make --pkg super_lio --make-args cube_offline_node -j2
python3 -m py_compile tools/offline/run.py tools/offline/validate.py tools/offline/compare_reference.py \
  tools/p2r/run.py tools/cube_lio/run.py
tools/offline/run_experiment.sh --help
python3 tools/p2r/run.py --help
python3 tools/cube_lio/run.py --help
```

The parity and benchmark runs were full-bag replays with the same config and
binary. Each run wrote its Ouster time audit into its own output directory.

```bash
tools/offline/run_experiment.sh eee_01 \
  --name offline_infra_eee01_threads1 --port 11630 --threads 1 \
  --audit-csv runtime/offline_infra_eee01_threads1/time_audit.csv

tools/offline/run_experiment.sh eee_01 \
  --name offline_infra_eee01_threads32 --port 11631 --threads 32 \
  --audit-csv runtime/offline_infra_eee01_threads32/time_audit.csv

python3 tools/offline/validate.py \
  --threads-1 offline_infra_eee01_threads1 \
  --threads-32 offline_infra_eee01_threads32
```

The validator writes `parity.json` first and only writes `benchmark.json` when
all byte-level parity checks pass. `wall_processing_s` and CPU usage come from
the same replay loop interval in `run.json`; 32T logical CPU utilization is
`CPU seconds / (wall seconds * 32) * 100`. The 1T and 32T timings are single
measurements, not a multi-run benchmark distribution.

One COIN-enabled G1 run was also replayed at 32T and compared against the saved
P2-T G1 production reference (`p2t_g1_repeat3`, recorded at 4T):

```bash
tools/offline/run_experiment.sh tunnel_d \
  --name offline_infra_tunneld_g1_threads32 --port 11632 --threads 32 \
  --coin --selector g1 \
  --gate-g1-threshold 0.31913064578672057 \
  --gate-g2-threshold 0.359189249724233 \
  --audit-csv runtime/offline_infra_tunneld_g1_threads32/time_audit.csv

python3 tools/offline/compare_reference.py \
  --reference p2t_g1_repeat3 \
  --candidate offline_infra_tunneld_g1_threads32 \
  --output g1_cross_thread_parity.json
```

This G1 run is an additional parity check, not part of the eee_01 speedup
measurement. The old and new run metrics serialize bag duration at different
precisions; the comparison normalizes that field to six significant digits
and compares evaluator metrics and trajectory/diagnostic artifacts as raw
bytes.
