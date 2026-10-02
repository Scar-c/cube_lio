# P3-A TunnelD Cubemap validation report

结论：**NO-GO（本轮固定配置）**。在同一个 COIN patch/选择/融合通道中，R1–R3 均未达到 TunnelD ATE < 10 m。COIN G1 基线为 3.546209 m；最佳 Cubemap 为 R3（IDW+IGM），146.791259 m。当前结果不支持转入 Livox 泛化验证。

## 实验来源与范围

- 分支：`p3a-tunneld-cubemap-validation`；起点 `04a48588bcdc4a2210b189aaefe2cf3a29f48302`，包含 P3、offline32T、外部代码审计包、P3-R attribution audit。
- 原始任务：[P3A_PROMPT.md](P3A_PROMPT.md)。本轮未运行 Shield4；完成本报告与发布后停止。
- Bag：`/home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag`；GT：`/home/lc/algorithm_versa/bag/ENWIDE/gt-tunnel_d.tum`。SHA-256 在各结果 JSON 的 identity/evaluation 中。
- 全部 7 次 replay 均为 offline 32T，顺序执行，不使用在线 ROS replay。每次读取 1189 LiDAR / 11861 IMU，输出 1180 poses，匹配 1180 GT，估计轨迹跨度 118.014169931 s。
- 使用冻结 evaluator：nearest timestamp ≤0.1 s、原有 TunnelD lever arm、SE3 无 scale 对齐；所有实验具有相同首末时间戳。既有 evaluator 记录 1 个 duplicate estimate timestamp，各组相同。
- Phase 0 在干净起点运行；其后两次 R0 在接入新表示后的同一构建中运行。dirty source 由 `source_manifest.json` 的逐文件 SHA 和各 run 的 binary/library SHA 精确绑定，不能仅凭 source_head 识别实验代码。

## 为什么重新建立共用观测通道

P3 的 COIN 与 PhotoObservation 是两套不同的特征管理和融合路径；后者有 1200 点特征、sigma normalization/Huber weighting 等独立设置，不能将两条路径的 ATE 差异只归因于表示。本轮全部使用 `--coin`，不启用 `--photo`。R0 使用既有 calibrated Ouster intensity 图；R1–R3 将既有 CubeImage 导出到同一个 COIN patch-image contract。

原生 COIN 的 intensity scale、line removal、brightness normalization、blur、255 截断等预处理在所有组共同执行。R1–R3 的输入强度取对应 acquisition pixel 的预处理结果，坐标取同一支持链已 deskew 的 scan-end 点；R0 仍保留原生 calibrated acquisition-pixel 及 matching 语义。因此本轮的 “raw” 指 raw-intensity measurement channel，相对于 IGM，并非未经预处理的传感器 DN。

Cubemap 部分使用现有六面 dominant-axis projection、最近深度 z-buffer、face-local IDW、Gaussian derivative magnitude。IDW 填充的像素以插值深度与该像素射线形成 intensity-only landmark；不进入 Super geometry cloud/map。Cubemap 图已在 scan-end，matching 返回同坐标并使用 identity transform，防止再次施加 acquisition transform。跨 face 不插值，COIN 的 patch erosion 与 margin 分别施加在每个 face。

这里验证的是受控的 CUBE-style representation 在共用 COIN 观测通道中的行为，不是整个原版 CUBE estimator 的复刻。旧 P3 PhotoObservation 结果不混入本轮消融表。

## 冻结组件与参数

`source_manifest.json` 验证 Super estimator/ESKF、geometry/map、IMU、ROS preprocessing、offline replay/node/runner、evaluator、dataset/photo YAML 和 SuperDegeneracyGate 均与起点相同。额外按函数文本比较：`CoinObservation::add`、`finish`（gate/selector）、`CoinFeatureManager::update/track/detect/detectComplementary` 全部字节一致；CoinPhotometricModel 未修改。

| 共同设置 | 值 |
|---|---|
| Phase 1 selector | 既有 COIN `g1`，Super-native gate |
| G1 / G2 threshold | 0.31913064578672057 / 0.359189249724233（G2 非本轮选择器） |
| feature cap / patch | 60 centers / 5×5，最多 1500 rows |
| max lifetime / NMS radius / margin | 25 / 10 / 10（Cubemap 每面同值） |
| gradient threshold / NCC threshold / range tracking | 16.5 / 0.7075 / 0.2 m |
| photo_scale / R | 0.00095 / 0.001 |
| information factor | photo_scale²/R = 0.0009025 |
| Cube resolution / IDW radius / k / min support | 96 / 3 / 6 / 3 |
| IDW power / range abs / range rel / Gaussian sigma | 2 / 0.3 m / 0.02 / 1 |

没有调 photo_scale/R、增加特征上限、修改 EKF weighting、geometry residual 或 gate。每组首个有效融合帧的现有 fusion audit 均 PASS，scale/R/factor 相同。该 audit 是首个有效帧的代数核对，不是逐帧 fusion proof；逐帧共用路径由未修改的 add 函数保证。

`control_validation.json` 还核验所有组每 scan 的 timestamp、IMU unsupported count、scan-end delta、USED/SKIPPED 状态和原因均与 R0 相同；每个 Cubemap frame 的支持点数量与 R0 相同。各组均有 4 个 intensity-skipped scans，几何继续更新。所有 feature/row 上限检查通过。

## Phase 0/1：表示消融

下表时间为 `run.json.wall_processing_s`，不包含 ROS master/config loading/GT evaluator；单次时间不是稳定加速比。active centers、valid patches 和 residual 取最终 add/finish diagnostics，均值包含零观测与跳过帧。pooled RMS = sqrt(Σ rows×scan_RMS² / Σ rows)。raw 与 IGM 的数值通道不同，RMS 大小不能直接表示信息质量。

| 组别 | 表示 | ATE RMSE (m) | replay wall (s) | 平均 active centers | 平均有效 patches | 平均 photo rows | pooled residual RMS |
|---|---|---:|---:|---:|---:|---:|---:|
| R0 | COIN raw | 3.546209445 | 34.908086 | 60.000000 | 48.257627 | 1206.440678 | 24.765936 |
| R1 | Cube raw / no IDW | 154.492187420 | 48.225995 | 46.961864 | 30.300847 | 757.521186 | 31.829301 |
| R2 | Cube raw + IDW | 170.410876117 | 46.120454 | 59.143220 | 38.755085 | 968.877119 | 32.336755 |
| R3 | Cube IGM + IDW | 146.791259008 | 44.731551 | 9.257627 | 5.419492 | 135.487288 | 14.629570 |

| 组别 | trajectory SHA-256 |
|---|---|
| R0 | `d71c1ab0b0e3cc542aed184fa9d1e9489b8a7880598a7403a2eb8c3df847bf63` |
| R1 | `908fa7649cbf8e011ade59ecefc520a5aada66a9222738e0cbb91701c11d2b69` |
| R2 | `20b4ee8932d02deb9abee0bd4df2f0f85a5b973f22a29aec43c38d85b0c2eff6` |
| R3 | `066861866faeef5bc826ec45d47b898308d002137f54aee20e714f59a9ed2f88` |

## Phase 2：选择器依赖

最佳 Cubemap 按 R1–R3 全包 ATE 最小选取 R3，没有参数搜索或重调。A 使用 G1 gate 下的 weakest-direction 路径；B 使用既有 `gradient` selector，纯 gradient 排序，feature cap/NCC/fusion 相同。B 可以继续计算 geometry 信号用于诊断，但选择结果不依赖弱方向或 gate 激活。

| R3 selector | ATE RMSE (m) | replay wall (s) | gate active scans |
|---|---:|---:|---:|
| G1 weakest | 146.791259008 | 44.731551 | 1033 |
| all-use (`gradient`) | 146.791259008 | 43.536254 | 0 |

两组 trajectory SHA 完全相同，但 COIN CSV 不同：1036 scans 的 selected-center **顺序**不同，每 scan 的 selected-center **集合**相同；累加顺序使部分 double diagnostics 有末位差异。R3 平均 active centers 9.257627，最多 22，始终低于 60 的上限，rank 未形成不同的 feature subset。这是失败轨迹下的 nonbinding-cap 情形，不构成在有效定位前提下减少选择器依赖的证据。

## Phase 3：确定性

唯一 ATE <10 m 的候选是 R0。按要求运行 3 次，三个 trajectory SHA、完整 evaluation JSON、完整 COIN diagnostic CSV 的 SHA 均一致。Phase 0 前与接入后的默认 COIN 路径也相同；并且匹配此前 `offline_infra_tunneld_g1_threads32` 的已保存 G1 trajectory SHA。详见 `determinism.json`、`baseline_parity.json`。

所有 Cubemap 候选以及 all-use 均 ≥10 m，所以按提示不增加三次重跑。R3 在两种选择器下同 SHA 是本轮观察，不替代三个独立相同配置的确定性检验。wall/CPU/RSS 等运行指标不要求逐次相同，确定性结论针对轨迹、评价和 COIN diagnostics。

## 五个问题与证据边界

1. **Cubemap 是否优于 COIN？** 否。最佳 R3 为 146.791259 m，COIN 为 3.546209 m，R1–R3 均不满足 <10 m。NO-GO 限定本轮 96-resolution、冻结 COIN patch/settings/fusion 的受控实现；不能据此证明所有 Cubemap 方法必然失败。
2. **IDW 是否有作用？** 有 representation/coverage 作用，但未带来本次定位收益。R1→R2 平均有效 patch 30.300847→38.755085、active centers 46.961864→59.143220；ATE 154.492187→170.410876 m，变差约 15.918689 m。闭环轨迹变化会反过来影响后续 raster/feature，累计像素计数不应当作相同 pose 下的离线图像检验。
3. **IGM 是否有作用？** R2→R3 ATE 改善约 23.619617 m，但仍失败。平均有效 patch 38.755085→5.419492、active centers 59.143220→9.257627，固定 gradient threshold 下观察到明显的特征不足。较小 residual RMS 属于不同通道的数值，不能据此声称 IGM 信息更强。
4. **是否减少 weakest-direction 依赖？** 未证明。all-use 同样失败，且由于 cap 不约束，同一批中心全部被两种排序接纳。
5. **是否准备好 Livox 验证？** 否。Ouster 上本轮 primary acceptance 尚未通过；不能把此前 Shield4 的失败唯一归因于 Livox 适配，也不能以本轮结果声称 Livox 泛化成功。本轮不修复、不运行 Shield4。

可见的诊断线索：R0/R1/R2/R3 每帧 photo information matrix Frobenius norm 平均约 7.858e6 / 1.063e6 / 1.395e6 / 6.767e4；IGM 有效 patch 明显不足。矩阵范数不是 weakest-direction information 或 observability 证明，这些线索没有完成唯一失败原因归因。96 像素每面、chart seam/margin、patch 存活、通道幅值与 frozen threshold 的适配均是本轮配置的限制；本阶段没有为达标做 resolution/weight/threshold 搜索。

## 代码定位与复审

| 模块 | 文件 / 函数 | 本轮变化 |
|---|---|---|
| Existing representation | `src/super_lio/include/intensity/intensity_representation.hpp`, `cube_image.hpp`, `src/intensity/cube_image.cpp` | 未改；CubeImage 真实构建六面/IDW/IGM |
| Representation bridge | `src/super_lio/include/intensity/coin/coin_cubemap_representation.hpp`, `src/intensity/coin/coin_cubemap_representation.cpp`; `p3aRepresentation`, `p3aCubeSettings`, `buildCoinCubemap` | 新增 opt-in factory、共用 intensity 输入、frame 导出、private IDW landmarks |
| Projector coordinate bridge | `coin_ouster_projector.hpp/.cpp`; constructor, `project`, `projectionJacobian` | cube_resolution=0 保留原 calibrated 路径；非零使用 CubeProjector，并给 face 添加固定 horizontal offset |
| Chart validity/matching | `coin_feature_manager.cpp`; constructor, `projectUndistorted` | 每 face 同 margin；scan-end matching 不二次 deskew；排序/跟踪/检测实现未改 |
| Frame lifecycle | `coin_observation.hpp/.cpp`; constructor, `prepare` | 构造表示，导出当前 frame 与 representation.csv；add/finish 未改 |
| Targeted checks | `src/super_lio/test/test_coin_feature_math.cpp` | packed projection finite difference、IDW landmark reprojection、边界、identity motion、共用 residual 入口 |
| Orchestration | `tools/p3a/validate.py` | 新增实验/归档脚本；调用原 offline runner，不复制或修改 replay/evaluator |

Opt-in 环境变量 `CUBE_P3A_REPRESENTATION` 有四个值：`coin`（unset 的默认）、`cube_raw_no_idw`、`cube_raw_idw`、`cube_igm_idw`。只有 COIN enabled 时读取。原 runner 完全冻结，因此其 identity 不记录该环境变量；新脚本在 result JSON 的 `representation_environment`、commands.md 和 Cubemap representation.csv 中单独记录。不要用 runner identity 中 `/photo` 的默认字段推断 COIN representation；本轮 `photo=false`。

Cubemap scalar residual/Jacobian 通过同一未改的 CoinPhotometricModel 计算：float bilinear intensity、既有 central-image-gradient、Cube projection Jacobian、既有 Super-state correction column/sign convention。这与旧 PhotoObservation 对 CubeImage bilinear interpolant 的 exact derivative 路径有区别，本轮保留共用 COIN residual model 以隔离表示。已有 CubeImage 的 IDW/IGM 算法未改。

针对性检查通过：packed cube Jacobian、filled landmark 与 pixel 一致、防止 double deskew、chart boundary mask、共用 COIN residual；既有 Cube photo 检查 6000 projection points、1120 residual points，最大绝对误差约 8.895e-9 / 3.785e-9。既有 NCC/selector/gate checks 也通过。检查记录在 `checks.json`。这些验证支持数学/接入路径正确性，不等价于 ATE 达标。

## 可复现材料

- [baseline_coin.json](../../artifacts/p3a/baseline_coin.json)：Phase 0 原始评价、SHA、运行指标和 diagnostics summary。
- [cubemap_results.json](../../artifacts/p3a/cubemap_results.json)：R0–R3 完整评价、identity、source/binary/library hashes、残差和特征计数。
- [selector_ablation.json](../../artifacts/p3a/selector_ablation.json)、[determinism.json](../../artifacts/p3a/determinism.json)。
- [source_manifest.json](../../artifacts/p3a/source_manifest.json)、[control_validation.json](../../artifacts/p3a/control_validation.json)、[baseline_parity.json](../../artifacts/p3a/baseline_parity.json)。
- [commands.md](../../artifacts/p3a/commands.md)：逐条实际 replay 命令与 opt-in 值。fresh run 需要新的 runtime name；本机当前 runtime 中保留原始 trajectory/evaluation/identity/run/fusion 文件。
- 需要复跑时先用冻结 runner 生成新的原生 G1 Phase 0 run，然后运行 `python3 tools/p3a/validate.py --baseline-run NEW_BASELINE --prefix NEW_PREFIX --output-dir runtime/NEW_ARCHIVE`。这些参数用于避免覆盖入库材料或既有 runtime；默认无参数的命令是本轮最初实际执行方式。CLI 路径选项是实验完成后的归档便利补充，未修改本轮生产构建。
- `artifacts/p3a/diagnostics/`：全部 7 次完整 COIN CSV、各 Cubemap 完整 representation CSV（无截断）。轨迹本体在 ignored runtime，不上传 bag/GT；SHA 和全部评价结果入库。
- 较早 CUBE/COIN 代码来源与 f3060768 完整源码快照见 [P3_CODE_AUDIT_MAP.md](../p3/P3_CODE_AUDIT_MAP.md)。本轮生产代码通过本分支 Git diff 复审，旧快照没有改动。

完成状态：实验和报告已完成，接受结论 NO-GO；commit/push 后由交付消息提供最终 SHA，并核验远端与 clean worktree。STOP。
