# P1 commands and frozen experiment scope

User correction: ENWIDE TunnelD replaces Runway. Official defaults remain
`tools/cube_lio/config/photo.yaml`; geometry configs/evaluator are inherited
unchanged from P0. No ATE-driven parameter sweep or scaling changes.

```bash
source /opt/ros/noetic/setup.bash
catkin_make -j4 -DCMAKE_BUILD_TYPE=Release
source devel/setup.bash
devel/lib/super_lio/test_cube_photo
cd build/super_lio
ctest --output-on-failure
cd ../..
python3 tools/cube_lio/run.py eee_01 --name p1_final_geo_eee_01
python3 tools/cube_lio/run.py eee_01 --name p1_final_photo_eee_01 --photo
python3 tools/cube_lio/run.py shield1 --name p1_final_geo_shield1
python3 tools/cube_lio/run.py shield1 --name p1_final_photo_shield1 --photo
python3 tools/cube_lio/run.py tunnel_d --name p1_final_geo_tunnel_d
python3 tools/cube_lio/run.py tunnel_d --name p1_final_photo_tunnel_d --photo
python3 tools/cube_lio/run.py eee_01 --name p1_ablation_eee_idw_off --photo --idw-off
python3 tools/cube_lio/summarize.py
python3 eval/evaluate.py eee_01 runtime/p1_final_geo_eee_01
python3 eval/evaluate.py eee_01 runtime/p1_final_photo_eee_01
python3 eval/evaluate.py shield1 runtime/p1_final_geo_shield1
python3 eval/evaluate.py shield1 runtime/p1_final_photo_shield1
python3 eval/evaluate.py tunnel_d runtime/p1_final_geo_tunnel_d
python3 eval/evaluate.py tunnel_d runtime/p1_final_photo_tunnel_d

git diff --check
git status --short --branch
git diff --stat
git log --oneline --decorate -n 10
git push -u origin p1-cube-photo-v0
git ls-remote origin refs/heads/p0-super-lio-baseline refs/heads/p1-cube-photo-v0
```

The run tool isolates each ROS master and invokes the exact evaluator itself.
All official experiments above execute serially with 4 TBB threads. Initial
`p1_geo_*` / `p1_photo_*` runs were superseded after discovering the TunnelD
IMU-gap exception. Their partial logs stay in ignored runtime, not in metrics.
The fix disables photo constraints/births on a scan with no deskew history,
while preserving upstream geometry and every output row. No parameters changed.
The IDW-off NTU run is an ablation only, never a replacement official result.
