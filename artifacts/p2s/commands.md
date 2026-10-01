# P2-S commands and run ledger

The P2-S worktree starts at `9e03a7c48cd7631a7c5f367d36d012b6730b038c` on `p2s-geometry-degeneracy-interface-audit`. Run commands from `/home/lc/cube_lio` in the ROS/catkin environment. The TunnelD bag, GT, evaluator, official COIN source and configuration hashes are recorded in `reference_identity.json`.

The official row trace was reused from P2: `runtime/p2/coin_geometry_trace.bin` and timestamp pairing `runtime/p2/feature_frames_real.csv`. This reproduces the pinned rule without rerunning or changing official COIN. P2-R's corrected G1 remains the fixed geometry-only trajectory.

Build and local checks:

```bash
catkin_make -j4 --pkg super_lio
devel/lib/super_lio/test_coin_feature_math
ctest --output-on-failure    # run from /home/lc/cube_lio/build
python3 -m py_compile tools/p2r/run.py tools/p2s/analyze_geometry.py tools/p2s/analyze_selectors.py tools/p2s/archive_results.py
git diff --check
```

The following shadow runs keep COIN rows out of the ESKF. Each `runtime` folder stores the image/feature diagnostics; S0 additionally exports the final Super translation rows and residuals. All four trajectories were checked byte-for-byte against P2-R G1.

```bash
python3 tools/p2r/run.py tunnel_d --name p2s_shadow_s0_final --port 11577 --shadow --selector original --audit-csv runtime/p2s_shadow_s0_final/time_audit.csv --geometry-rows runtime/p2s_shadow_s0_final/geometry_rows.bin
python3 tools/p2r/run.py tunnel_d --name p2s_shadow_gradient_final --port 11578 --shadow --selector gradient --audit-csv runtime/p2s_shadow_gradient_final/time_audit.csv
python3 tools/p2r/run.py tunnel_d --name p2s_shadow_weakest_final --port 11579 --shadow --selector weakest --audit-csv runtime/p2s_shadow_weakest_final/time_audit.csv
python3 tools/p2r/run.py tunnel_d --name p2s_shadow_normalized_final --port 11580 --shadow --selector normalized --audit-csv runtime/p2s_shadow_normalized_final/time_audit.csv
python3 tools/p2s/analyze_geometry.py --official-rows runtime/p2/coin_geometry_trace.bin --official-frames runtime/p2/feature_frames_real.csv --super-rows runtime/p2s_shadow_s0_final/geometry_rows.bin --output-dir artifacts/p2s
python3 tools/p2s/analyze_selectors.py
```

The normalized candidate was frozen at the median official `contribution/N_geo_rows` among official weak-direction samples, **0.01609598**, before any Super production selector result was inspected. Production used only this predeclared value.

Sequential production commands (S1 and S2 repeated because their first runs crossed the predeclared 10% improvement or 10 m gate; S3 did not):

```bash
python3 tools/p2r/run.py tunnel_d --name p2s_production_gradient --port 11581 --coin --selector gradient --audit-csv runtime/p2s_production_gradient/time_audit.csv --fusion-audit-json runtime/p2s_production_gradient/fusion_equivalence.json
python3 tools/p2r/run.py tunnel_d --name p2s_production_gradient_2 --port 11582 --coin --selector gradient --audit-csv runtime/p2s_production_gradient_2/time_audit.csv
python3 tools/p2r/run.py tunnel_d --name p2s_production_gradient_3 --port 11583 --coin --selector gradient --audit-csv runtime/p2s_production_gradient_3/time_audit.csv
python3 tools/p2r/run.py tunnel_d --name p2s_production_weakest --port 11584 --coin --selector weakest --audit-csv runtime/p2s_production_weakest/time_audit.csv
python3 tools/p2r/run.py tunnel_d --name p2s_production_weakest_2 --port 11585 --coin --selector weakest --audit-csv runtime/p2s_production_weakest_2/time_audit.csv
python3 tools/p2r/run.py tunnel_d --name p2s_production_weakest_3 --port 11586 --coin --selector weakest --audit-csv runtime/p2s_production_weakest_3/time_audit.csv
python3 tools/p2r/run.py tunnel_d --name p2s_production_normalized --port 11587 --coin --selector normalized --audit-csv runtime/p2s_production_normalized/time_audit.csv
python3 tools/p2s/archive_results.py
```

Each production run uses the unchanged evaluator to report ATE after the selector has run. The selector mode reads Super geometry rows and fixed COIN image candidates only; no GT input affects selection. Runtime bags, row dumps, trajectories and logs remain ignored. Compact hashes and metrics are archived under `artifacts/p2s/`; the two focused audits and final decision are in `spec/p2s/`.

ROS binds an isolated local XML-RPC master for each replay. The sandbox requires permission escalation for that local socket; runs used the user-authorized P2-S bag-replay scope. One initial shadow attempt was blocked by sandbox loopback restrictions before estimator startup; the successful final shadow replay is the one named above.
