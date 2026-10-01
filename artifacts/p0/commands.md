# P0 commands

Initial repository: no valid HEAD or remotes; remote was empty.
`git init`, `git symbolic-ref HEAD refs/heads/p0-super-lio-baseline`,
`git remote add origin https://github.com/Scar-c/cube_lio.git`.

```bash
git clone --branch ros1 --single-branch https://github.com/Liansheng-Wang/Super-LIO.git refs/Super-LIO-ros1
git clone --branch ros2 --single-branch https://github.com/Liansheng-Wang/Super-LIO.git refs/Super-LIO-ros2
git clone --branch main --single-branch https://github.com/ethz-asl/COIN-LIO.git refs/COIN-LIO
git clone https://github.com/PengYu-Team/GEODE_dataset.git refs/GEODE_dataset
source /opt/ros/noetic/setup.bash
catkin_make -j4 -DCMAKE_BUILD_TYPE=Release
source devel/setup.bash
python3 tools/cube_lio/run.py eee_01 --name p0_serial_eee_01
python3 tools/cube_lio/run.py shield1 --name p0_serial_shield1
python3 tools/cube_lio/run.py tunnel_d --name p0_serial_tunnel_d
python3 eval/evaluate.py eee_01 runtime/p0_serial_eee_01
python3 eval/evaluate.py shield1 runtime/p0_serial_shield1
python3 eval/evaluate.py tunnel_d runtime/p0_serial_tunnel_d
git diff --check
git status --short --branch
git diff --stat
git log --oneline --decorate -n 10
git push -u origin p0-super-lio-baseline
git ls-remote origin refs/heads/p0-super-lio-baseline
```

Initial parallel discovery runs are `runtime/p0_*`; official timing is from
the serial runs above. Baseline source hashes and frozen evaluator identities
are captured in `metrics.json`. The source tree was clean before the first build.
