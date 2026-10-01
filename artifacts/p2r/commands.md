# P2-R reproduction commands and run identity

Run from `/home/lc/cube_lio` with the pinned bag and ROS environment. The branch starts at `b493d593cb56bd07adb255779aaaf20f551b3621`. The R0 audit was run first on the historical code and committed as `41d2a09`; do not regenerate R0 with the corrected executable. The archived `time_semantics_before.csv/json` and G0 trajectory hash preserve that historical result.

Build and unit/regression tests for the corrected source:

```bash
catkin_make -j4 --pkg super_lio
ctest --test-dir build --output-on-failure
devel/lib/super_lio/test_coin_feature_math
python3 -m py_compile tools/p2r/run.py tools/p2r/summarize_time.py
git diff --check
```

Each runner command creates a new, isolated `runtime/<name>` directory and returns `SUCCESS` only when the estimator and evaluator complete. The final G1 and C1 runs were performed sequentially, with four TBB threads and photo/CUBE off. Use fresh `--name` values when reproducing; the commands below show the original names and paths.

```bash
python3 tools/p2r/run.py tunnel_d --name p2r_g1_rebased_history --port 11568 --audit-csv runtime/p2r_g1_rebased_history/time_audit.csv
python3 tools/p2r/summarize_time.py runtime/p2r_g1_rebased_history/time_audit.csv runtime/p2r_g1_rebased_history artifacts/p2r/time_semantics_after.json --phase R1_CORRECTED_G1
python3 tools/p2r/run.py eee_01 --name p2r_eee01_photo_off --port 11569
python3 tools/p2r/run.py shield1 --name p2r_shield1_photo_off_scoped --port 11569
python3 tools/p2r/run.py tunnel_d --name p2r_c1_rebased_1 --port 11569 --coin --audit-csv runtime/p2r_c1_rebased_1/time_audit.csv --fusion-audit-json runtime/p2r_c1_rebased_1/fusion_equivalence.json
python3 tools/p2r/run.py tunnel_d --name p2r_c1_rebased_2 --port 11569 --coin --audit-csv runtime/p2r_c1_rebased_2/time_audit.csv
python3 tools/p2r/run.py tunnel_d --name p2r_c1_rebased_3 --port 11569 --coin --audit-csv runtime/p2r_c1_rebased_3/time_audit.csv
```

The G1 audit CSV was copied to `artifacts/p2r/time_semantics_after.csv`, and the fixed-state C1 fusion audit to `artifacts/p2r/fusion_equivalence.json`. The compact JSON files were derived from the retained `runtime` trajectories, evaluation, timing, diagnostic and identity files; `c1_coin_runs.json` preserves the per-run summaries and timestamp vector, and `build_identity.json` pins the final binary, source, config, bag, GT and evaluator hashes. `runtime/` is intentionally ignored because it contains large logs and trajectories. `g1_geometry.json` and `c1_coin_runs.json` include their run directories and identities to connect the archived summaries to the retained local evidence.

No parameter sweep was performed. The first exploratory C1 runs before overlap-history rebasing were diagnostic failures of the time-support gate and are excluded from the three accepted validation runs. The final G1 was rerun after that repair, before C1 validation. NTU `eee_01` is Ouster, so its trajectory legitimately changes under an Ouster-only timing fix; Shield1 is Livox and its final photo-off trajectory is byte-identical to the historical baseline.
