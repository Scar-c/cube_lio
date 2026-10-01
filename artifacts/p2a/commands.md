# P2A command and identity ledger

All estimator runs used the same Release binary from production code commit
`cd8f8e3c4f10f2691f49d2041faac64eab94f373`. Later commits contain analysis/report
files only. ROS local sockets required the authorized outside-sandbox runner;
dataset files and external repositories were never modified.

## Starting point and build

```bash
git switch -c p2a-photo-information-consistency 8db5c84281c35e848f2212fc482b60bf4e2a694b
source /opt/ros/noetic/setup.bash
catkin_make -j4 -DCMAKE_BUILD_TYPE=Release
source devel/setup.bash
(cd build/super_lio && ctest --output-on-failure)
devel/lib/super_lio/test_cube_photo > artifacts/p2a/jacobian_tests.json
devel/lib/super_lio/test_information_budget > artifacts/p2a/policy_tests.json
```

Build log: ignored `/tmp/cube_p2a_build.log`. Both CTest targets passed.
Binary/compiler/package/source/frozen-file fingerprints:
[build_identity.json](build_identity.json).

## Reference oracles

Exact official build/run/recording/evaluation protocol is in
[oracle_commands.md](oracle_commands.md). Official source remained clean.
BIEVR archive was read-only and evaluated through the frozen P1 convention;
no BIEVR estimator was rerun. See [REFERENCE_AUDIT.md](../../spec/p2a/REFERENCE_AUDIT.md).

## Frozen P1 shadow, then information-only gate

```bash
source /opt/ros/noetic/setup.bash
source devel/setup.bash
python3 tools/cube_lio/run.py eee_01 --name p2a_shadow_eee_01 --photo --audit
python3 tools/cube_lio/run.py tunnel_d --name p2a_shadow_tunnel_d --photo --audit
python3 tools/cube_lio/run.py shield1 --name p2a_shadow_shield1 --photo --audit
python3 tools/cube_lio/p2a_shadow.py
```

The shadow summary/gate code never reads evaluation.json, GT, or ATE. It tests
all frame/iteration policy rows. Substantial reduction is operationalized as
at least 50% reduction of TunnelD weakest-geometry-decile median authority.
All three candidates passed before the actual matrix began. Gate JSON and
shadow CSV hashes are retained; full iteration CSVs stay ignored in runtime.

## Single-use, serial actual matrix

```bash
source /opt/ros/noetic/setup.bash
source devel/setup.bash
python3 tools/cube_lio/p2a_matrix.py
python3 tools/cube_lio/p2a_summarize.py
```

[matrix_plan.json](matrix_plan.json) records every exact argv, in execution
order: NTU → TunnelD → Shield1. Each dataset starts with one photo-off regression;
policy order is C0 → C60 → C100 → K100. NTU and Shield1 have one run per policy;
TunnelD has exactly two consecutive runs per policy. No matrix rerun, parameter
change, or selection based on observed ATE took place. The script refuses a
second invocation once the plan exists and stops if repeats differ materially.
Information audit is disabled for actual-run timing. Frozen per-frame P1 photo
diagnostics remain enabled. No reference estimator, build, or other benchmark
ran concurrently; normal desktop/browser services remained present.

The run.py runner creates an isolated master on port 11431, loads the unchanged
dataset/photo YAMLs, sets photo enable/policy/audit explicitly, executes the
offline estimator with four TBB workers, then invokes exactly:

```bash
python3 eval/evaluate.py DATASET runtime/RUN_NAME
```

Trajectories, ROS logs, node logs, run/identity/evaluation JSONs, and full photo
CSVs remain in ignored `runtime/RUN_NAME`. Compact checked-in artifacts record
trajectory hashes, frames/timestamps, paired maximum pose differences, ATE,
timing/RSS, support/authority summaries, and test results.

## Closure

```bash
git diff --check
git add tools/cube_lio/p2a_*.py artifacts/p2a spec/p2a
git commit -m 'Archive P2A information gates, deterministic ablations and verdict'
git push -u origin p2a-photo-information-consistency
git ls-remote origin refs/heads/p2a-photo-information-consistency refs/heads/p0-super-lio-baseline refs/heads/p1-cube-photo-v0
git status --porcelain
```

Final branch SHA is printed by the closing response; build identity records
the experimental source commit rather than a self-referential report SHA.
P0/P1 refs must remain at their frozen SHAs. Stop for external review; no P2B.
