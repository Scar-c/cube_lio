# P2-T command ledger

The branch starts from P2-S SHA `5b28b14fab1785c990bf8793eaa4eef635dc72cd`. Run from `/home/lc/cube_lio` in the configured ROS/catkin environment. Runtime bags, trajectories, row traces and logs remain ignored; compact evidence is archived under `artifacts/p2t/`.

Build and tests:

```bash
catkin_make -j4 --pkg super_lio
devel/lib/super_lio/test_coin_feature_math
ctest --output-on-failure    # from /home/lc/cube_lio/build
python3 -m py_compile tools/p2r/run.py tools/p2t/analyze_degeneracy.py tools/p2t/archive_results.py
git diff --check
```

Collect photo-off final geometry rows for TunnelD, NTU eee_01 and Shield1, then capture the S2 production geometry trace:

```bash
python3 tools/p2r/run.py tunnel_d --name p2t_geometry_tunnel_d --port 11590 --audit-csv runtime/p2t_geometry_tunnel_d/time_audit.csv --geometry-rows runtime/p2t_geometry_tunnel_d/geometry_rows.bin
python3 tools/p2r/run.py eee_01 --name p2t_geometry_eee_01 --port 11591 --geometry-rows runtime/p2t_geometry_eee_01/geometry_rows.bin
python3 tools/p2r/run.py shield1 --name p2t_geometry_shield1 --port 11592 --geometry-rows runtime/p2t_geometry_shield1/geometry_rows.bin
python3 tools/p2r/run.py tunnel_d --name p2t_s2_reference --port 11593 --coin --selector s2 --audit-csv runtime/p2t_s2_reference/time_audit.csv --geometry-rows runtime/p2t_s2_reference/geometry_rows.bin
python3 tools/p2t/analyze_degeneracy.py --tunnel-geometry runtime/p2t_geometry_tunnel_d/geometry_rows.bin --ntu-geometry runtime/p2t_geometry_eee_01/geometry_rows.bin --shield-geometry runtime/p2t_geometry_shield1/geometry_rows.bin --s2-geometry runtime/p2t_s2_reference/geometry_rows.bin --output-dir artifacts/p2t
```

The analyzer freezes G1/G2 from the NTU geometry-only confidence distribution before any G1/G2 production ATE was inspected. Exact resulting thresholds: G1 `0.31913064578672057` (q95), G2 `0.359189249724233` (q99).

Production selector runs and repeats:

```bash
python3 tools/p2r/run.py tunnel_d --name p2t_s2_repeat2 --port 11594 --coin --selector s2 --audit-csv runtime/p2t_s2_repeat2/time_audit.csv
python3 tools/p2r/run.py tunnel_d --name p2t_s2_repeat3 --port 11595 --coin --selector s2 --audit-csv runtime/p2t_s2_repeat3/time_audit.csv
python3 tools/p2r/run.py tunnel_d --name p2t_g1_first --port 11596 --coin --selector g1 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233 --audit-csv runtime/p2t_g1_first/time_audit.csv --fusion-audit-json runtime/p2t_g1_first/fusion_equivalence.json
python3 tools/p2r/run.py tunnel_d --name p2t_g2_first --port 11597 --coin --selector g2 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233 --audit-csv runtime/p2t_g2_first/time_audit.csv --fusion-audit-json runtime/p2t_g2_first/fusion_equivalence.json
python3 tools/p2r/run.py tunnel_d --name p2t_g1_repeat2 --port 11598 --coin --selector g1 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233 --audit-csv runtime/p2t_g1_repeat2/time_audit.csv
python3 tools/p2r/run.py tunnel_d --name p2t_g1_repeat3 --port 11599 --coin --selector g1 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233 --audit-csv runtime/p2t_g1_repeat3/time_audit.csv
```

NTU and Shield1 photo-off controls (COIN remains disabled on these datasets):

```bash
python3 tools/p2r/run.py eee_01 --name p2t_ntu_validation --port 11600 --selector g1 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233
python3 tools/p2r/run.py eee_01 --name p2t_ntu_validation_repeat2 --port 11601 --selector g1 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233
python3 tools/p2r/run.py eee_01 --name p2t_ntu_validation_repeat3 --port 11602 --selector g1 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233
python3 tools/p2r/run.py eee_01 --name p2t_ntu_original_control --port 11605
python3 tools/p2r/run.py tunnel_d --name p2t_g2_repeat2 --port 11603 --coin --selector g2 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233 --audit-csv runtime/p2t_g2_repeat2/time_audit.csv
python3 tools/p2r/run.py tunnel_d --name p2t_g2_repeat3 --port 11604 --coin --selector g2 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233 --audit-csv runtime/p2t_g2_repeat3/time_audit.csv
python3 tools/p2r/run.py shield1 --name p2t_shield_validation --port 11606 --selector g1 --gate-g1-threshold 0.31913064578672057 --gate-g2-threshold 0.359189249724233
python3 tools/p2t/archive_results.py
```

The local ROS master requires loopback socket access for bag replay. Each final replay above completed successfully. GT was used only by the existing evaluator after a run; no GT value selected either gate threshold. No projection ablation or Cubemap run was started.
