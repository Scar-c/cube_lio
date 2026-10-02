# P3-A efeb69bf 外部代码复审单文件材料

# efeb69bf 实际源码与 diff 审计材料

审查目标 commit：`efeb69bf6f4814b83cc8709eb06d792ffb47e41e`。父提交：`04a48588bcdc4a2210b189aaefe2cf3a29f48302`。

本材料直接从 Git commit tree 导出，完整保留文件字节，不取当前工作区，不截断 diff 或源码。
本次只补材料与内容一致性核验，不修改生产代码，不构建，不运行新实验，也不宣称已经完成独立逐文件正确性审查。

## 入口

- `COMMIT_FULL.patch`：实际 `git show --format=fuller --binary --full-index --no-ext-diff --no-renames efeb69bf6f4814b83cc8709eb06d792ffb47e41e`，全部 31 个文件，包括完整 JSON/CSV/doc diff。
- `CORE_CODE.diff`：同一 parent→target 的 9 个源码/测试/实验脚本 diff，不包含实验结果的大量 CSV。
- `COMMIT_FILE_LIST.md`：原始 stat/name-only/name-status 输出。
- `source/`：35 个 target 完整核心源码及依赖、runner/evaluator/config；其余 unchanged 文件仅用于调用链上下文。
- `parent_source/`：6 个实际修改且父提交已存在文件的完整旧版；新文件没有伪造的父版本。
- `evidence/`：commit 内原始 P3A JSON、commands、prompt 和报告。完整 CSV 在 full patch 中，也可由 target commit 获取。
- `EXPORT_MANIFEST.json`：每个源文件的 revision、Git blob、SHA-256、完整行数/大小；`EXPORT_VERIFICATION.json`：本轮字节/冻结函数核验。

网页入口：[commit 页面](https://github.com/Scar-c/cube_lio/commit/efeb69bf6f4814b83cc8709eb06d792ffb47e41e)；
[原始 commit patch](https://github.com/Scar-c/cube_lio/commit/efeb69bf6f4814b83cc8709eb06d792ffb47e41e.patch)。本地导出可避免网页折叠大 diff 或 reviewer 无法访问仓库的情况。

## 实际调用链与边界

```text
frozen offline runner --coin (photo=false)
  -> CoinObservation::prepare: existing support + deskew -> COIN preprocessed intensity
       R0: original calibrated acquisition-pixel CoinFrame
       R1/R2/R3: CubeImage::build -> buildCoinCubemap -> scan-end CoinFrame
  -> CoinObservation::add -> CoinPhotometricModel::linearize
       -> projectUndistorted + packed/canonical projection + shared residual/J
       -> unchanged photo_scale²/R fusion
  -> CoinObservation::finish -> unchanged g1 or gradient -> same feature manager
```

| 模块 | efeb69bf 源码定位 | 状态 / 输入输出 |
|---|---|---|
| 表示切换 / 参数 | [coin_cubemap_representation.cpp:10](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/src/super_lio/src/intensity/coin/coin_cubemap_representation.cpp#L10) | 新增：读取 opt-in 环境变量；cube 设置只按 R1/R2/R3 切换 IDW/IGM |
| 共同输入 / Cubemap 导出 | [coin_cubemap_representation.cpp:45](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/src/super_lio/src/intensity/coin/coin_cubemap_representation.cpp#L45) | 新增：COIN 预处理 intensity + 已支持/deskew 的 scan-end points → packed frame |
| IDW pixel 的 3D landmark | [coin_cubemap_representation.cpp:34](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/src/super_lio/src/intensity/coin/coin_cubemap_representation.cpp#L34) | 新增：插值 Euclidean range × unit ray；只存入 intensity 私有 points_ |
| 3D → 六面坐标 | [cube_projector.hpp:25](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/src/super_lio/include/intensity/cube_projector.hpp#L25) | 复用：dominant axis、face basis、ratio projection、analytic Jacobian、seam 标记 |
| 图像 storage / z-buffer | [cube_image.cpp:28](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/src/super_lio/src/intensity/cube_image.cpp#L28) | 复用：nearest range 胜出；raw pixel 保存原 point index |
| IDW | [cube_image.cpp:58](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/src/super_lio/src/intensity/cube_image.cpp#L58) | 复用：face-local raw support、k/min support、inverse-distance weights、depth discontinuity rejection |
| IGM | [cube_image.cpp:91](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/src/super_lio/src/intensity/cube_image.cpp#L91) | 复用：3×3 Gaussian + central differences → hypot；validity mask |
| packed projection | [coin_ouster_projector.cpp:66](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/src/super_lio/src/intensity/coin/coin_ouster_projector.cpp#L66) | 修改：复用 CubeProjector 并加 face*N constant horizontal offset |
| scan-end matching | [coin_feature_manager.cpp:129](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/src/super_lio/src/intensity/coin/coin_feature_manager.cpp#L129) | 修改：cubemap 不再次 deskew；nearest pixel owner + end-frame coordinates |
| photometric residual / J | [coin_photometric_model.cpp:32](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/src/super_lio/src/intensity/coin/coin_photometric_model.cpp#L32) | 未改：P3A 实际使用的 float bilinear + central image gradient + state chain |
| A/b fusion | [coin_observation.cpp:190](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/src/super_lio/src/intensity/coin/coin_observation.cpp#L190) | 函数未改：same patch loop / state column order / factor / accumulator sign |
| gate / selector | [coin_observation.cpp:281](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/src/super_lio/src/intensity/coin/coin_observation.cpp#L281) | 函数未改：g1/gradient 分支，既有 Super-native gate |
| feature tracking / detection | [coin_feature_manager.cpp:192](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/src/super_lio/src/intensity/coin/coin_feature_manager.cpp#L192) | 函数未改：NCC / lifetime / 60 centers / directional or gradient ranking；输入 frame 改变 |
| P3A 接入测试 | [test_coin_feature_math.cpp:70](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/src/super_lio/test/test_coin_feature_math.cpp#L70) | 新增：一个 +X 点的 packed projection FD、hole pixel ownership、constant image residual 有效性 |
| 旧 Cube residual 测试 | [test_cube_photo.cpp:61](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/src/super_lio/test/test_cube_photo.cpp#L61) | 未改：旧 CubeImage exact-bilinear residual/J 路径，不能替代 P3A production-path J 测试 |
| offline32T 调度/归档 | [validate.py:91](https://github.com/Scar-c/cube_lio/blob/efeb69bf6f4814b83cc8709eb06d792ffb47e41e/tools/p3a/validate.py#L91) | 新增：设置环境变量后调用冻结 runner；CLI export-directory 选项在实验后补充 |

`IntensityRepresentation` 是抽象接口；本轮实际复用其 CubeImage 实现的 build/face 数据，然后导出给 COIN 的 patch contract。
P3A production residual 不调用 CubeImage::sampleGradient/computeResidual，也不实例化旧 CoinIntensityRepresentation adapter。
“same fusion/selector”意味着函数文本、参数和 feature cap 相同；不意味着 projection、sample support、patch ownership、有效 feature 集合或每条 row 的 J/r 相同。

## 交给 reviewer 的重点问题

1. **输入 intensity 的定义**：R1–R3 来自 COIN acquisition-pixel 的预处理值，不是原始 DN；R0 与 Cubemap 的图像坐标/matching 语义不同。该实验可否支持 reviewer 所要求的具体 representation-only 结论，需要结合这些边界判断。
2. **量化与 pixel/world 对应**：raw cubemap pixel 的 owner 保留实测 XYZ，reference value 按 rounded pixel 存储；IDW pixel 的 owner 按 depth×pixelRay 构建。审查非整数坐标采样、range gate、reference 与 landmark 的一致性。
3. **IDW 数学与相关性**：检查 support 是否仅 raw、range threshold 的定义、插值 intensity/depth、pixelRay 六面方向，以及多个 filled-pixel measurements 的相关性在 frozen R 下如何解释。
4. **IGM 与生产 J**：IGM 图是 Gaussian derivative magnitude；进入 fusion 时使用未改 COIN 的 central-image-gradient，而不是 exact derivative of bilinear interpolant。不能直接把旧 exact-bilinear Cube residual finite-difference 数值当作新路径的验证。
5. **mask / chart seam**：导出按面 erosion、feature margin、packed projection seam rejection；审查所有实际 track/linearize/detect 入口是否足以避免跨 chart 或 invalid support。
6. **acquisition transform**：Cubemap 图与 private landmark 都在 scan end；新 matching 不二次 deskew，frame T 为 identity。检查完整状态/外参/J 链的一致性。
7. **selector ablation 的效力**：R3 始终只有 ≤22 centers，cap=60；两个 selector 每帧中心集合相同、顺序不同，所以相同失败 SHA 不证明在成功定位时不依赖 geometry。
8. **结果的可识别性**：环境变量才是 COIN representation switch；冻结 runner identity 的默认 photo/projection/measurement 字段不代表本轮 COIN backend。需对照 commands/result manifest/representation.csv。

上述是源码可定位的复审问题与证据限制，不是已经完成唯一原因归因的结论。本轮 NO-GO 是本实现/配置上的 TunnelD 测量，不能推出 CUBE 方法普遍不可行或 Shield4 失败唯一来自 Livox。

## 测试覆盖需要明确收窄

- `test_cube_photo` 的 6000 projection / 1120 residual points 是已有 CubeProjector 与旧 exact-bilinear Cube residual 路径的检查。
- efeb69bf 新增 packed projection finite-difference 检查使用 `(20,2,1)` 的 +X face，检查 XYZ→pixel；没有新增覆盖所有 packed faces 的非零 face-offset 测试。
- 新增 COIN bridge residual 检查使用 constant intensity=100，断言 valid、zero residual、finite J；这不是非恒定 raw/IGM 图像上、六自由度 production residual/J 的 finite-difference 对比。
- 默认 R0 三次 SHA/evaluation/COIN CSV 一致支持本轮默认路径未改变。首个有效 frame 的 fusion audit 支持累加公式；它不是全轨迹逐帧 J 正确性证明。

因此原报告中的 “检查通过” 应按这些具体覆盖解读。若需要更强的 production-path Jacobian 或原论文一致性结论，应另行审查/验证；本次没有补跑或修代码。

## 验证方法

所有 snapshot 的 `SHA256(file)` 与 `git_blob = SHA1("blob " + byte_length + NUL + file)` 均对 target/parent tree 核验。
原 run source manifest 中 110 个记录的文件 SHA 均与 efeb69bf tree 对应文件一致；这是 source content linkage，不是独立 build attestation。
add/finish/update-track-detect 三个冻结 section 重新从 parent/target tree 比较，字节一致，并匹配原 manifest 的 section hashes。
源码快照中的既有格式/空白被原样保留，不做重新格式化。

可直接读取 patch，不需要启动 ROS 或持有 bag：

```bash
git show --stat efeb69bf6f4814b83cc8709eb06d792ffb47e41e
git show --name-only efeb69bf6f4814b83cc8709eb06d792ffb47e41e
git show --binary --full-index efeb69bf6f4814b83cc8709eb06d792ffb47e41e > efeb69bf.patch
git diff 04a48588bcdc4a2210b189aaefe2cf3a29f48302 efeb69bf6f4814b83cc8709eb06d792ffb47e41e -- src/super_lio tools/p3a
git apply --stat COMMIT_FULL.patch
```

原始外部 COIN config/calibration 路径、bag/GT SHA 均保留在 evidence JSON 中。
外部 refs 配置不属于 efeb69bf Git tree，本包没有将其伪装为该 commit 内的文件，也不携带 bag/GT 或可执行二进制。

## efeb69bf 原始实验报告（完整历史记录）

以下报告保留原提交的记录；测试覆盖按前面的补充说明限定。

````markdown
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
````

## 原始 commit 文件统计

```text
commit efeb69bf6f4814b83cc8709eb06d792ffb47e41e
Author: Scar-c <lc1365142579@gmail.com>
Date:   Fri Oct 2 11:29:32 2026 +0800

    Validate TunnelD cubemap representations with frozen COIN fusion

 artifacts/p3a/baseline_coin.json                   |  163 +++
 artifacts/p3a/baseline_parity.json                 |    6 +
 artifacts/p3a/checks.json                          |   28 +
 artifacts/p3a/commands.md                          |   45 +
 artifacts/p3a/control_validation.json              |   77 ++
 artifacts/p3a/cubemap_results.json                 | 1095 ++++++++++++++++++
 artifacts/p3a/determinism.json                     |  774 +++++++++++++
 .../p3a/diagnostics/p3a_r0_coin_g1_run1_coin.csv   | 1181 ++++++++++++++++++++
 artifacts/p3a/diagnostics/p3a_r0_g1_run2_coin.csv  | 1181 ++++++++++++++++++++
 artifacts/p3a/diagnostics/p3a_r0_g1_run3_coin.csv  | 1181 ++++++++++++++++++++
 artifacts/p3a/diagnostics/p3a_r1_g1_run1_coin.csv  | 1181 ++++++++++++++++++++
 .../diagnostics/p3a_r1_g1_run1_representation.csv  | 1177 +++++++++++++++++++
 artifacts/p3a/diagnostics/p3a_r2_g1_run1_coin.csv  | 1181 ++++++++++++++++++++
 .../diagnostics/p3a_r2_g1_run1_representation.csv  | 1177 +++++++++++++++++++
 artifacts/p3a/diagnostics/p3a_r3_g1_run1_coin.csv  | 1181 ++++++++++++++++++++
 .../diagnostics/p3a_r3_g1_run1_representation.csv  | 1177 +++++++++++++++++++
 .../p3a/diagnostics/p3a_r3_gradient_run1_coin.csv  | 1181 ++++++++++++++++++++
 .../p3a_r3_gradient_run1_representation.csv        | 1177 +++++++++++++++++++
 artifacts/p3a/selector_ablation.json               |  623 +++++++++++
 artifacts/p3a/source_manifest.json                 |  190 ++++
 spec/p3a/P3A_PROMPT.md                             |  316 ++++++
 spec/p3a/P3A_TUNNELD_CUBEMAP_VALIDATION_REPORT.md  |  118 ++
 .../intensity/coin/coin_cubemap_representation.hpp |   23 +
 .../include/intensity/coin/coin_observation.hpp    |    4 +
 .../intensity/coin/coin_ouster_projector.hpp       |    8 +-
 .../intensity/coin/coin_cubemap_representation.cpp |  100 ++
 .../src/intensity/coin/coin_feature_manager.cpp    |   17 +
 .../src/intensity/coin/coin_observation.cpp        |   30 +-
 .../src/intensity/coin/coin_ouster_projector.cpp   |   13 +-
 src/super_lio/test/test_coin_feature_math.cpp      |   51 +
 tools/p3a/validate.py                              |  238 ++++
 31 files changed, 16886 insertions(+), 8 deletions(-)
```

## 实际核心代码 diff（完整）

```diff
diff --git a/src/super_lio/include/intensity/coin/coin_cubemap_representation.hpp b/src/super_lio/include/intensity/coin/coin_cubemap_representation.hpp
new file mode 100644
index 0000000000000000000000000000000000000000..880e7da30b2ab37e5aac6bbcf1a5f223347bb949
--- /dev/null
+++ b/src/super_lio/include/intensity/coin/coin_cubemap_representation.hpp
@@ -0,0 +1,23 @@
+#pragma once
+
+#include "intensity/cube_image.hpp"
+#include "intensity/coin/coin_image_processor.hpp"
+#include <string>
+
+namespace cube::coin {
+
+// Explicit opt-in for P3-A; the frozen runner passes this environment through.
+// No switch is inferred from the existing /photo controls.
+std::string p3aRepresentation();
+Settings p3aCubeSettings(const std::string& representation);
+
+// Export the existing Cubemap representation into COIN's patch-image contract.
+// Upstream intensity preprocessing, patch tracking, selector, and fusion stay
+// shared. Filled pixels receive a depth-derived landmark, never a fabricated
+// geometric observation; these points are private to the intensity channel.
+CoinFrame buildCoinCubemap(CubeImage& image,const CoinFrame& calibrated,
+                          const CoinOusterProjector& calibrated_projector,
+                          const CoinImageSettings& settings,
+                          std::vector<CoinScanPoint>& points);
+
+} // namespace cube::coin
diff --git a/src/super_lio/include/intensity/coin/coin_observation.hpp b/src/super_lio/include/intensity/coin/coin_observation.hpp
index 656aa3e7663c70ed065a176016d81afc1f7b2005..e852e6056e7645ff554852a416978555c47603bb 100644
--- a/src/super_lio/include/intensity/coin/coin_observation.hpp
+++ b/src/super_lio/include/intensity/coin/coin_observation.hpp
@@ -3,6 +3,7 @@
 
 #include "common/ds.h"
 #include "intensity/coin/coin_photometric_model.hpp"
+#include "intensity/coin/coin_cubemap_representation.hpp"
 #include <ros/ros.h>
 
 #include <fstream>
@@ -37,6 +38,9 @@ class CoinObservation {
   CoinImageSettings image_settings_;
   CoinFeatureSettings feature_settings_;
   std::unique_ptr<CoinOusterProjector> projector_;
+  std::unique_ptr<CubeImage> cube_representation_;
+  std::string representation_mode_="coin";
+  std::ofstream representation_diagnostics_;
   std::unique_ptr<CoinImageProcessor> image_processor_;
   std::unique_ptr<CoinFeatureManager> feature_manager_;
   std::vector<CoinScanPoint> points_;
diff --git a/src/super_lio/include/intensity/coin/coin_ouster_projector.hpp b/src/super_lio/include/intensity/coin/coin_ouster_projector.hpp
index 0de83ef251e7f3deac57726b1ca14f3186791c6e..f3be1cfb52f0614e4e09caded0d09c3f0dc4858d 100644
--- a/src/super_lio/include/intensity/coin/coin_ouster_projector.hpp
+++ b/src/super_lio/include/intensity/coin/coin_ouster_projector.hpp
@@ -29,10 +29,11 @@ struct ProjectedPoint {
 
 class CoinOusterProjector {
  public:
-  explicit CoinOusterProjector(OusterMetadata metadata);
+  explicit CoinOusterProjector(OusterMetadata metadata, int cube_resolution=0);
   const OusterMetadata& metadata() const { return metadata_; }
-  int rows() const { return metadata_.rows; }
-  int cols() const { return metadata_.cols; }
+  int rows() const { return cube_resolution_ ? cube_resolution_ : metadata_.rows; }
+  int cols() const { return cube_resolution_ ? 6*cube_resolution_ : metadata_.cols; }
+  int cubeResolution() const { return cube_resolution_; }
   std::size_t indexFromPixel(int row, int col) const;
   ProjectedPoint project(const Vec3& p) const;
   Mat23 projectionJacobian(const Vec3& p) const;
@@ -40,6 +41,7 @@ class CoinOusterProjector {
 
  private:
   OusterMetadata metadata_;
+  int cube_resolution_=0;
   std::vector<double> elevation_radians_;
   std::vector<int> raw_to_row_, raw_to_col_;
   Eigen::Matrix3d K_=Eigen::Matrix3d::Zero();
diff --git a/src/super_lio/src/intensity/coin/coin_cubemap_representation.cpp b/src/super_lio/src/intensity/coin/coin_cubemap_representation.cpp
new file mode 100644
index 0000000000000000000000000000000000000000..641c53d0ffc7c6eaa0db04bdf92ee49aec20b86c
--- /dev/null
+++ b/src/super_lio/src/intensity/coin/coin_cubemap_representation.cpp
@@ -0,0 +1,100 @@
+#include "intensity/coin/coin_cubemap_representation.hpp"
+#include <ros/ros.h>
+#include <opencv2/imgproc.hpp>
+#include <cstdlib>
+#include <limits>
+#include <stdexcept>
+
+namespace cube::coin {
+
+std::string p3aRepresentation(){
+  const char* value=std::getenv("CUBE_P3A_REPRESENTATION");
+  const std::string mode=value?value:"coin";
+  if(mode!="coin" && mode!="cube_raw_no_idw" && mode!="cube_raw_idw" && mode!="cube_igm_idw")
+    throw std::invalid_argument("unknown CUBE_P3A_REPRESENTATION: "+mode);
+  return mode;
+}
+
+Settings p3aCubeSettings(const std::string& representation){
+  Settings cfg;ros::NodeHandle nh;
+  nh.param("/cubemap/resolution",cfg.resolution,96);
+  nh.param("/cubemap/idw_radius",cfg.idw_radius,3);
+  nh.param("/cubemap/idw_k",cfg.idw_k,6);
+  nh.param("/cubemap/idw_min_support",cfg.idw_min_support,3);
+  nh.param("/cubemap/idw_power",cfg.idw_power,2.);
+  nh.param("/cubemap/range_absolute",cfg.range_absolute,.3);
+  nh.param("/cubemap/range_relative",cfg.range_relative,.02);
+  nh.param("/cubemap/gaussian_sigma",cfg.gaussian_sigma,1.);
+  cfg.idw_enable=representation!="cube_raw_no_idw";
+  cfg.build_igm=representation=="cube_igm_idw";
+  cfg.validate();return cfg;
+}
+
+namespace {
+Vec3 pixelRay(int face,int u,int v,int n){
+  const double x=2.*u/(n-1)-1.,y=2.*v/(n-1)-1.;
+  const int axis=face/2;const double sign=face%2?-1.:1.;
+  Vec3 p=Vec3::Zero();p[axis]=sign;
+  if(axis==0){p.y()=sign*x;p.z()=y;}
+  if(axis==1){p.x()=-sign*x;p.z()=y;}
+  if(axis==2){p.x()=x;p.y()=sign*y;}
+  return p.normalized();
+}
+}
+
+CoinFrame buildCoinCubemap(CubeImage& image,const CoinFrame& calibrated,
+    const CoinOusterProjector& calibrated_projector,const CoinImageSettings& settings,
+    std::vector<CoinScanPoint>& points){
+  std::vector<RepresentationPoint> input;input.reserve(points.size());
+  // COIN's acquisition-pixel intensity preprocessing is common to all arms.
+  // The point coordinates have already passed the identical support/deskew path.
+  for(const auto& point:points){
+    int row,col;calibrated_projector.pixelFromRawIndex(point.raw_index,row,col);
+    bool masked=false;
+    for(const auto& mask:settings.masks)if(mask.contains(cv::Point(col,row))){masked=true;break;}
+    input.push_back({point.point_lidar,masked?std::numeric_limits<double>::quiet_NaN():
+                     calibrated.intensity.ptr<float>(row)[col]});
+  }
+  image.build(input);
+  const int n=image.coordinateResolution();
+  CoinFrame frame;
+  frame.intensity=cv::Mat::zeros(n,6*n,CV_32F);
+  frame.range=cv::Mat::zeros(n,6*n,CV_32F);
+  frame.mask=cv::Mat::zeros(n,6*n,CV_8U);
+  frame.image_index=cv::Mat::ones(n,6*n,CV_32S)*(-1);
+  frame.dx=cv::Mat::zeros(n,6*n,CV_32F);frame.dy=frame.dx.clone();
+  for(int face=0;face<6;++face){
+    const auto& source=image.face(face);const cv::Rect roi(face*n,0,n,n);
+    const bool igm=!source.igm.empty();
+    (igm?source.igm:source.intensity).convertTo(frame.intensity(roi),CV_32F);
+    source.depth.convertTo(frame.range(roi),CV_32F);
+    cv::Mat mask=(igm?source.igm_mask:source.mask).clone();
+    for(int v=0;v<n;++v)for(int u=0;u<n;++u){
+      if(!mask.at<uchar>(v,u))continue;
+      const double range=source.depth.at<double>(v,u);
+      if(range<settings.min_range || range>settings.max_range){mask.at<uchar>(v,u)=0;continue;}
+      int index=source.point_index[v*n+u];
+      if(index<0){
+        CoinScanPoint point;point.point_lidar=range*pixelRay(face,u,v,n);
+        point.range=range;point.intensity=frame.intensity.at<float>(v,face*n+u);
+        index=static_cast<int>(points.size());points.push_back(point);
+      }
+      frame.image_index.at<int>(v,face*n+u)=index;
+    }
+    // The original COIN patch/erosion settings also apply at each chart border.
+    const int erosion=settings.patch_size+settings.erosion_margin;
+    cv::erode(mask,frame.mask(roi),cv::Mat::ones(erosion,erosion,CV_32F),
+              cv::Point(-1,-1),1,cv::BORDER_CONSTANT,cv::Scalar(0));
+    cv::Mat kernel_dx=(cv::Mat_<float>(1,3)<<-.5f,0.f,.5f);
+    cv::Mat kernel_dy=kernel_dx.t();
+    cv::filter2D(frame.intensity(roi),frame.dx(roi),CV_32F,kernel_dx,cv::Point(-1,-1),0,cv::BORDER_CONSTANT);
+    cv::filter2D(frame.intensity(roi),frame.dy(roi),CV_32F,kernel_dy,cv::Point(-1,-1),0,cv::BORDER_CONSTANT);
+  }
+  frame.raw_intensity=frame.intensity.clone();frame.intensity.convertTo(frame.photo_u8,CV_8U);
+  frame.T_Li_Lk_vec.push_back(Eigen::Matrix4d::Identity());
+  // vec_idx all-zero: frame images and landmarks are both at scan end.
+  frame.vec_idx.assign(points.size(),0);
+  return frame;
+}
+
+} // namespace cube::coin
diff --git a/src/super_lio/src/intensity/coin/coin_feature_manager.cpp b/src/super_lio/src/intensity/coin/coin_feature_manager.cpp
index 595d844608a0700bf3494e0f111b0ef401d62748..7adab96f822d04dc7d1e20b049508e78383a15fb 100644
--- a/src/super_lio/src/intensity/coin/coin_feature_manager.cpp
+++ b/src/super_lio/src/intensity/coin/coin_feature_manager.cpp
@@ -41,6 +41,12 @@ CoinFeatureManager::CoinFeatureManager(CoinOusterProjector projector,CoinFeature
                      projector_.cols()-2*settings_.margin,projector_.rows()-2*settings_.margin);
   if(roi.width<=0||roi.height<=0)throw std::invalid_argument("COIN feature margin removes the full image");
   margin_mask_(roi)=255;
+  if(projector_.cubeResolution()){
+    margin_mask_.setTo(0);
+    const int n=projector_.cubeResolution(),m=settings_.margin;
+    for(int face=0;face<6;++face)
+      margin_mask_(cv::Rect(face*n+m,m,n-2*m,n-2*m))=255;
+  }
   const int half=settings_.patch_size/2;
   patch_offsets_.reserve(settings_.patch_size*settings_.patch_size);
   // The oracle builds offsets as (i,j), then applies i to image x and j to image y.
@@ -125,6 +131,17 @@ bool CoinFeatureManager::projectUndistorted(const CoinFrame& frame,const std::ve
                                              bool round_bucket) const{
   ProjectedPoint projected=projector_.project(p_Lk);
   if(!projected.in_fov)return false;
+  // Cubemap pixels already describe the supported, deskewed end-of-scan cloud.
+  // Applying the Ouster acquisition transform a second time would double deskew.
+  if(projector_.cubeResolution()){
+    const int n=projector_.cubeResolution();
+    const int face=static_cast<int>(projected.uv.x())/n;
+    const int col=std::clamp(static_cast<int>(std::lround(projected.uv.x())),face*n,(face+1)*n-1);
+    const int row=std::clamp(static_cast<int>(std::lround(projected.uv.y())),0,n-1);
+    distortion_index=frame.image_index.ptr<int>(row)[col];
+    if(distortion_index<0 || static_cast<std::size_t>(distortion_index)>=points.size())return false;
+    p_Li=p_Lk;uv=projected.uv;return true;
+  }
   if(round_bucket){projected.uv.x()=std::round(projected.uv.x());projected.uv.y()=std::round(projected.uv.y());}
   int row=static_cast<int>(projected.uv.y()),col=static_cast<int>(projected.uv.x());
   constexpr std::size_t duplicate_points=10;
diff --git a/src/super_lio/src/intensity/coin/coin_observation.cpp b/src/super_lio/src/intensity/coin/coin_observation.cpp
index 7ee790fe7cb51a2bc2bb6b910a05cfd6cd2fff21..f3d824b71119a2d45e32b5f37d57e61a9361a5de 100644
--- a/src/super_lio/src/intensity/coin/coin_observation.cpp
+++ b/src/super_lio/src/intensity/coin/coin_observation.cpp
@@ -58,14 +58,28 @@ CoinObservation::CoinObservation(ros::NodeHandle& nh,const Eigen::Matrix4d& T_IL
   if(!(photo_scale_>0.)||!(measurement_variance_>0.))
     throw std::invalid_argument("invalid COIN scale or measurement variance");
   const OusterMetadata metadata=OusterMetadata::fromRosParams();
-  projector_=std::make_unique<CoinOusterProjector>(metadata);
+  representation_mode_=p3aRepresentation();
+  const auto cube_settings=p3aCubeSettings(representation_mode_);
+  const bool use_cube=representation_mode_!="coin";
+  if(use_cube){
+    cube_representation_=std::make_unique<CubeImage>(cube_settings,
+        cube_settings.build_igm?MeasurementChannel::IntensityGradientMagnitude:MeasurementChannel::RawIntensity);
+  }
+  projector_=std::make_unique<CoinOusterProjector>(metadata,use_cube?cube_settings.resolution:0);
   image_settings_=CoinImageSettings::fromRosParams();
   feature_settings_=CoinFeatureSettings::fromRosParams();
-  image_processor_=std::make_unique<CoinImageProcessor>(*projector_,image_settings_);
+  if(use_cube && cube_settings.resolution<=2*feature_settings_.margin)
+    throw std::invalid_argument("cubemap too small for frozen COIN margin");
+  image_processor_=std::make_unique<CoinImageProcessor>(CoinOusterProjector(metadata),image_settings_);
   feature_manager_=std::make_unique<CoinFeatureManager>(*projector_,feature_settings_);
   std::string output_dir;
   nh.getParam("/lio/offline/out_dir",output_dir);
   if(!output_dir.empty()){
+    if(use_cube){
+      representation_diagnostics_.open(output_dir+"/representation.csv");
+      if(!representation_diagnostics_)throw std::runtime_error("cannot write representation diagnostics");
+      representation_diagnostics_<<"frame,representation,input_points,raw_pixels,filled_pixels,igm_pixels,patch_mask_pixels,intensity_points\n";
+    }
     diagnostics_.open(output_dir+"/coin_observation.csv");
     if(!diagnostics_)throw std::runtime_error("cannot write COIN observation diagnostics");
     diagnostics_<<"frame,timestamp,raw_points,motion_fallback_points,coin_minus_super_scan_end_s,active_before,valid_patches,photo_rows,residual_rms,photo_A_norm,photo_b_norm,weak_dirs,active_after,added,removed,status,skip_reason,ncc_count,ncc_median,rejected_ncc,selector_mode,selected_centers_xy,selected_gradient_mean,selected_score_e1,selected_score_e2,selected_score_e3"
@@ -159,8 +173,16 @@ void CoinObservation::prepare(const LI2Sup::LidarData& lidar,
     return;
   }
   frame_=image_processor_->process(points_);
-  frame_.T_Li_Lk_vec=std::move(transforms);
-  frame_.vec_idx=std::move(transform_indices);
+  if(cube_representation_){
+    const auto input_points=points_.size();
+    frame_=buildCoinCubemap(*cube_representation_,frame_,image_processor_->projector(),image_settings_,points_);
+    if(representation_diagnostics_)representation_diagnostics_<<scan_index_<<','<<representation_mode_<<','
+      <<input_points<<','<<cube_representation_->rawPixelCount()<<','<<cube_representation_->filledPixelCount()<<','
+      <<cube_representation_->validFeaturePixelCount()<<','<<cv::countNonZero(frame_.mask)<<','<<points_.size()<<'\n';
+  }else{
+    frame_.T_Li_Lk_vec=std::move(transforms);
+    frame_.vec_idx=std::move(transform_indices);
+  }
   prepared_=true;active_before_=static_cast<int>(feature_manager_->features().size());
   valid_patches_=photo_rows_=0;residual_square_sum_=photo_A_norm_=photo_b_norm_=0.;
 }
diff --git a/src/super_lio/src/intensity/coin/coin_ouster_projector.cpp b/src/super_lio/src/intensity/coin/coin_ouster_projector.cpp
index a04fd02c9f066de58587f6c66a05475390c01c07..8361b8b0de767af2d345a9e7e395852f4ebbae01 100644
--- a/src/super_lio/src/intensity/coin/coin_ouster_projector.cpp
+++ b/src/super_lio/src/intensity/coin/coin_ouster_projector.cpp
@@ -1,5 +1,6 @@
 // COIN-LIO Ouster projector reimplementation (BSD-3-Clause source authority).
 #include "intensity/coin/coin_ouster_projector.hpp"
+#include "intensity/cube_projector.hpp"
 #include <ros/ros.h>
 #include <algorithm>
 #include <cmath>
@@ -29,7 +30,10 @@ void OusterMetadata::validate() const {
   for(double a:beam_altitude_degrees)if(!std::isfinite(a))throw std::invalid_argument("nonfinite Ouster elevation angle");
 }
 
-CoinOusterProjector::CoinOusterProjector(OusterMetadata metadata):metadata_(std::move(metadata)) {
+CoinOusterProjector::CoinOusterProjector(OusterMetadata metadata,int cube_resolution)
+  :metadata_(std::move(metadata)),cube_resolution_(cube_resolution) {
+  if(cube_resolution_ && (cube_resolution_<8 || cube_resolution_>1024))
+    throw std::invalid_argument("invalid COIN cubemap resolution");
   metadata_.validate();
   elevation_radians_.reserve(metadata_.beam_altitude_degrees.size());
   for(double a:metadata_.beam_altitude_degrees)elevation_radians_.push_back(a*M_PI/180.);
@@ -61,6 +65,12 @@ void CoinOusterProjector::pixelFromRawIndex(std::size_t raw,int& row,int& col) c
 
 ProjectedPoint CoinOusterProjector::project(const Vec3& p) const {
   ProjectedPoint out;
+  if(cube_resolution_){
+    const auto q=cube::CubeProjector(cube_resolution_).project(p);
+    if(q.face<0 || q.seam)return out;
+    out.uv=q.uv;out.uv.x()+=q.face*cube_resolution_;
+    out.in_fov=true;return out;
+  }
   if(!p.allFinite())return out;
   const double L=std::sqrt(p.x()*p.x()+p.y()*p.y())-beam_offset_m_;
   const double R=std::sqrt(p.z()*p.z()+L*L);
@@ -82,6 +92,7 @@ ProjectedPoint CoinOusterProjector::project(const Vec3& p) const {
 }
 
 Mat23 CoinOusterProjector::projectionJacobian(const Vec3& p) const {
+  if(cube_resolution_)return cube::CubeProjector(cube_resolution_).project(p).jacobian;
   const double rxy=p.head<2>().norm(),L=rxy-beam_offset_m_,R2=L*L+p.z()*p.z();
   const double irxy=1./rxy,irxy2=irxy*irxy,fx_irxy2=K_(0,0)*irxy2;
   Mat23 J;
diff --git a/src/super_lio/test/test_coin_feature_math.cpp b/src/super_lio/test/test_coin_feature_math.cpp
index 02c4610fe8f5eb5b75bdb5ee709560786ec6dc43..2b350e91c25e4a14bf959176b2acddd57087a7d9 100644
--- a/src/super_lio/test/test_coin_feature_math.cpp
+++ b/src/super_lio/test/test_coin_feature_math.cpp
@@ -1,5 +1,6 @@
 #include "intensity/coin/coin_feature_manager.hpp"
 #include "intensity/coin/coin_intensity_representation.hpp"
+#include "intensity/coin/coin_cubemap_representation.hpp"
 #include "intensity/coin/super_degeneracy_gate.hpp"
 #include "intensity/intensity_representation.hpp"
 #include <algorithm>
@@ -64,6 +65,56 @@ int main(){
     static_assert(!std::is_abstract<CoinIntensityRepresentation>::value,
                   "COIN adapter must implement projection, samples, residual and validity checks");
 
+    // Exercise the production representation seam, including filled-pixel
+    // landmark ownership and the identity transform that prevents double deskew.
+    OusterMetadata metadata;metadata.rows=128;metadata.cols=1024;
+    metadata.pixel_shift_by_row.assign(128,0);
+    for(int i=0;i<128;++i)metadata.beam_altitude_degrees.push_back(22.5-45.*i/127.);
+    CoinOusterProjector native_projector(metadata),packed_projector(metadata,96);
+    const Vec3 test_point(20.,2.,1.);
+    const auto projected=packed_projector.project(test_point);
+    const auto jacobian=packed_projector.projectionJacobian(test_point);
+    for(int k=0;k<3;++k){
+      Vec3 plus=test_point,minus=test_point;plus[k]+=1e-5;minus[k]-=1e-5;
+      const Vec2 numerical=(packed_projector.project(plus).uv-packed_projector.project(minus).uv)/2e-5;
+      require((numerical-jacobian.col(k)).norm()<1e-7,"packed cube Jacobian must differentiate pixel coordinates");
+    }
+    require(projected.in_fov&&!packed_projector.project(Vec3(1.,1.,0.)).in_fov,
+            "cube seam must not be sampled across charts");
+    CoinFrame calibrated;calibrated.intensity=cv::Mat(128,1024,CV_32F,cv::Scalar(100.));
+    std::vector<CoinScanPoint> cube_points;
+    for(int v=30;v<=65;++v)for(int u=30;u<=65;++u){
+      if(u==47&&v==47)continue;
+      CoinScanPoint point;point.point_lidar=Vec3(20.,20.*(2.*u/95.-1.),20.*(2.*v/95.-1.));
+      point.raw_index=cube_points.size();point.range=point.point_lidar.norm();cube_points.push_back(point);
+    }
+    cube::Settings cube_cfg;cube_cfg.build_igm=false;
+    cube::CubeImage cube_image(cube_cfg,cube::MeasurementChannel::RawIntensity);
+    CoinImageSettings image_settings;image_settings.masks.clear();
+    const auto packed=buildCoinCubemap(cube_image,calibrated,native_projector,image_settings,cube_points);
+    const int filled_index=packed.image_index.at<int>(47,47);
+    require(filled_index>=0&&static_cast<std::size_t>(filled_index)<cube_points.size(),
+            "IDW patch pixel must own a private depth-derived intensity landmark");
+    const auto reprojection=packed_projector.project(cube_points[filled_index].point_lidar);
+    require((reprojection.uv-Vec2(47.,47.)).norm()<1e-10,"filled landmark must reproject to its reference pixel");
+    require(packed.mask.at<uchar>(47,47)&&!packed.mask.at<uchar>(47,95),
+            "patch mask must preserve a supported interior and exclude chart boundaries");
+    require(packed.T_Li_Lk_vec.size()==1&&packed.T_Li_Lk_vec[0].isIdentity()&&
+            packed.vec_idx.size()==cube_points.size(),"end-frame representation must use identity acquisition transform");
+    CoinFeatureManager packed_manager(packed_projector);
+    Vec3 acquisition;Vec2 uv;int index=-1;
+    require(packed_manager.projectUndistorted(packed,cube_points,cube_points[filled_index].point_lidar,
+                                             acquisition,uv,index,true)&&
+            (acquisition-cube_points[filled_index].point_lidar).norm()==0.,
+            "cubemap matching must never deskew the end-frame point twice");
+    const Eigen::Matrix4d identity=Eigen::Matrix4d::Identity();
+    const auto photo_row=CoinPhotometricModel::linearize(packed_manager,packed_projector,packed,cube_points,
+        identity,identity,cube_points[filled_index].point_lidar,100.,.7,30.,10);
+    require(photo_row.valid&&photo_row.residual==0.&&photo_row.correction_jacobian_super.allFinite(),
+            "cubemap patch must enter the shared COIN residual and Jacobian path");
+    std::cout<<"{\"packed_cube_jacobian\":\"PASS\",\"idw_landmark_ownership\":\"PASS\","
+               "\"chart_boundary_mask\":\"PASS\",\"no_double_deskew\":\"PASS\",\"shared_coin_residual\":\"PASS\"}\n";
+
     Eigen::MatrixXd duplicated(2*H.rows(),3);
     duplicated.topRows(H.rows())=H;duplicated.bottomRows(H.rows())=H;
     const auto duplicated_weak=CoinFeatureManager::weakDirectionsFromGeometry(duplicated,R,5.);
diff --git a/tools/p3a/validate.py b/tools/p3a/validate.py
new file mode 100644
index 0000000000000000000000000000000000000000..0d13f9393fd4649424a69295b71ffb70d442003f
--- /dev/null
+++ b/tools/p3a/validate.py
@@ -0,0 +1,238 @@
+#!/usr/bin/env python3
+"""P3-A orchestration/archival; delegates replay and evaluation unchanged."""
+import csv
+import argparse
+import hashlib
+import json
+import math
+import os
+import re
+from pathlib import Path
+import shutil
+import subprocess
+
+ROOT = Path(__file__).resolve().parents[2]
+OUT = ROOT / 'artifacts/p3a'
+PREFIX = 'p3a'
+BASE = '04a48588bcdc4a2210b189aaefe2cf3a29f48302'
+G1 = '0.31913064578672057'
+G2 = '0.359189249724233'
+MODES = {'R0': 'coin', 'R1': 'cube_raw_no_idw',
+         'R2': 'cube_raw_idw', 'R3': 'cube_igm_idw'}
+
+
+def sha(path):
+    digest = hashlib.sha256()
+    with path.open('rb') as stream:
+        for block in iter(lambda: stream.read(1 << 20), b''):
+            digest.update(block)
+    return digest.hexdigest()
+
+
+def write(name, value):
+    (OUT / name).write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')
+
+
+def load(path):
+    return json.loads(path.read_text())
+
+
+def diagnostics(path):
+    with path.open() as stream:
+        rows = list(csv.DictReader(stream))
+    def values(key):
+        return [float(row[key]) for row in rows if row.get(key, '') != '']
+    result = {'scans': len(rows), 'csv_sha256': sha(path)}
+    for key in ('active_before', 'active_after', 'valid_patches', 'photo_rows',
+                'residual_rms', 'ncc_median', 'gate_active', 'motion_fallback_points'):
+        data = values(key)
+        result[key] = {'mean': sum(data)/len(data), 'min': min(data),
+                       'max': max(data), 'sum': sum(data)} if data else None
+    photo_rows = sum(float(row['photo_rows']) for row in rows)
+    result['pooled_residual_rms'] = math.sqrt(sum(
+        float(row['photo_rows'])*float(row['residual_rms'])**2 for row in rows)/photo_rows) if photo_rows else None
+    result['skipped_scans'] = sum(row['status'] != 'USED' for row in rows)
+    return result
+
+
+def summarize(name, representation, selector):
+    folder = ROOT / 'runtime' / name
+    result = load(folder / 'result.json')
+    if result['status'] != 'SUCCESS':
+        raise RuntimeError(f'{name} failed: {result}')
+    evaluation = load(folder / 'evaluation.json')
+    if not math.isfinite(evaluation['ate_rmse_m']):
+        raise RuntimeError(f'{name}: nonfinite ATE')
+    record = {'name': name, 'representation': representation, 'selector': selector,
+              'trajectory_sha256': sha(folder / 'trajectory.tum'),
+              'evaluation_sha256': sha(folder / 'evaluation.json'),
+              'evaluation': evaluation, 'runtime': load(folder / 'run.json'),
+              'identity': load(folder / 'identity.json'),
+              'diagnostics': diagnostics(folder / 'coin_observation.csv')}
+    audit = folder / 'fusion.json'
+    if audit.exists():
+        record['fusion_audit'] = load(audit)
+        if record['fusion_audit']['status'] != 'PASS':
+            raise RuntimeError(f'{name}: fusion audit failed')
+    representation_csv = folder / 'representation.csv'
+    if representation_csv.exists():
+        with representation_csv.open() as stream:
+            rows = list(csv.DictReader(stream))
+        record['representation_diagnostics'] = {
+            'scans': len(rows), 'sha256': sha(representation_csv),
+            'sum': {key: sum(int(row[key]) for row in rows) for key in
+                    ('input_points', 'raw_pixels', 'filled_pixels', 'igm_pixels',
+                     'patch_mask_pixels', 'intensity_points')}}
+        shutil.copyfile(representation_csv, OUT / 'diagnostics' / f'{name}_representation.csv')
+    shutil.copyfile(folder / 'coin_observation.csv', OUT / 'diagnostics' / f'{name}_coin.csv')
+    return record
+
+
+def run(representation, selector, repeat, port):
+    name = f'{PREFIX}_{representation.lower()}_{selector}_run{repeat}'
+    command = ['tools/offline/run_experiment.sh', 'tunnel_d', '--name', name,
+               '--port', str(port), '--threads', '32', '--coin', '--selector', selector,
+               '--gate-g1-threshold', G1, '--gate-g2-threshold', G2,
+               '--fusion-audit-json', f'runtime/{name}/fusion.json']
+    mode = MODES[representation]
+    with (OUT / 'commands.md').open('a') as stream:
+        stream.write('\n```bash\nCUBE_P3A_REPRESENTATION=' + mode + ' ' + ' '.join(command) + '\n```\n')
+    env = dict(os.environ, CUBE_P3A_REPRESENTATION=mode)
+    print(json.dumps({'starting': name, 'mode': mode, 'threads': 32}), flush=True)
+    with (ROOT / 'runtime' / f'{name}_launch.log').open('w') as log:
+        subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=log, check=True)
+    record = summarize(name, mode, selector)
+    record['command'] = command
+    record['representation_environment'] = {'CUBE_P3A_REPRESENTATION': mode}
+    record['build_source_sha256'] = load(OUT / 'source_manifest.json')['working_source_sha256']
+    print(json.dumps({'finished': name, 'ate_rmse_m': record['evaluation']['ate_rmse_m'],
+                      'sha256': record['trajectory_sha256']}), flush=True)
+    return record
+
+
+def source_manifest():
+    paths = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard',
+                                      'src/super_lio', 'src/basic', 'tools/offline', 'eval',
+                                      'tools/cube_lio/config'], cwd=ROOT, text=True).splitlines()
+    working = {path: sha(ROOT / path) for path in paths if (ROOT / path).is_file()}
+    changed = {}
+    for path, digest in working.items():
+        original = subprocess.run(['git', 'show', f'{BASE}:{path}'], cwd=ROOT,
+                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
+        previous = hashlib.sha256(original.stdout).hexdigest() if original.returncode == 0 else None
+        if previous != digest:
+            changed[path] = {'base_sha256': previous, 'working_sha256': digest}
+    protected = ['src/super_lio/src/lio/', 'src/super_lio/include/lio/',
+                 'src/super_lio/src/ros/', 'src/super_lio/include/ros/', 'src/basic/',
+                 'src/super_lio/src/apps/', 'tools/offline/', 'eval/', 'tools/cube_lio/config/']
+    if any(any(path.startswith(prefix) for prefix in protected) or
+           path.endswith('super_degeneracy_gate.hpp') for path in changed):
+        raise RuntimeError('frozen component changed')
+    functions = {
+        'fusion_add': ('src/super_lio/src/intensity/coin/coin_observation.cpp',
+                       'void CoinObservation::add(', 'void CoinObservation::finish('),
+        'gate_and_selector_finish': ('src/super_lio/src/intensity/coin/coin_observation.cpp',
+                                     'void CoinObservation::finish(', None),
+        'feature_track_detect': ('src/super_lio/src/intensity/coin/coin_feature_manager.cpp',
+                                 'void CoinFeatureManager::update(', None),
+    }
+    function_checks = {}
+    for label, (path, start, end) in functions.items():
+        before = subprocess.check_output(['git', 'show', f'{BASE}:{path}'], cwd=ROOT, text=True)
+        after = (ROOT / path).read_text()
+        def section(value):
+            value = value[value.index(start):]
+            return value[:value.index(end)] if end else value
+        previous, current = section(before), section(after)
+        function_checks[label] = {'identical': previous == current,
+                                 'sha256': hashlib.sha256(current.encode()).hexdigest()}
+        if previous != current:
+            raise RuntimeError(f'frozen function changed: {label}')
+    return {'base_commit': BASE, 'working_source_sha256': working,
+            'changed_source_files': changed, 'frozen_components_unchanged': True,
+            'frozen_function_checks': function_checks,
+            'binary_sha256': sha(ROOT / 'devel/lib/super_lio/cube_offline_node'),
+            'lio_library_sha256': sha(ROOT / 'devel/lib/liblio.so'),
+            'switch': 'CUBE_P3A_REPRESENTATION; opt-in, default coin',
+            'frozen_settings': {'selector': 'g1', 'g1_threshold': float(G1), 'g2_threshold': float(G2),
+                'photo_scale': .00095, 'measurement_variance': .001, 'num_features': 60,
+                'patch_size': 5, 'max_lifetime': 25, 'suppression_radius': 10, 'grad_min': 16.5,
+                'ncc_threshold': .7075, 'margin': 10, 'range_threshold': .2,
+                'resolution': 96, 'idw_radius': 3, 'idw_k': 6, 'idw_min_support': 3,
+                'idw_power': 2., 'range_absolute': .3, 'range_relative': .02, 'gaussian_sigma': 1.}}
+
+
+def repeat_check(representation, selector, first, port):
+    repeats = [first] + [run(representation, selector, repeat, port+repeat-2) for repeat in (2, 3)]
+    identical_sha = len({record['trajectory_sha256'] for record in repeats}) == 1
+    identical_metrics = all(record['evaluation'] == first['evaluation'] for record in repeats)
+    identical_diagnostics = len({record['diagnostics']['csv_sha256'] for record in repeats}) == 1
+    return {'representation': representation, 'selector': selector, 'runs': repeats,
+            'trajectory_sha_identical': identical_sha, 'evaluation_metrics_identical': identical_metrics,
+            'coin_diagnostics_identical': identical_diagnostics,
+            'pass': identical_sha and identical_metrics and identical_diagnostics}
+
+
+def main():
+    global OUT, PREFIX
+    parser = argparse.ArgumentParser(description=__doc__)
+    parser.add_argument('--output-dir', type=Path, default=Path('artifacts/p3a'))
+    parser.add_argument('--prefix', default='p3a', help='fresh runtime folder prefix for repeats/ablations')
+    parser.add_argument('--baseline-run', default='p3a_r0_coin_g1_run1', help='completed native COIN G1 Phase 0 run')
+    args = parser.parse_args()
+    if not re.fullmatch(r'[A-Za-z0-9_]+', args.prefix) or Path(args.baseline_run).name != args.baseline_run:
+        parser.error('prefix and baseline-run must be single safe folder names')
+    PREFIX = args.prefix
+    OUT = args.output_dir if args.output_dir.is_absolute() else ROOT / args.output_dir
+    OUT.mkdir(exist_ok=False, parents=True)
+    (OUT / 'diagnostics').mkdir()
+    write('source_manifest.json', source_manifest())
+    (OUT / 'commands.md').write_text('# P3-A commands\n\nAll replays use the frozen offline runner and 32T.\n'
+        'The explicit environment switch is recorded separately because the frozen runner does not record it.\n'
+        'Source and binary hashes are in `source_manifest.json` and each result. No configurations are edited.\n\n'
+        '```bash\ngit switch -c p3a-tunneld-cubemap-validation\n'
+        'tools/offline/run_experiment.sh tunnel_d --name p3a_r0_coin_g1_run1 --port 11740 --threads 32 '
+        '--coin --selector g1 --gate-g1-threshold ' + G1 + ' --gate-g2-threshold ' + G2 + ' '
+        '--fusion-audit-json runtime/p3a_r0_coin_g1_run1/fusion.json\n'
+        'source /opt/ros/noetic/setup.bash\nsource devel/setup.bash\n'
+        'catkin_make --pkg super_lio --make-args cube_offline_node test_coin_feature_math test_cube_photo -j2\n'
+        'devel/lib/super_lio/test_coin_feature_math\ndevel/lib/super_lio/test_cube_photo\n'
+        'python3 tools/p3a/validate.py\n```\n')
+    baseline = summarize(args.baseline_run, 'coin', 'g1')
+    write('baseline_coin.json', baseline)
+    # Verify opt-in isolation before any Cubemap run; these are also R0's
+    # required three repeats, including the clean, pre-integration Phase 0 run.
+    checks = [repeat_check('R0', 'g1', baseline, 11745)]
+    if not checks[0]['pass']:
+        raise RuntimeError('default COIN baseline changed after representation integration')
+    records = {'R0': baseline}
+    for index, representation in enumerate(('R1', 'R2', 'R3')):
+        records[representation] = run(representation, 'g1', 1, 11741+index)
+        write('cubemap_results.json', {'same_selector_feature_limit_fusion': True, 'results': records})
+    best = min(('R1', 'R2', 'R3'), key=lambda key: records[key]['evaluation']['ate_rmse_m'])
+    all_use = run(best, 'gradient', 1, 11744)
+    write('selector_ablation.json', {'best_cubemap': best, 'selection_rule': 'minimum R1/R2/R3 full-bag ATE; no tuning',
+          'weakest_with_super_native_gate': records[best], 'all_use_no_direction_dependency': all_use,
+          'all_use_means': 'existing COIN gradient mode: pure gradient ranking, same cap/NCC/fusion; gate not used'})
+    candidates = [(key, record['selector'], record) for key, record in records.items()
+                  if key != 'R0' and record['evaluation']['ate_rmse_m'] < 10.]
+    if all_use['evaluation']['ate_rmse_m'] < 10.:
+        candidates.append((best, 'gradient', all_use))
+    for index, (representation, selector, first) in enumerate(candidates):
+        checks.append(repeat_check(representation, selector, first, 11747+index*2))
+    write('determinism.json', {'criterion': 'three runs for every ATE<10 candidate', 'checks': checks,
+          'runtime_not_required_identical': True, 'pass': all(check['pass'] for check in checks)})
+    if not all(check['pass'] for check in checks):
+        raise RuntimeError('determinism failed')
+    original = ROOT / 'runtime/offline_infra_tunneld_g1_threads32'
+    write('baseline_parity.json', {'reference': str(original.relative_to(ROOT)),
+        'reference_trajectory_sha256': sha(original / 'trajectory.tum'),
+        'phase0_trajectory_sha256': baseline['trajectory_sha256'],
+        'identical': sha(original / 'trajectory.tum') == baseline['trajectory_sha256']})
+    print(json.dumps({'complete': True, 'best_cubemap': best,
+        'best_ate_rmse_m': records[best]['evaluation']['ate_rmse_m'],
+        'cubemap_below_10m': any(records[key]['evaluation']['ate_rmse_m'] < 10. for key in ('R1', 'R2', 'R3'))}), flush=True)
+
+
+if __name__ == '__main__':
+    main()
```

## 完整源码与关键配置（全部来自 efeb69bf）

### eval/evaluate.py

Git blob `2d6c8dbc042256a161318d8db0e8356b9e7a1696`；SHA-256 `07b2a25d83658baeec934dd7b003e69b5bc1a28ba9075310f544da48ec7f9e68`；79 行完整文件。

````python
#!/usr/bin/env python3
"""Frozen no-scale SE3 ATE; NTU author association, evo association for stress sets."""
import hashlib
import json
from pathlib import Path
import sys
import numpy as np
from evo.core import sync
import ntu_author as ntu

ROOT = Path(__file__).resolve().parents[1]

def ground_truth(dataset):
    if dataset == 'eee_01':
        cache = ROOT/'runtime/eee_01_gt.tum'
        if not cache.exists():
            import rosbag
            with rosbag.Bag('/home/lc/super_livo/bag/NTU/eee_01/eee_01.bag') as bag, cache.open('w') as out:
                for _,m,_ in bag.read_messages(topics=['/leica/pose/relative']):
                    p,q=m.pose.position,m.pose.orientation
                    out.write(' '.join(format(v,'.17g') for v in [m.header.stamp.to_sec(),p.x,p.y,p.z,q.x,q.y,q.z,q.w])+'\n')
        return cache
    if dataset == 'shield1': return Path('/home/lc/algorithm_versa/bag/GEODE/Shield_tunnel1.txt')
    if dataset == 'shield4': return Path('/home/lc/algorithm_versa/bag/GEODE/Shield_tunnel4.txt')
    return Path('/home/lc/algorithm_versa/bag/ENWIDE/gt-tunnel_d.tum')

def main():
    dataset,folder=sys.argv[1:]
    folder=Path(folder)
    if dataset == 'eee_01':
        times,positions,quats=ntu.load_tum(folder/'trajectory.tum')
    else:
        raw=np.loadtxt(folder/'trajectory.tum')
        if not np.all(np.isfinite(raw)) or np.any(np.diff(raw[:,0])<0):
            raise ValueError('invalid estimate')
        times,positions,quats=raw[:,0],raw[:,1:4],raw[:,4:8]
    gt_path=ground_truth(dataset)
    gt=np.loadtxt(gt_path)
    if not np.all(np.isfinite(gt)) or np.any(np.diff(gt[:,0])<=0):
        raise ValueError('invalid GT positions/timestamps')
    if dataset == 'eee_01':
        gt_times,gt_positions=ntu.remove_dataset_position_duplicates(gt[:,0],gt[:,1:4])
        indices,matched_gt=ntu.interpolate_dataset_gt(times,gt_times,gt_positions,max_bracket=.1)
        positions=positions+np.array([ntu.quaternion_to_rotation(q)@ntu.PRISM_B for q in quats])
        estimated=positions[indices]
        association='NTU author linear interpolation strict bracket <0.1s, duplicate removal'
    else:
        if dataset in ('shield1', 'shield4'):
            # Official gamma2GT_leica.py: T_eval=T_device @ inverse(T).
            q=np.array([-.00492765,.00575961,.0117651,.999901])
            x,y,z,w=q
            r=np.array([[1-2*y*y-2*z*z,2*x*y-2*z*w,2*x*z+2*y*w],
                        [2*x*y+2*z*w,1-2*x*x-2*z*z,2*y*z-2*x*w],
                        [2*x*z-2*y*w,2*y*z+2*x*w,1-2*x*x-2*y*y]])
            lever=-np.linalg.inv(r)@np.array([.00947221,-.308202,-.365733])
        else: lever=np.array([-.006253,.011775,.10825])
        positions=positions+np.array([ntu.quaternion_to_rotation(q)@lever for q in quats])
        # Stress GT contains position-only Leica data; its quaternion is unused.
        if len(gt)<len(times):
            ids_gt,ids_est=sync.matching_time_indices(gt[:,0],times,max_diff=.1,offset_2=0.)
        else:
            ids_est,ids_gt=sync.matching_time_indices(times,gt[:,0],max_diff=.1,offset_2=0.)
        estimated=positions[ids_est]; matched_gt=gt[ids_gt,1:4]
        association='evo nearest timestamps max_diff=0.1s offset=0'
    rotation,translation=ntu.align_se3(estimated,matched_gt)
    errors=np.linalg.norm(estimated@rotation.T+translation-matched_gt,axis=1)
    result={'dataset':dataset,'alignment':'SE3 no scale','association':association,
            'ate_rmse_m':float(np.sqrt(np.mean(errors**2))), 'ate_mean_m':float(errors.mean()),
            'ate_median_m':float(np.median(errors)), 'ate_max_m':float(errors.max()),
            'matched':len(errors),'duplicate_estimate_timestamps':int(np.sum(np.diff(times)==0)),
            'frames':len(times),'processed_duration_s':float(times[-1]-times[0]),
            'first_timestamp':float(times[0]),'last_timestamp':float(times[-1]),
            'gt_path':str(gt_path),'gt_sha256':hashlib.sha256(gt_path.read_bytes()).hexdigest(),
            'evaluator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'ntu_author_sha256':hashlib.sha256((ROOT/'eval/ntu_author.py').read_bytes()).hexdigest()}
    (folder/'evaluation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))

if __name__=='__main__': main()
````

### eval/ntu_author.py

Git blob `18df1cd2aa4d4e37a6a5491dab0304e03c247184`；SHA-256 `092beba2b99ac02cfbb1d30b1c0b1ec49cf2b41203090a81c66eb0d0824187dd`；211 行完整文件。

````python
# Provenance: recovered from the legacy `super-livo` branch of the Super-LIO
# repository (repo: /home/lc/super_livo_legacy/src/Super-LIO, branch super-livo).
#   eval_ntu_viral_official.py : commit 829b65b21dbe83cf73165096e581077b13b9c9df
#                                "feat(super-livo): recover official benchmark evaluators"
#   ntu_viral_official_ate.py   : same commit 829b65b2 (recovered alongside)
#   pose_bag_to_tum.py          : commit 17b493bc1b67bba3b0582fb631aae739cab4b0eb
#                                "bench(super-lio): reproduce pristine NTU baselines"
# Upstream evaluator provenance (pinned inside eval_ntu_viral_official.py):
#   ntu-aris/viral_eval @ 194dd4595b1fb5e8ae2a5a0c01255f816ab4082f (dataset author).
# Prob-LIO keeps this as the canonical NTU metric; do not replace with evo_ape.
#!/usr/bin/env python3
"""Python parity wrapper for the NTU VIRAL dataset-author MATLAB evaluator."""
import argparse
import hashlib
import pathlib
import sys
from typing import Optional, Sequence

import numpy as np
import yaml


UPSTREAM_REPOSITORY = "https://github.com/ntu-aris/viral_eval"
UPSTREAM_REVISION = "194dd4595b1fb5e8ae2a5a0c01255f816ab4082f"
UPSTREAM_FILES = {
    "evaluate_one.m": "20756b58d56dcaa66d9add0d41662034c01a329ca0fb4cda1215e5c853faec11",
    "traj_align.m": "f0ab16497ab35e709bf7c2c96a44aa50eec0f1bd78b595f7cd5ddd9fbb6bb319",
    "combteeth.m": "817bb3070c5d778e84e54e31a5d61094ce5c444f13e70809291a89541cba9e87",
    "vecitp.m": "730ccf7a973fba6dfaec2729d97c5861e6dc1a493df9a890c623ab21b37323d5",
    "quatconv.m": "5a28231a53b18ded9d4361e0d00738e5dbcb1cb1d0bf7ce8fd0a049dbb1294a8",
    "trans_B2prism.csv": "e4f50bcbbc3e670d268aab26ad80de77013819413833bb9642bea46db90232b9",
}
PRISM_B = np.asarray([-0.293656, -0.012288, -0.273095], dtype=float)


def sha256_file(path):
    digest = hashlib.sha256()
    with pathlib.Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_tum(path):
    timestamps, positions, quaternions = [], [], []
    with pathlib.Path(path).open(encoding="utf-8") as stream:
        for line_number, line in enumerate(stream, 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            fields = line.split()
            if len(fields) < 8:
                raise ValueError(f"{path}:{line_number}: expected at least 8 columns")
            values = np.asarray([float(value) for value in fields[:8]], dtype=float)
            if not np.all(np.isfinite(values)):
                raise ValueError(f"{path}:{line_number}: non-finite TUM row")
            timestamps.append(values[0])
            positions.append(values[1:4])
            quaternions.append(values[4:8])
    if not timestamps:
        raise ValueError(f"{path}: no TUM rows")
    timestamps = np.asarray(timestamps)
    if np.any(np.diff(timestamps) <= 0):
        raise ValueError(f"{path}: timestamps are not strictly increasing")
    return timestamps, np.asarray(positions), np.asarray(quaternions)


def quaternion_to_rotation(quaternion):
    norm = float(np.linalg.norm(quaternion))
    if not np.isfinite(norm) or norm <= 0:
        raise ValueError("invalid estimate quaternion")
    x, y, z, w = quaternion / norm
    return np.array(
        [
            [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
            [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
            [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
        ],
        dtype=float,
    )


def remove_dataset_position_duplicates(timestamps, positions):
    """Replicate union(unique-index(x), unique-index(y), unique-index(z))."""
    retained = set()
    for axis in range(3):
        _, first_indices = np.unique(positions[:, axis], return_index=True)
        retained.update(int(index) for index in first_indices)
    indices = np.asarray(sorted(retained), dtype=int)
    return timestamps[indices], positions[indices]


def interpolate_dataset_gt(est_times, gt_times, gt_positions, max_bracket=0.1):
    interpolated, estimate_indices = [], []
    last_gt_index = 0
    for estimate_index, timestamp in enumerate(est_times):
        match = None
        for gt_index in range(last_gt_index, len(gt_times) - 1):
            if (
                gt_times[gt_index] <= timestamp < gt_times[gt_index + 1]
                and abs(gt_times[gt_index + 1] - gt_times[gt_index]) < max_bracket
            ):
                match = gt_index
                last_gt_index = gt_index
                break
        if match is None:
            continue
        fraction = (timestamp - gt_times[match]) / (gt_times[match + 1] - gt_times[match])
        interpolated.append(
            gt_positions[match]
            + fraction * (gt_positions[match + 1] - gt_positions[match])
        )
        estimate_indices.append(estimate_index)
    return np.asarray(estimate_indices, dtype=int), np.asarray(interpolated, dtype=float)


def align_se3(estimate, ground_truth):
    if estimate.shape != ground_truth.shape or len(estimate) < 3:
        raise ValueError("SE(3) alignment requires at least three position pairs")
    estimate_mean = estimate.mean(axis=0)
    gt_mean = ground_truth.mean(axis=0)
    covariance = (ground_truth - gt_mean).T @ (estimate - estimate_mean) / len(estimate)
    u_matrix, _, vt_matrix = np.linalg.svd(covariance)
    correction = np.eye(3)
    if np.linalg.det(u_matrix) * np.linalg.det(vt_matrix.T) < 0:
        correction[2, 2] = -1
    rotation = u_matrix @ correction @ vt_matrix
    translation = gt_mean - rotation @ estimate_mean
    return rotation, translation


def evaluate(est_times, est_positions, est_quaternions, gt_times, gt_positions, prism_b=PRISM_B):
    gt_times, gt_positions = remove_dataset_position_duplicates(gt_times, gt_positions)
    prism_positions = np.empty_like(est_positions)
    for index, (position, quaternion) in enumerate(zip(est_positions, est_quaternions)):
        prism_positions[index] = position + quaternion_to_rotation(quaternion) @ prism_b
    indices, interpolated_gt = interpolate_dataset_gt(
        est_times, gt_times, gt_positions, max_bracket=0.1
    )
    if len(indices) < 3:
        raise ValueError(f"insufficient interpolated matches: {len(indices)} < 3")
    compared = prism_positions[indices]
    rotation, translation = align_se3(compared, interpolated_gt)
    aligned = (rotation @ compared.T).T + translation
    component_errors = interpolated_gt - aligned
    component_rms = np.sqrt(np.mean(component_errors ** 2, axis=0))
    ate = float(np.linalg.norm(component_rms))
    return {
        "primary_metric": "NTU_VIRAL_DATASET_TRANSLATION_ATE_RMSE_M",
        "translation_ate_rmse_m": ate,
        "component_rmse_m": [float(value) for value in component_rms],
        "matched": int(len(indices)),
        "lever_arm_applied": bool(np.linalg.norm(prism_b) > 0),
        "prism_translation_B_m": [float(value) for value in prism_b],
        "alignment": "SE3_UMEYAMA_NO_SCALE",
        "association": "LINEAR_GT_INTERPOLATION_AT_ESTIMATE_TIMESTAMPS",
        "interpolation_max_bracket_s": 0.1,
        "interpolation_bracket_comparison": "STRICT_LESS_THAN",
        "estimated_pose_frame": "IMU_BODY_W_T_B",
        "ground_truth_measurement_frame": "LEICA_PRISM_POSITION_IN_LEICA_WORLD",
    }


def identity(path):
    path = pathlib.Path(path).resolve()
    return {"path": str(path), "size_bytes": path.stat().st_size, "sha256": sha256_file(path)}


def parse_args(argv: Optional[Sequence[str]] = None):
    parser = argparse.ArgumentParser(description="NTU VIRAL dataset-author-compatible evaluator")
    parser.add_argument("estimate", type=pathlib.Path, help="estimated IMU/body W_T_B in TUM")
    parser.add_argument("leica", type=pathlib.Path, help="Leica prism positions in TUM")
    parser.add_argument("--out", type=pathlib.Path, required=True)
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    try:
        est_times, est_positions, est_quaternions = load_tum(args.estimate)
        gt_times, gt_positions, _ = load_tum(args.leica)
        metrics = evaluate(
            est_times, est_positions, est_quaternions, gt_times, gt_positions, PRISM_B
        )
        result = {
            "schema_version": 1,
            "evaluator": {
                "name": metrics["primary_metric"],
                "provenance_tier": "DATASET_AUTHOR_BENCHMARK",
                "upstream_repository": UPSTREAM_REPOSITORY,
                "upstream_revision": UPSTREAM_REVISION,
                "upstream_files_sha256": UPSTREAM_FILES,
                "wrapper_path": str(pathlib.Path(__file__).resolve()),
                "wrapper_sha256": sha256_file(pathlib.Path(__file__)),
            },
            "inputs": {"estimate": identity(args.estimate), "leica": identity(args.leica)},
            "result": metrics,
        }
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(yaml.safe_dump(result, sort_keys=False), encoding="utf-8")
        print(f"primary_metric: {metrics['primary_metric']}")
        print(f"translation_ate_rmse_m: {metrics['translation_ate_rmse_m']:.9f}")
        print(f"matched: {metrics['matched']}")
        return 0
    except (OSError, ValueError, np.linalg.LinAlgError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
````

### src/super_lio/CMakeLists.txt

Git blob `d7ffe5b633106be5fdcc01307840260db7ce08e3`；SHA-256 `c2a23da5f6196d0bc8e6f1183b5ff135a8f5ac9a3094e08944a918ef3e2c53c5`；150 行完整文件。

````text
cmake_minimum_required(VERSION 3.16)
project(super_lio)


#DEBUG CONFIGURATION
# set(CMAKE_BUILD_TYPE "Debug")
# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_FLAGS_DEBUG "-O0 -Wall -g -pthread -fsanitize=address")

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -pthread -fexceptions -flto=auto")
add_definitions(-DROOT=\"${CMAKE_CURRENT_SOURCE_DIR}/\")

# Find packages
find_package(Eigen3 REQUIRED)
find_package(catkin REQUIRED COMPONENTS
  roscpp
  rosbag
  rospy
  basic
  pcl_ros
  std_msgs
  geometry_msgs
  sensor_msgs
  visualization_msgs
  nav_msgs
  message_generation
)
find_package(PythonLibs REQUIRED)
find_package(OpenCV REQUIRED)

add_message_files(
  FILES
  CloudPose.msg
  CloudPose2.msg
)

generate_messages(
  DEPENDENCIES
  std_msgs
  sensor_msgs
  geometry_msgs
)

# Include directories for libraries
include_directories(
  ${catkin_INCLUDE_DIRS}
  ${EIGEN3_INCLUDE_DIRS}
  include
  3rdparty
)

# Package setup for catkin
catkin_package(
  CATKIN_DEPENDS geometry_msgs nav_msgs roscpp rospy std_msgs message_runtime
  DEPENDS EIGEN3 PCL
  INCLUDE_DIRS include
)

# LIO library source files
file(GLOB_RECURSE LIO_LIB_SRC CONFIGURE_DEPENDS "src/lio/*.cpp" "src/ros/*.cpp" "src/intensity/*.cpp")

file(GLOB_RECURSE LIO_LIB_HEADERS "include/lio/*.hpp" "include/lio/*.h")

# library build
add_library(lio ${LIO_LIB_SRC} ${LIO_LIB_HEADERS})

# Target specific includes
target_include_directories(lio PUBLIC
  include
  3rdparty
  ${EIGEN3_INCLUDE_DIRS}
  ${catkin_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  ${PCL_INCLUDE_DIRS}
)

# Linking libraries
target_link_libraries(lio
  ${EIGEN3_LIBRARIES}
  ${catkin_LIBRARIES}
  ${PCL_LIBRARIES}
  ${PYTHON_LIBRARIES}
  ${OpenCV_LIBS}
  tbb 
  gflags 
  glog
)


add_executable(super_lio_node src/apps/super_lio_node.cpp)
target_link_libraries(super_lio_node
  lio
  ${catkin_LIBRARIES}
)


add_executable(relocation_node src/apps/relocation_node.cpp)
target_link_libraries(relocation_node
  lio
  ${catkin_LIBRARIES}
)

add_dependencies(lio ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
add_executable(cube_offline_node src/apps/cube_offline_node.cpp)
target_link_libraries(cube_offline_node lio ${catkin_LIBRARIES})

add_executable(test_cube_photo test/test_cube_photo.cpp src/intensity/cube_image.cpp)
target_include_directories(test_cube_photo PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(test_cube_photo ${OpenCV_LIBS} tbb)
enable_testing()
add_test(NAME cube_photo_jacobians COMMAND test_cube_photo)

add_executable(test_spherical_photo test/test_spherical_photo.cpp
  src/intensity/spherical_image.cpp src/intensity/cube_image.cpp)
target_include_directories(test_spherical_photo PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(test_spherical_photo ${OpenCV_LIBS} tbb)
add_test(NAME spherical_photo_jacobians COMMAND test_spherical_photo)

add_executable(test_information_budget test/test_information_budget.cpp src/intensity/information_budget.cpp)
target_link_libraries(test_information_budget tbb)
add_test(NAME photo_information_budget COMMAND test_information_budget)

# The comparison executable is deliberately optional and links the exact,
# unmodified COIN-LIO oracle only for the P2 parity audit. Production lio never
# links against the external checkout.
set(COIN_LIO_REF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../refs/COIN-LIO" CACHE PATH "Pinned COIN-LIO source checkout")
set(COIN_LIO_ORACLE_DEVEL "/tmp/cube_p2a_coin_oracle/devel" CACHE PATH "Isolated upstream COIN-LIO catkin devel space")
if(EXISTS "${COIN_LIO_REF_ROOT}/include/projector.h" AND EXISTS "${COIN_LIO_ORACLE_DEVEL}/lib/libcoin_lio.so")
  find_library(COIN_LIO_CV_BRIDGE_LIBRARY NAMES cv_bridge PATHS ${catkin_LIBRARY_DIRS})
  if(NOT COIN_LIO_CV_BRIDGE_LIBRARY)
    message(FATAL_ERROR "Pinned COIN oracle needs cv_bridge for parity executable")
  endif()
  add_executable(test_coin_frontend_parity test/test_coin_frontend_parity.cpp
    src/intensity/coin/coin_ouster_projector.cpp src/intensity/coin/coin_image_processor.cpp)
  target_include_directories(test_coin_frontend_parity PRIVATE "${COIN_LIO_REF_ROOT}/include" "${COIN_LIO_ORACLE_DEVEL}/include")
  target_link_libraries(test_coin_frontend_parity lio "${COIN_LIO_ORACLE_DEVEL}/lib/libcoin_lio.so" ${COIN_LIO_CV_BRIDGE_LIBRARY} ${catkin_LIBRARIES} ${PCL_LIBRARIES} ${OpenCV_LIBS})

  add_executable(test_coin_feature_parity test/test_coin_feature_parity.cpp)
  target_include_directories(test_coin_feature_parity PRIVATE "${COIN_LIO_REF_ROOT}/include" "${COIN_LIO_ORACLE_DEVEL}/include")
  target_link_libraries(test_coin_feature_parity lio "${COIN_LIO_ORACLE_DEVEL}/lib/libcoin_lio.so" ${COIN_LIO_CV_BRIDGE_LIBRARY} ${catkin_LIBRARIES} ${PCL_LIBRARIES} ${OpenCV_LIBS})
endif()

add_executable(test_coin_feature_math test/test_coin_feature_math.cpp)
target_link_libraries(test_coin_feature_math lio ${catkin_LIBRARIES} ${OpenCV_LIBS})

add_executable(test_ouster_time_support test/test_ouster_time_support.cpp)
target_link_libraries(test_ouster_time_support lio ${catkin_LIBRARIES})
add_test(NAME ouster_time_support COMMAND test_ouster_time_support)
````

### src/super_lio/include/intensity/coin/coin_acquisition.hpp

Git blob `06e434289017f336818ba1de2799d4e52eb95290`；SHA-256 `af53c385f08b55d1920445628ff14f713c72b0b3d438e38710205d3a9a8d7d51`；51 行完整文件。

````cpp
#pragma once

#include "common/ds.h"
#include <algorithm>

namespace cube::coin {

inline std::string coinFrameInputIssue(const LI2Sup::LidarData& lidar,
    const std::vector<LI2Sup::DynamicState>& history){
  if(lidar.coin_raw_points.empty())return "no_raw_ouster_samples";
  if(!lidar.imu_support_issue.empty())return lidar.imu_support_issue;
  if(history.size()<2)return "insufficient_propagated_imu_history";
  return {};
}

inline bool stateAt(const std::vector<LI2Sup::DynamicState>& history,double time,
                    Eigen::Matrix3d& R,Eigen::Vector3d& p){
  constexpr double tolerance=1e-6;
  if(history.size()<2||time<history.front().time-tolerance||
     time>history.back().time+tolerance)return false;
  time=std::clamp(time,history.front().time,history.back().time);
  auto tail=std::upper_bound(history.begin(),history.end(),time,
      [](double t,const LI2Sup::DynamicState& s){return t<s.time;});
  if(tail==history.end()){
    const auto& last=history.back();R=last.R.cast<double>();p=last.p.cast<double>();
    return R.allFinite()&&p.allFinite();
  }
  auto head=tail==history.begin()?tail:std::prev(tail);
  if(head==tail){
    auto next=std::next(tail);if(next==history.end())return false;tail=next;
  }
  const double dt=tail->time-head->time;
  if(!(dt>0.))return false;
  const double tau=time-head->time,s=tau/dt;
  Eigen::Quaterniond q0(head->R.cast<double>()),q1(tail->R.cast<double>());
  R=q0.slerp(s,q1).toRotationMatrix();
  p=head->p.cast<double>()+head->v.cast<double>()*tau+
    0.5*tail->a.cast<double>()*tau*tau;
  return R.allFinite()&&p.allFinite();
}

inline Eigen::Matrix4d lidarAcquisitionToEnd(const Eigen::Matrix3d& R_acq,
    const Eigen::Vector3d& t_acq,const Eigen::Matrix3d& R_end,
    const Eigen::Vector3d& t_end){
  Eigen::Matrix4d T=Eigen::Matrix4d::Identity();
  T.topLeftCorner<3,3>()=R_acq.transpose()*R_end;
  T.topRightCorner<3,1>()=R_acq.transpose()*(t_end-t_acq);
  return T;
}

} // namespace cube::coin
````

### src/super_lio/include/intensity/coin/coin_cubemap_representation.hpp

Git blob `880e7da30b2ab37e5aac6bbcf1a5f223347bb949`；SHA-256 `3e49bff3579b61b5e4c6ed5730385c39dba84156f91394860c27601907be509a`；23 行完整文件。

````cpp
#pragma once

#include "intensity/cube_image.hpp"
#include "intensity/coin/coin_image_processor.hpp"
#include <string>

namespace cube::coin {

// Explicit opt-in for P3-A; the frozen runner passes this environment through.
// No switch is inferred from the existing /photo controls.
std::string p3aRepresentation();
Settings p3aCubeSettings(const std::string& representation);

// Export the existing Cubemap representation into COIN's patch-image contract.
// Upstream intensity preprocessing, patch tracking, selector, and fusion stay
// shared. Filled pixels receive a depth-derived landmark, never a fabricated
// geometric observation; these points are private to the intensity channel.
CoinFrame buildCoinCubemap(CubeImage& image,const CoinFrame& calibrated,
                          const CoinOusterProjector& calibrated_projector,
                          const CoinImageSettings& settings,
                          std::vector<CoinScanPoint>& points);

} // namespace cube::coin
````

### src/super_lio/include/intensity/coin/coin_feature_manager.hpp

Git blob `6b6cf00d4787915093fa5ef6de67af1665a33235`；SHA-256 `5e65e35fd9f9cb0b386a135fe6713f4920f4a67b9d309f780ecd6897999fc331`；98 行完整文件。

````cpp
// COIN-LIO patch feature semantics reimplemented from the pinned BSD-3-Clause oracle.
#pragma once

#include "intensity/coin/coin_image_processor.hpp"
#include <Eigen/Eigenvalues>
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace cube::coin {

struct CoinFeatureSettings {
  int patch_size=5;
  double max_range=30.;
  int max_lifetime=25;
  double min_range=.7;
  int suppression_radius=10;
  int num_features=60;
  double grad_min=16.5;
  double ncc_threshold=.7075;
  int margin=10;
  double range_threshold=.2;
  double n_uninformative=25.;
  static CoinFeatureSettings fromRosParams();
  void validate() const;
};

struct CoinFeature {
  std::uint64_t id=0;
  int lifetime=1;
  Vec2 center=Vec2::Zero();
  std::vector<double> reference_intensity;
  std::vector<Vec3> points_global;
  std::vector<Vec2> current_uv;
};

struct CoinFeatureFrameStats {
  int active_before=0,active_after=0,added=0,removed=0;
  int candidates_after_nms=0,selected_centers=0;
  int rejected_projection=0,rejected_border=0,rejected_mask=0,rejected_range=0;
  int rejected_ncc=0,rejected_lifetime=0;
  double selected_gradient_sum=0.;
  std::array<double,3> selected_directional_score_sum{{0.,0.,0.}};
  int selected_metric_count=0;
  std::vector<Eigen::Vector2i> selected_center_pixels;
  std::vector<double> ncc_values;
};

struct CoinWeakDirections {
  Eigen::Vector3d contribution=Eigen::Vector3d::Zero();
  Eigen::Vector3d eigenvalues=Eigen::Vector3d::Zero();
  Eigen::Matrix3d eigenvectors=Eigen::Matrix3d::Identity();
  Eigen::Index geometry_rows=0;
  std::vector<Vec3> global;
  std::vector<Vec3> lidar;
};

class CoinFeatureManager {
 public:
  CoinFeatureManager(CoinOusterProjector projector,CoinFeatureSettings settings={});
  void update(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
              const std::vector<Vec3>& weak_directions_lidar,const Eigen::Matrix4d& T_GL,
              bool pure_gradient=false,const std::vector<Vec3>& audit_eigenvectors_lidar={});
  bool projectUndistorted(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
                          const Vec3& p_Lk,Vec3& p_Li,Vec2& uv,int& distortion_index,
                          bool round_bucket=false) const;
  const std::vector<CoinFeature>& features() const{return features_;}
  const CoinFeatureFrameStats& lastStats() const{return last_stats_;}
  static double normalizedCrossCorrelation(const std::vector<double>& reference,
                                          const std::vector<double>& current);
  static CoinWeakDirections weakDirectionsFromGeometry(const Eigen::MatrixXd& H_translation,
                                                       const Eigen::Matrix3d& R_GL,
                                                       double n_uninformative=25.);
  static std::vector<cv::Point> selectPureGradient(
      const std::vector<std::pair<double,cv::Point>>& candidates,int cap);

 private:
  void track(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,const Eigen::Matrix4d& T_GL);
  void updateSuppressionMask();
  void detect(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
              const std::vector<Vec3>& weak_directions_lidar,const Eigen::Matrix4d& T_GL,
              bool pure_gradient,const std::vector<Vec3>& audit_eigenvectors_lidar);
  void detectComplementary(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
                           const std::vector<Vec3>& weak_directions_lidar,int needed,
                           std::vector<cv::Point>& centers,bool pure_gradient);
  static double sampleBilinearFloat(const cv::Mat& image,double x,double y);

  CoinOusterProjector projector_;
  CoinFeatureSettings settings_;
  std::vector<CoinFeature> features_;
  cv::Mat suppression_mask_,margin_mask_;
  std::vector<Eigen::Vector2i> patch_offsets_;
  CoinFeatureFrameStats last_stats_;
  std::uint64_t next_id_=1;
};

} // namespace cube::coin
````

### src/super_lio/include/intensity/coin/coin_image_processor.hpp

Git blob `ee5c206b9a68d692b8c3043027f08130c6525feb`；SHA-256 `8c4dee6513e876ada3fd8f6a40c9de2c2647672cb6135b96c37b7ed59354743d`；49 行完整文件。

````cpp
// COIN-LIO calibrated Ouster intensity image pipeline reimplementation.
#pragma once

#include "intensity/coin/coin_ouster_projector.hpp"
#include <opencv2/core.hpp>
#include <cstdint>
#include <vector>

namespace cube::coin {

struct CoinImageSettings {
  bool reflectivity=false,line_removal=true,brightness_filter=true,blur=true;
  double intensity_scale=.25,min_range=.7,max_range=30.;
  int patch_size=5,erosion_margin=2;
  cv::Size brightness_window{41,7};
  std::vector<cv::Rect> masks;
  std::vector<double> highpass,lowpass;
  static CoinImageSettings fromRosParams();
};

struct CoinScanPoint {
  Vec3 point_lidar=Vec3::Zero();
  float intensity=0.f;
  float range=0.f;
  std::size_t raw_index=0;
  double offset_seconds=0;
};

struct CoinFrame {
  cv::Mat raw_intensity,intensity,range;
  cv::Mat image_index,mask,photo_u8,dx,dy;
  // For each rounded geometric projection cell: count followed by <=9 point indices.
  std::vector<int> projected_index;
  // End-of-scan LiDAR point to acquisition-time LiDAR transform lookup.
  std::vector<Eigen::Matrix4d> T_Li_Lk_vec;
  std::vector<int> vec_idx;
};

class CoinImageProcessor {
 public:
  CoinImageProcessor(CoinOusterProjector projector,CoinImageSettings settings);
  CoinFrame process(std::vector<CoinScanPoint>& points) const;
  const CoinOusterProjector& projector() const{return projector_;}
 private:
  CoinOusterProjector projector_;
  CoinImageSettings settings_;
};

} // namespace cube::coin
````

### src/super_lio/include/intensity/coin/coin_intensity_representation.hpp

Git blob `990685778648cba2ad56183fb8f780b3279341d8`；SHA-256 `5121eb75602e56ae6910b6df74d540f5e595d1e4b143bc6b9527cc6b34b852c4`；79 行完整文件。

````cpp
#pragma once

#include "intensity/intensity_representation.hpp"
#include "intensity/coin/coin_photometric_model.hpp"
#include <cmath>

namespace cube::coin {

// Adapter for the frozen COIN raw-intensity image and scalar residual. It is
// available for controlled comparisons; the production COIN call path remains
// unchanged and continues to use its calibrated acquisition-time matches.
class CoinIntensityRepresentation final : public cube::IntensityRepresentation {
 public:
  CoinIntensityRepresentation(const CoinOusterProjector& projector,const CoinFrame& frame)
      :projector_(projector),frame_(frame){}

  bool project(const Vec3& point,cube::Projection& projection) const override{
    const auto projected=projector_.project(point);
    if(!projected.in_fov)return false;
    projection=cube::Projection{};projection.face=0;projection.uv=projected.uv;
    projection.jacobian=projector_.projectionJacobian(point);
    return projection.uv.allFinite()&&projection.jacobian.allFinite();
  }

  bool sampleIntensity(const cube::Projection& projection,double& intensity) const override{
    if(!coordinatesValid(projection,false))return false;
    intensity=CoinPhotometricModel::sampleFloat(frame_.intensity,projection.uv.x(),projection.uv.y());
    return std::isfinite(intensity);
  }

  bool sampleGradient(const cube::Projection& projection,Eigen::Vector2d& gradient) const override{
    if(!coordinatesValid(projection,true))return false;
    gradient=CoinPhotometricModel::centralImageGradient(frame_.intensity,
                                                        projection.uv.x(),projection.uv.y());
    return gradient.allFinite();
  }

  bool computeResidual(const cube::Projection& projection,double reference_intensity,
                       double& residual) const override{
    double current=0.;
    if(!std::isfinite(reference_intensity)||!sampleIntensity(projection,current))return false;
    residual=current-reference_intensity;
    return std::isfinite(residual);
  }

  bool validityCheck(const cube::Projection& projection,double expected_depth,
                     double range_absolute,double range_relative,
                     cube::Sample& sample) const override{
    if(!coordinatesValid(projection,false)||frame_.range.empty()||!std::isfinite(expected_depth))return false;
    const int u=static_cast<int>(std::floor(projection.uv.x()));
    const int v=static_cast<int>(std::floor(projection.uv.y()));
    sample.depth=frame_.range.ptr<float>(v)[u];
    if(!(sample.depth>0.)||std::abs(expected_depth-sample.depth)>
       range_absolute+range_relative*sample.depth)return false;
    Eigen::Vector2d gradient;
    if(!sampleIntensity(projection,sample.value)||!sampleGradient(projection,gradient))return false;
    sample.gradient=gradient.transpose();
    return true;
  }

 private:
  bool coordinatesValid(const cube::Projection& projection,bool need_gradient) const{
    if(frame_.intensity.empty()||frame_.mask.empty()||!projection.uv.allFinite()||
       projection.face!=0||projection.uv.x()<0.||projection.uv.y()<0.||
       projection.uv.x()>=frame_.intensity.cols-1||projection.uv.y()>=frame_.intensity.rows-1)
      return false;
    if(need_gradient&&(projection.uv.x()<1.||projection.uv.y()<1.||
       projection.uv.x()+1.>=frame_.intensity.cols-1||
       projection.uv.y()+1.>=frame_.intensity.rows-1))return false;
    const int u=static_cast<int>(std::floor(projection.uv.x()));
    const int v=static_cast<int>(std::floor(projection.uv.y()));
    return frame_.mask.ptr<uchar>(v)[u]!=0;
  }

  const CoinOusterProjector& projector_;
  const CoinFrame& frame_;
};

} // namespace cube::coin
````

### src/super_lio/include/intensity/coin/coin_observation.hpp

Git blob `e852e6056e7645ff554852a416978555c47603bb`；SHA-256 `ec44a9e721a1e516c6bdf0258241594d50845a88ad4795fcb118f9b47ccfd8cc`；57 行完整文件。

````cpp
// One-shot diagnostic COIN measurement channel for the Super-LIO pose update.
#pragma once

#include "common/ds.h"
#include "intensity/coin/coin_photometric_model.hpp"
#include "intensity/coin/coin_cubemap_representation.hpp"
#include <ros/ros.h>

#include <fstream>
#include <memory>

namespace cube::coin {

class CoinObservation {
 public:
  CoinObservation(ros::NodeHandle& nh,const Eigen::Matrix4d& T_IL);
  bool enabled() const { return enabled_; }
  bool shadow() const { return shadow_; }

  void prepare(const LI2Sup::LidarData& lidar,
               const std::vector<LI2Sup::DynamicState>& history,
               const BASIC::SE3& predicted_pose);
  void add(const BASIC::SE3& pose,BASIC::M6& A,BASIC::V6& b);
  void finish(const BASIC::SE3& pose,double timestamp,
              const Eigen::MatrixXd& geometry_translation_rows);

 private:
  Eigen::Matrix4d poseMatrix(const BASIC::SE3& pose) const;

  bool enabled_=false,prepared_=false,shadow_=false;
  std::string selector_mode_="original";
  double photo_scale_=0.00095,measurement_variance_=0.001;
  double gate_g1_confidence_threshold_=-1.,gate_g2_confidence_threshold_=-1.;
  bool has_previous_weak_axis_=false;
  Eigen::Vector3d previous_weak_axis_global_=Eigen::Vector3d::UnitX();
  double previous_weak_axis_timestamp_=0.;
  Eigen::Matrix4d T_IL_=Eigen::Matrix4d::Identity();
  CoinImageSettings image_settings_;
  CoinFeatureSettings feature_settings_;
  std::unique_ptr<CoinOusterProjector> projector_;
  std::unique_ptr<CubeImage> cube_representation_;
  std::string representation_mode_="coin";
  std::ofstream representation_diagnostics_;
  std::unique_ptr<CoinImageProcessor> image_processor_;
  std::unique_ptr<CoinFeatureManager> feature_manager_;
  std::vector<CoinScanPoint> points_;
  CoinFrame frame_;
  std::ofstream diagnostics_;
  std::ofstream fusion_audit_;
  bool fusion_audited_=false;
  std::size_t scan_index_=0;
  int active_before_=0,valid_patches_=0,photo_rows_=0,motion_fallback_points_=0;
  double residual_square_sum_=0.,photo_A_norm_=0.,photo_b_norm_=0.;
  double coin_scan_end_delta_s_=0.;
};

} // namespace cube::coin
````

### src/super_lio/include/intensity/coin/coin_ouster_projector.hpp

Git blob `f3be1cfb52f0614e4e09caded0d09c3f0dc4858d`；SHA-256 `b7d59d39535ac92e93f54393fbe1d7efa1970a1650c81c9b4c957f1017170624`；51 行完整文件。

````cpp
// COIN-LIO Ouster projection reimplementation. See spec/p2/COIN_SOURCE_MAPPING.md.
#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <cstddef>
#include <vector>

namespace cube::coin {

using Vec2 = Eigen::Vector2d;
using Vec3 = Eigen::Vector3d;
using Mat23 = Eigen::Matrix<double,2,3>;

struct OusterMetadata {
  int rows=0, cols=0, u_shift=0;
  double beam_offset_mm=0;
  bool destagger=true;
  std::vector<int> pixel_shift_by_row;
  std::vector<double> beam_altitude_degrees;
  static OusterMetadata fromRosParams();
  void validate() const;
};

struct ProjectedPoint {
  bool in_fov=false;
  Vec2 uv=Vec2::Zero();
};

class CoinOusterProjector {
 public:
  explicit CoinOusterProjector(OusterMetadata metadata, int cube_resolution=0);
  const OusterMetadata& metadata() const { return metadata_; }
  int rows() const { return cube_resolution_ ? cube_resolution_ : metadata_.rows; }
  int cols() const { return cube_resolution_ ? 6*cube_resolution_ : metadata_.cols; }
  int cubeResolution() const { return cube_resolution_; }
  std::size_t indexFromPixel(int row, int col) const;
  ProjectedPoint project(const Vec3& p) const;
  Mat23 projectionJacobian(const Vec3& p) const;
  void pixelFromRawIndex(std::size_t raw_index, int& row, int& col) const;

 private:
  OusterMetadata metadata_;
  int cube_resolution_=0;
  std::vector<double> elevation_radians_;
  std::vector<int> raw_to_row_, raw_to_col_;
  Eigen::Matrix3d K_=Eigen::Matrix3d::Zero();
  double beam_offset_m_=0;
};

} // namespace cube::coin
````

### src/super_lio/include/intensity/coin/coin_photometric_model.hpp

Git blob `fba1ce0c0a7359bda4acccdbff80e0eb3ffa0050`；SHA-256 `3b6db9dd38329f8ac99b324094629585f7447f17096774fb7e394b5cbcfba67a`；41 行完整文件。

````cpp
// COIN-LIO fixed-correspondence photometric observation model.
#pragma once

#include "intensity/coin/coin_feature_manager.hpp"

namespace cube::coin {

struct CoinPhotoLinearization {
  bool valid=false;
  double residual=0.;
  Vec2 uv=Vec2::Zero();
  Vec3 point_lidar_end=Vec3::Zero();
  Vec3 point_lidar_acquisition=Vec3::Zero();
  int distortion_index=-1;
  Eigen::Matrix<double,1,6> correction_jacobian_coin=Eigen::Matrix<double,1,6>::Zero();
  Eigen::Matrix<double,1,6> correction_jacobian_super=Eigen::Matrix<double,1,6>::Zero();
};

class CoinPhotometricModel {
 public:
  static CoinPhotoLinearization linearize(const CoinFeatureManager& manager,
      const CoinOusterProjector& projector,const CoinFrame& frame,
      const std::vector<CoinScanPoint>& points,const Eigen::Matrix4d& T_GI,
      const Eigen::Matrix4d& T_IL,const Vec3& point_global,double reference_intensity,
      double min_range=.7,double max_range=30.,int margin=10);

  // Evaluate the same residual while holding the selected acquisition-time
  // point correspondence fixed. This is the finite-difference diagnostic path.
  static double evaluateFixed(const CoinOusterProjector& projector,const CoinFrame& frame,
      const std::vector<CoinScanPoint>& points,const Eigen::Matrix4d& T_GI,
      const Eigen::Matrix4d& T_IL,const Vec3& point_global,double reference_intensity,
      int distortion_index);

  static Eigen::Matrix4d applyCoinCorrection(const Eigen::Matrix4d& T_GI,
      const Eigen::Matrix<double,6,1>& delta_position_then_rotation);

  static double sampleFloat(const cv::Mat& image,double x,double y);
  static Eigen::Vector2d centralImageGradient(const cv::Mat& image,double x,double y);
};

} // namespace cube::coin
````

### src/super_lio/include/intensity/coin/super_degeneracy_gate.hpp

Git blob `397e734c2df780755127c61de6476389a2f004be`；SHA-256 `fbf4e67c85da002cb6a5ecfca6c05a14bf0e973572d8129de9c568e317446584`；54 行完整文件。

````cpp
#pragma once

#include "intensity/coin/coin_feature_manager.hpp"
#include <algorithm>
#include <cmath>

namespace cube::coin {

struct SuperDegeneracySignal {
  bool valid=false;
  double lambda1_over_lambda2=0.;
  double lambda1_over_lambda3=0.;
  double weakest_axis_stability=0.;
  double anisotropy_confidence=0.;
  double eigengap_confidence=0.;
  double confidence=0.;
};

class SuperDegeneracyGate {
 public:
  // Confidence combines a weak axis separated from the next axis, overall
  // translation anisotropy, and sign-invariant temporal axis stability.
  static SuperDegeneracySignal measure(const CoinWeakDirections& geometry,
      const Eigen::Vector3d& previous_weak_axis_global,bool has_previous_axis){
    SuperDegeneracySignal signal;
    const Eigen::Vector3d eigenvalues=geometry.eigenvalues;
    if(geometry.geometry_rows<=3||!eigenvalues.allFinite()||eigenvalues[2]<=1e-12||
       !geometry.eigenvectors.allFinite())
      return signal;
    signal.valid=true;
    const double lambda1=std::max(0.,eigenvalues[0]);
    const double lambda2=std::max(0.,eigenvalues[1]);
    const double lambda3=std::max(0.,eigenvalues[2]);
    signal.lambda1_over_lambda2=lambda2>1e-12?lambda1/lambda2:1.;
    signal.lambda1_over_lambda3=lambda1/lambda3;
    signal.anisotropy_confidence=std::clamp(1.-signal.lambda1_over_lambda3,0.,1.);
    signal.eigengap_confidence=std::clamp((lambda2-lambda1)/lambda3,0.,1.);
    if(has_previous_axis&&previous_weak_axis_global.allFinite()&&
       previous_weak_axis_global.norm()>1e-12){
      signal.weakest_axis_stability=std::clamp(
          std::abs(geometry.eigenvectors.col(0).normalized().dot(previous_weak_axis_global.normalized())),
          0.,1.);
    }
    signal.confidence=signal.anisotropy_confidence*signal.eigengap_confidence*
                      signal.weakest_axis_stability;
    return signal;
  }

  static bool activate(const SuperDegeneracySignal& signal,double threshold){
    return signal.valid&&std::isfinite(threshold)&&threshold>=0.&&signal.confidence>=threshold;
  }
};

} // namespace cube::coin
````

### src/super_lio/include/intensity/cube_image.hpp

Git blob `db16f7a72630c0e5c3090dd2c25c6ec94dcc8415`；SHA-256 `114e6cb119b4a75f496ead7ef22d8211ca693bcc9b3c1ab90acca8963d81bf93`；75 行完整文件。

````cpp
// CUBE-LIO independent implementation, GPL-3.0-or-later.
#pragma once
#include "cube_projector.hpp"
#include "intensity/intensity_representation.hpp"
#include <opencv2/core.hpp>
#include <vector>

namespace cube {
using ScanPoint=RepresentationPoint;
struct Settings {
  bool enable=false,idw_enable=true;
  bool build_igm=true;
  int resolution=96,idw_radius=3,idw_k=6,idw_min_support=3;
  double idw_power=2.,range_absolute=.3,range_relative=.02,gaussian_sigma=1.;
  int max_features=1200,max_lifetime=10,suppression_radius=2,normalization_frames=20;
  double high_response=5.,weight=1.,sigma_min=1.,robust_gate=4.685,huber_delta=1.345;
  void validate() const;
};
struct Face {
  cv::Mat intensity,depth,igm,grad_u,grad_v;
  cv::Mat raw_mask,mask,igm_mask;
  std::vector<int> point_index;
};
class CubeImage final : public RasterIntensityRepresentation {
 public:
  explicit CubeImage(const Settings& cfg,
                     MeasurementChannel channel=MeasurementChannel::IntensityGradientMagnitude);
  void build(const std::vector<ScanPoint>& points) override;
  bool project(const Vec3& point,Projection& projection) const override;
  bool sampleIntensity(const Projection& projection,double& intensity) const override;
  bool sampleGradient(const Projection& projection,Eigen::Vector2d& gradient) const override;
  bool computeResidual(const Projection& projection,double reference,double& residual) const override;
  bool validityCheck(const Projection& projection,double expected_depth,double range_absolute,
                     double range_relative,Sample& sample) const override;
  bool sample(const Projection& q,Sample& out) const;
  const Face& face(int i) const {return faces_[i];}
  int chartCount() const override {return 6;}
  int coordinateResolution() const override {return cfg_.resolution;}
  int chartWidth(int chart) const override {return chart>=0&&chart<6?cfg_.resolution:0;}
  int chartHeight(int chart) const override {return chart>=0&&chart<6?cfg_.resolution:0;}
  bool wrapsHorizontally(int) const override{return false;}
  const std::vector<int>& candidatePointIndices() const override{return candidate_points_;}
  double rasterMilliseconds() const override{return raster_ms;}
  double interpolationMilliseconds() const override{return idw_ms;}
  double featureChannelMilliseconds() const override{return igm_ms;}
  int rawPixelCount() const override{return raw_pixels;}
  int filledPixelCount() const override{return filled_pixels;}
  int validFeaturePixelCount() const override{return valid_igm_pixels;}
  CubeProjector projector;
  double raster_ms=0,idw_ms=0,igm_ms=0;
  int raw_pixels=0,filled_pixels=0,valid_igm_pixels=0;
 private:
  Settings cfg_;
  MeasurementChannel channel_;
  std::array<Face,6> faces_;
  std::vector<int> candidate_points_;
  void fill(Face& face);
  void gradients(Face& face);
};
// Right-local rotation, global translation: R_new=R Exp(w), t_new=t+dt.
// The state is T_GB and the LiDAR extrinsic is T_BL.
inline Vec3 landmarkInLidar(const Vec3& world,const Mat3& R,const Vec3& t,
                           const Mat3& R_BL,const Vec3& t_BL) {
  return R_BL.transpose()*(R.transpose()*(world-t)-t_BL);
}
inline Row6 residualJacobian(const Vec3& world,const Mat3& R,const Vec3& t,
                            const Mat3& R_BL,const Vec3& t_BL,
                            const Projection& q,const Sample& sample) {
  const Vec3 p_B=R.transpose()*(world-t);
  Eigen::Matrix<double,3,6> chain;
  chain.leftCols<3>()=R_BL.transpose()*hat(p_B);
  chain.rightCols<3>()=-R_BL.transpose()*R.transpose();
  return sample.gradient*q.jacobian*chain;
}
} // namespace cube
````

### src/super_lio/include/intensity/cube_projector.hpp

Git blob `e93414e3ef86e1da333715f48b1bd07814b247f0`；SHA-256 `6f1ee9db424e7a561d95d680e59b8d3d2539f61fcf3b69f2090a535a4aae49e2`；51 行完整文件。

````cpp
// CUBE-LIO independent implementation, GPL-3.0-or-later.
#pragma once
#include "intensity/intensity_representation.hpp"
#include <Eigen/Core>
#include <array>
#include <cmath>
#include <stdexcept>

namespace cube {
using Vec3 = Eigen::Vector3d;
using Mat3 = Eigen::Matrix3d;
using Row6 = Eigen::Matrix<double,1,6>;
using Mat6 = Eigen::Matrix<double,6,6>;
using Vec6 = Eigen::Matrix<double,6,1>;
inline Mat3 hat(const Vec3& v) {
  Mat3 a; a << 0,-v.z(),v.y(),v.z(),0,-v.x(),-v.y(),v.x(),0; return a;
}
class CubeProjector {
 public:
  explicit CubeProjector(int n) : n_(n), f_((n-1)*.5) {
    if(n<8) throw std::invalid_argument("cubemap resolution <8");
  }
  int resolution() const { return n_; }
  // Face id: +X,-X,+Y,-Y,+Z,-Z. Ties choose X before Y before Z.
  Projection project(const Vec3& p) const {
    Projection out;
    if(!p.allFinite() || p.squaredNorm()<1e-16) return out;
    const Vec3 abs=p.cwiseAbs();
    int axis=0;
    if(abs.y()>abs.x()) axis=1;
    if(abs.z()>abs[axis]) axis=2;
    out.face=2*axis+(p[axis]<0);
    Vec3 d=Vec3::Zero(),a=Vec3::Zero(),b=Vec3::Zero();
    d[axis]=p[axis]<0?-1.:1.;
    // Local image axes form a consistent right-handed face frame.
    if(axis==0) {a.y()=d[axis];b.z()=1.;}
    if(axis==1) {a.x()=-d[axis];b.z()=1.;}
    if(axis==2) {a.x()=1.;b.y()=d[axis];}
    double depth=d.dot(p),x=a.dot(p),y=b.dot(p);
    out.uv << f_*(x/depth+1),f_*(y/depth+1);
    out.jacobian.row(0)=f_*(a/depth-x*d/(depth*depth)).transpose();
    out.jacobian.row(1)=f_*(b/depth-y*d/(depth*depth)).transpose();
    double second=0.;for(int k=0;k<3;++k)if(k!=axis)second=std::max(second,abs[k]);
    out.seam=(abs[axis]-second)<=1e-8*abs[axis];
    return out;
  }
 private:
  int n_;
  double f_;
};
} // namespace cube
````

### src/super_lio/include/intensity/intensity_representation.hpp

Git blob `458207091ee23ce6d3149530c70cf64a9a3bc841`；SHA-256 `9a1ac3bedaee50a53ad5f106e955bf84c23ccb082069fb8c03889c776af4367d`；64 行完整文件。

````cpp
#pragma once

#include <Eigen/Core>
#include <vector>

namespace cube {

// A coordinate in a projection chart. face is a cubemap face for cubemaps and
// zero for single-chart projections. Jacobians map sensor-frame XYZ to pixels.
struct Projection {
  int face=-1;
  Eigen::Vector2d uv=Eigen::Vector2d::Zero();
  Eigen::Matrix<double,2,3> jacobian=Eigen::Matrix<double,2,3>::Zero();
  bool seam=false;
};

struct Sample {
  double value=0.,depth=0.;
  Eigen::RowVector2d gradient=Eigen::RowVector2d::Zero();
};

enum class MeasurementChannel { RawIntensity, IntensityGradientMagnitude };

struct RepresentationPoint {
  Eigen::Vector3d p=Eigen::Vector3d::Zero();
  double intensity=0.;
};

// Projection-specific intensity access. Residuals retain the same scalar
// current-minus-reference convention; implementations choose the sampled
// channel when constructed. COIN implements its existing raw-intensity model.
class IntensityRepresentation {
 public:
  virtual ~IntensityRepresentation()=default;
  virtual bool project(const Eigen::Vector3d& point,Projection& projection) const=0;
  virtual bool sampleIntensity(const Projection& projection,double& intensity) const=0;
  virtual bool sampleGradient(const Projection& projection,Eigen::Vector2d& gradient) const=0;
  virtual bool computeResidual(const Projection& projection,double reference_intensity,
                               double& residual) const=0;
  virtual bool validityCheck(const Projection& projection,double expected_depth,
                             double range_absolute,double range_relative,
                             Sample& sample) const=0;
};

// Per-scan raster implementations add construction and deterministic feature
// candidate enumeration without putting image lifecycle into the estimator.
class RasterIntensityRepresentation : public IntensityRepresentation {
 public:
  virtual void build(const std::vector<RepresentationPoint>& points)=0;
  virtual int chartCount() const=0;
  virtual int coordinateResolution() const=0;
  virtual int chartWidth(int chart) const=0;
  virtual int chartHeight(int chart) const=0;
  virtual bool wrapsHorizontally(int chart) const=0;
  virtual const std::vector<int>& candidatePointIndices() const=0;
  virtual double rasterMilliseconds() const=0;
  virtual double interpolationMilliseconds() const=0;
  virtual double featureChannelMilliseconds() const=0;
  virtual int rawPixelCount() const=0;
  virtual int filledPixelCount() const=0;
  virtual int validFeaturePixelCount() const=0;
};

} // namespace cube
````

### src/super_lio/include/intensity/photo_observation.hpp

Git blob `f7bffdba766f0f8ccefb354c93731bc787656382`；SHA-256 `7208176982a2f2461b48d0cd1972958b19d778216a6abe4c076f7a91460e94a2`；71 行完整文件。

````cpp
// Independent GPLv3 implementation. COIN-LIO lifecycle design inspiration:
// Patrick Pfreundschuh, COIN-LIO main 76729cc4 (BSD-3-Clause). No source copied.
#pragma once
#include "cube_image.hpp"
#include "information_budget.hpp"
#include "common/ds.h"
#include <fstream>
#include <chrono>
#include <memory>
#include <string>
#include <ros/ros.h>

namespace cube {
struct Feature {
  Vec3 world;
  double reference=0,birth_time=0;
  size_t birth_frame=0;
  int birth_face=-1;
};
struct PhotoTerms {
  Mat6 A=Mat6::Zero();
  Vec6 b=Vec6::Zero();
  int valid=0,fov=0,invalid=0,range=0,outlier=0;
  double mean=0,rms=0,median=0;
};
class PhotoObservation {
 public:
  explicit PhotoObservation(ros::NodeHandle& nh);
  bool enabled() const{return cfg_.enable;}
  bool needsGeometryRows() const{return cfg_.enable&&selector_mode_=="weakest";}
  void prepare(const LI2Sup::MeasureGroup& measures,
               const std::vector<LI2Sup::DynamicState>& history,const BASIC::SE3& predicted);
  void add(const BASIC::SE3& pose,BASIC::M6& A,BASIC::V6& b,const BASIC::M6& prior_covariance);
  void finish(const BASIC::SE3& pose,double timestamp,
              const Eigen::MatrixXd& geometry_translation_rows);
 private:
  Settings cfg_;
  std::string projection_name_="cubemap",measurement_name_="igm",selector_mode_="all";
  double weakest_gate_threshold_=0.31913064578672057,weakest_gate_confidence_=0.;
  bool weakest_gate_active_=false,has_previous_weak_axis_=false;
  Eigen::Vector3d previous_weak_axis_global_=Eigen::Vector3d::UnitX();
  double previous_weak_axis_timestamp_=0.;
  InformationPolicy policy_=InformationPolicy::C0;
  bool audit_enabled_=false;
  std::ofstream audit_;
  // P3-R attribution controls. Both are off in the production/default path.
  bool time_audit_enabled_=false,history_supported_only_=false;
  std::ofstream time_audit_;
  int audit_iteration_=0;
  BASIC::SE3 predicted_pose_;
  std::vector<ResidualContribution> auditRows(const BASIC::SE3& pose)const;
  void writeAudit(const BASIC::SE3& pose,const BASIC::M6& covariance,
                  const Vec6& geometry_b,const std::vector<ResidualContribution>& rows);
  std::unique_ptr<RasterIntensityRepresentation> image_;
  std::vector<ScanPoint> points_;
  std::vector<Feature> features_;
  std::vector<double> early_residuals_;
  Mat3 R_BL_;
  Vec3 t_BL_;
  size_t frame_=0;
  bool frozen_=false,frame_supported_=false;
  double sigma_=1,deskew_ms_=0,photo_ms_=0,update_ms_=0,replenish_ms_=0;
  std::chrono::steady_clock::time_point update_start_;
  std::ofstream diagnostics_;
  PhotoTerms last_;
  Mat6 geometry_=Mat6::Zero();
  PhotoTerms observe(const BASIC::SE3& pose,bool weighted,std::vector<double>* residuals=nullptr)const;
  void replenish(const BASIC::SE3& pose,double timestamp,
                 const Eigen::MatrixXd& geometry_translation_rows);
};
} // namespace cube
````

### src/super_lio/include/ros/ouster_time_support.hpp

Git blob `422f29fcc4518e3127e98c507def5d5cebc1e2bd`；SHA-256 `fa8bdf31920484496ce5f0fbf32739f367ad04ea9186b0107f291c8d6efb72d2`；78 行完整文件。

````cpp
#pragma once

#include "common/ds.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <string>

namespace LI2Sup {

struct OusterScanTimeSupport {
  bool has_valid_raw=false;
  std::size_t valid_raw_points=0;
  double max_offset_s=0.;
};

// Match the pinned COIN Ouster raw-point eligibility: reject NaN coordinates
// and ranges below /preprocess/blind, then take the maximum raw point.t.
template<class Points>
OusterScanTimeSupport ousterScanTimeSupport(const Points& points,double blind){
  OusterScanTimeSupport result;
  for(const auto& pt:points){
    if(std::isnan(pt.x)||std::isnan(pt.y)||std::isnan(pt.z))continue;
    const double range=std::sqrt(double(pt.x)*pt.x+double(pt.y)*pt.y+double(pt.z)*pt.z);
    if(range<blind)continue;
    ++result.valid_raw_points;
    result.has_valid_raw=true;
    result.max_offset_s=std::max(result.max_offset_s,pt.t*1e-9);
  }
  return result;
}

inline bool imuHasReachedScanEnd(double latest_imu_time,double scan_end){
  return latest_imu_time>=scan_end;
}

struct OusterImuWindow {
  std::deque<IMUData> samples;
  IMUData end_sample;
  bool has_end_sample=false;
  double bracket_span_s=0.;
  std::string issue;
};

// Retain the right IMU sample in the buffer. Interpolate at scan end only
// when real samples bracket it within the allowed IMU gap; never extrapolate.
inline OusterImuWindow collectOusterImuWindow(std::deque<IMUData>& buffer,
    double scan_end,const IMUData* previous_end,double max_bracket_span_s=0.05){
  OusterImuWindow result;
  while(!buffer.empty()&&buffer.front().secs<=scan_end){
    result.samples.push_back(buffer.front());
    buffer.pop_front();
  }
  if(!result.samples.empty()&&result.samples.back().secs==scan_end){
    result.end_sample=result.samples.back();
    result.has_end_sample=true;
    return result;
  }
  const IMUData* left=result.samples.empty()?previous_end:&result.samples.back();
  if(!left){result.issue="missing_imu_left_bracket";return result;}
  if(buffer.empty()){result.issue="missing_imu_right_bracket";return result;}
  const auto& right=buffer.front();
  result.bracket_span_s=right.secs-left->secs;
  if(!(left->secs<=scan_end&&scan_end<right.secs&&
       result.bracket_span_s>0.&&result.bracket_span_s<=max_bracket_span_s)){
    result.issue="imu_bracket_gap_or_order_invalid";
    return result;
  }
  const double alpha=(scan_end-left->secs)/result.bracket_span_s;
  result.end_sample.secs=scan_end;
  result.end_sample.acc=left->acc+(right.acc-left->acc)*alpha;
  result.end_sample.gyr=left->gyr+(right.gyr-left->gyr)*alpha;
  result.samples.push_back(result.end_sample);
  result.has_end_sample=true;
  return result;
}

} // namespace LI2Sup
````

### src/super_lio/src/apps/cube_offline_node.cpp

Git blob `d180b66e023ee69a309e838adc03d8f41f39cf20`；SHA-256 `f3810e5a72701133a1a92b28de56d605a33b8db78a0c560853264095a03d4382`；90 行完整文件。

````cpp
// CUBE-LIO replay adapter, GPL-3.0-or-later. Estimator calls remain upstream.
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sys/resource.h>
#include <tbb/task_scheduler_init.h>
#include <rosbag/bag.h>
#include <rosbag/view.h>
#include "lio/super_lio.h"

class RecordedLIO : public LI2Sup::SuperLIO {
 public:
  explicit RecordedLIO(const std::string& dir) : trajectory_(dir + "/trajectory.tum") {
    if (!trajectory_) throw std::runtime_error("cannot write trajectory");
    trajectory_ << std::setprecision(17);
  }
  size_t frames = 0;
 protected:
  void Output() override {
    const auto state = kf_->GetNavState();
    const auto q = state.R.coeffs();
    trajectory_ << state.timestamp << ' ' << state.p.transpose() << ' '
                << q.transpose() << '\n';
    ++frames;
    SuperLIO::Output();
  }
 private:
  std::ofstream trajectory_;
};

int main(int argc, char** argv) {
  ros::init(argc, argv, "cube_offline");
  ros::NodeHandle nh;
  LI2Sup::LoadParamFromRos(nh);
  std::string bag_path, dir;
  nh.getParam("/lio/offline/bag", bag_path);
  nh.getParam("/lio/offline/out_dir", dir);
  int threads = 32;
  nh.param("/lio/offline/threads", threads, 32);
  if (threads < 1) throw std::invalid_argument("offline TBB thread count must be positive");
  tbb::task_scheduler_init scheduler(threads);
  auto wrapper = std::make_shared<LI2Sup::ROSWrapper>();
  RecordedLIO lio(dir);
  lio.setROSWrapper(wrapper);
  lio.init();
  rosbag::Bag bag(bag_path, rosbag::bagmode::Read);
  rosbag::View view(bag, rosbag::TopicQuery({LI2Sup::g_lidar_topic, LI2Sup::g_imu_topic}));
  if (!view.size()) throw std::runtime_error("no selected bag messages");
  struct rusage usage_before;
  getrusage(RUSAGE_SELF, &usage_before);
  const auto start = std::chrono::steady_clock::now();
  size_t lidar_count = 0, imu_count = 0;
  for (const auto& entry : view) {
    if (entry.getTopic() == LI2Sup::g_imu_topic) {
      auto m = entry.instantiate<sensor_msgs::Imu>();
      if (!m) throw std::runtime_error("IMU type mismatch");
      wrapper->replay(m);
      ++imu_count;
    } else if (LI2Sup::g_lidar_type == LI2Sup::LID_TYPE::LIVOX) {
      auto m = entry.instantiate<livox_ros_driver::CustomMsg>();
      if (!m) throw std::runtime_error("Livox type mismatch");
      wrapper->replay(m);
      ++lidar_count;
    } else {
      auto m = entry.instantiate<sensor_msgs::PointCloud2>();
      if (!m) throw std::runtime_error("PointCloud2 type mismatch");
      wrapper->replay(m);
      ++lidar_count;
    }
    lio.process();
  }
  for (int i = 0; i < 5; ++i) lio.process();
  const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  const auto cpu_seconds = [](const timeval& value) {
    return static_cast<double>(value.tv_sec) + static_cast<double>(value.tv_usec) / 1e6;
  };
  const double cpu_user = cpu_seconds(usage.ru_utime) - cpu_seconds(usage_before.ru_utime);
  const double cpu_system = cpu_seconds(usage.ru_stime) - cpu_seconds(usage_before.ru_stime);
  std::ofstream metrics(dir + "/run.json");
  metrics << std::setprecision(17);
  metrics << "{\"lidar_read\":" << lidar_count << ",\"imu_read\":" << imu_count
          << ",\"frames\":" << lio.frames << ",\"wall_processing_s\":" << wall
          << ",\"bag_duration_s\":" << (view.getEndTime() - view.getBeginTime()).toSec()
          << ",\"cpu_user_s\":" << cpu_user << ",\"cpu_system_s\":" << cpu_system
          << ",\"peak_rss_kb\":" << usage.ru_maxrss << ",\"tbb_threads\":" << threads << "}\n";
  lio.printTimeRecord();
  ros::shutdown();
}
````

### src/super_lio/src/intensity/coin/coin_cubemap_representation.cpp

Git blob `641c53d0ffc7c6eaa0db04bdf92ee49aec20b86c`；SHA-256 `d591bfb10c8bbfa3fdf6c52a706b896e5623ac27c4f1e8b2425fb6c92674d258`；100 行完整文件。

````cpp
#include "intensity/coin/coin_cubemap_representation.hpp"
#include <ros/ros.h>
#include <opencv2/imgproc.hpp>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace cube::coin {

std::string p3aRepresentation(){
  const char* value=std::getenv("CUBE_P3A_REPRESENTATION");
  const std::string mode=value?value:"coin";
  if(mode!="coin" && mode!="cube_raw_no_idw" && mode!="cube_raw_idw" && mode!="cube_igm_idw")
    throw std::invalid_argument("unknown CUBE_P3A_REPRESENTATION: "+mode);
  return mode;
}

Settings p3aCubeSettings(const std::string& representation){
  Settings cfg;ros::NodeHandle nh;
  nh.param("/cubemap/resolution",cfg.resolution,96);
  nh.param("/cubemap/idw_radius",cfg.idw_radius,3);
  nh.param("/cubemap/idw_k",cfg.idw_k,6);
  nh.param("/cubemap/idw_min_support",cfg.idw_min_support,3);
  nh.param("/cubemap/idw_power",cfg.idw_power,2.);
  nh.param("/cubemap/range_absolute",cfg.range_absolute,.3);
  nh.param("/cubemap/range_relative",cfg.range_relative,.02);
  nh.param("/cubemap/gaussian_sigma",cfg.gaussian_sigma,1.);
  cfg.idw_enable=representation!="cube_raw_no_idw";
  cfg.build_igm=representation=="cube_igm_idw";
  cfg.validate();return cfg;
}

namespace {
Vec3 pixelRay(int face,int u,int v,int n){
  const double x=2.*u/(n-1)-1.,y=2.*v/(n-1)-1.;
  const int axis=face/2;const double sign=face%2?-1.:1.;
  Vec3 p=Vec3::Zero();p[axis]=sign;
  if(axis==0){p.y()=sign*x;p.z()=y;}
  if(axis==1){p.x()=-sign*x;p.z()=y;}
  if(axis==2){p.x()=x;p.y()=sign*y;}
  return p.normalized();
}
}

CoinFrame buildCoinCubemap(CubeImage& image,const CoinFrame& calibrated,
    const CoinOusterProjector& calibrated_projector,const CoinImageSettings& settings,
    std::vector<CoinScanPoint>& points){
  std::vector<RepresentationPoint> input;input.reserve(points.size());
  // COIN's acquisition-pixel intensity preprocessing is common to all arms.
  // The point coordinates have already passed the identical support/deskew path.
  for(const auto& point:points){
    int row,col;calibrated_projector.pixelFromRawIndex(point.raw_index,row,col);
    bool masked=false;
    for(const auto& mask:settings.masks)if(mask.contains(cv::Point(col,row))){masked=true;break;}
    input.push_back({point.point_lidar,masked?std::numeric_limits<double>::quiet_NaN():
                     calibrated.intensity.ptr<float>(row)[col]});
  }
  image.build(input);
  const int n=image.coordinateResolution();
  CoinFrame frame;
  frame.intensity=cv::Mat::zeros(n,6*n,CV_32F);
  frame.range=cv::Mat::zeros(n,6*n,CV_32F);
  frame.mask=cv::Mat::zeros(n,6*n,CV_8U);
  frame.image_index=cv::Mat::ones(n,6*n,CV_32S)*(-1);
  frame.dx=cv::Mat::zeros(n,6*n,CV_32F);frame.dy=frame.dx.clone();
  for(int face=0;face<6;++face){
    const auto& source=image.face(face);const cv::Rect roi(face*n,0,n,n);
    const bool igm=!source.igm.empty();
    (igm?source.igm:source.intensity).convertTo(frame.intensity(roi),CV_32F);
    source.depth.convertTo(frame.range(roi),CV_32F);
    cv::Mat mask=(igm?source.igm_mask:source.mask).clone();
    for(int v=0;v<n;++v)for(int u=0;u<n;++u){
      if(!mask.at<uchar>(v,u))continue;
      const double range=source.depth.at<double>(v,u);
      if(range<settings.min_range || range>settings.max_range){mask.at<uchar>(v,u)=0;continue;}
      int index=source.point_index[v*n+u];
      if(index<0){
        CoinScanPoint point;point.point_lidar=range*pixelRay(face,u,v,n);
        point.range=range;point.intensity=frame.intensity.at<float>(v,face*n+u);
        index=static_cast<int>(points.size());points.push_back(point);
      }
      frame.image_index.at<int>(v,face*n+u)=index;
    }
    // The original COIN patch/erosion settings also apply at each chart border.
    const int erosion=settings.patch_size+settings.erosion_margin;
    cv::erode(mask,frame.mask(roi),cv::Mat::ones(erosion,erosion,CV_32F),
              cv::Point(-1,-1),1,cv::BORDER_CONSTANT,cv::Scalar(0));
    cv::Mat kernel_dx=(cv::Mat_<float>(1,3)<<-.5f,0.f,.5f);
    cv::Mat kernel_dy=kernel_dx.t();
    cv::filter2D(frame.intensity(roi),frame.dx(roi),CV_32F,kernel_dx,cv::Point(-1,-1),0,cv::BORDER_CONSTANT);
    cv::filter2D(frame.intensity(roi),frame.dy(roi),CV_32F,kernel_dy,cv::Point(-1,-1),0,cv::BORDER_CONSTANT);
  }
  frame.raw_intensity=frame.intensity.clone();frame.intensity.convertTo(frame.photo_u8,CV_8U);
  frame.T_Li_Lk_vec.push_back(Eigen::Matrix4d::Identity());
  // vec_idx all-zero: frame images and landmarks are both at scan end.
  frame.vec_idx.assign(points.size(),0);
  return frame;
}

} // namespace cube::coin
````

### src/super_lio/src/intensity/coin/coin_feature_manager.cpp

Git blob `7adab96f822d04dc7d1e20b049508e78383a15fb`；SHA-256 `aa4c83f8e51fcc17c571bfd242b23383f10c03520b6352d4ef3b87d9ae2d693f`；361 行完整文件。

````cpp
// COIN-LIO patch feature semantics reimplemented from the pinned BSD-3-Clause oracle.
#include "intensity/coin/coin_feature_manager.hpp"
#include <ros/ros.h>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace cube::coin {

CoinFeatureSettings CoinFeatureSettings::fromRosParams(){
  ros::NodeHandle nh;CoinFeatureSettings s;
  nh.param("image/patch_size",s.patch_size,5);
  nh.param("image/max_range",s.max_range,30.);
  nh.param("image/max_lifetime",s.max_lifetime,25);
  nh.param("image/min_range",s.min_range,.7);
  nh.param("image/suppression_radius",s.suppression_radius,10);
  nh.param("image/num_features",s.num_features,60);
  nh.param("image/grad_min",s.grad_min,16.5);
  nh.param("image/ncc_threshold",s.ncc_threshold,.7075);
  nh.param("image/margin",s.margin,10);
  nh.param("image/range_threshold",s.range_threshold,.2);
  nh.param("filter/n_uninformative",s.n_uninformative,25.);
  s.validate();return s;
}

void CoinFeatureSettings::validate() const{
  if(patch_size<1||patch_size%2==0||max_range<=min_range||min_range<0||max_lifetime<1||
     suppression_radius<0||num_features<1||grad_min<0||ncc_threshold<-1||ncc_threshold>1||
     margin<0||range_threshold<0||n_uninformative<0)
    throw std::invalid_argument("invalid COIN feature settings");
}

CoinFeatureManager::CoinFeatureManager(CoinOusterProjector projector,CoinFeatureSettings settings)
  :projector_(std::move(projector)),settings_(std::move(settings)){
  settings_.validate();
  margin_mask_=cv::Mat::zeros(projector_.rows(),projector_.cols(),CV_8UC1);
  const cv::Rect roi(settings_.margin,settings_.margin,
                     projector_.cols()-2*settings_.margin,projector_.rows()-2*settings_.margin);
  if(roi.width<=0||roi.height<=0)throw std::invalid_argument("COIN feature margin removes the full image");
  margin_mask_(roi)=255;
  if(projector_.cubeResolution()){
    margin_mask_.setTo(0);
    const int n=projector_.cubeResolution(),m=settings_.margin;
    for(int face=0;face<6;++face)
      margin_mask_(cv::Rect(face*n+m,m,n-2*m,n-2*m))=255;
  }
  const int half=settings_.patch_size/2;
  patch_offsets_.reserve(settings_.patch_size*settings_.patch_size);
  // The oracle builds offsets as (i,j), then applies i to image x and j to image y.
  // Store offsets in row/column order while preserving that exact patch traversal.
  for(int i=-half;i<=half;++i)for(int j=-half;j<=half;++j)
    patch_offsets_.emplace_back(j,i);
}

double CoinFeatureManager::normalizedCrossCorrelation(const std::vector<double>& reference,
                                                       const std::vector<double>& current){
  if(reference.size()!=current.size()||reference.empty())return std::numeric_limits<double>::quiet_NaN();
  double ref_mean=0,current_mean=0;
  for(std::size_t i=0;i<reference.size();++i){ref_mean+=reference[i];current_mean+=current[i];}
  ref_mean/=reference.size();current_mean/=current.size();
  double numerator=0,denom_ref=0,denom_current=0;
  for(std::size_t i=0;i<reference.size();++i){
    const double a=reference[i]-ref_mean,b=current[i]-current_mean;
    numerator+=a*b;denom_ref+=a*a;denom_current+=b*b;
  }
  return numerator/std::sqrt(denom_ref*denom_current);
}

CoinWeakDirections CoinFeatureManager::weakDirectionsFromGeometry(const Eigen::MatrixXd& H_translation,
                                                                   const Eigen::Matrix3d& R_GL,
                                                                   double n_uninformative){
  CoinWeakDirections result;
  result.geometry_rows=H_translation.rows();
  if(H_translation.cols()!=3)throw std::invalid_argument("COIN weak-direction Jacobian must have 3 translation columns");
  if(H_translation.rows()>3){
    const Eigen::Matrix3d hth=H_translation.transpose()*H_translation;
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> solver(hth);
    if(solver.info()!=Eigen::Success)throw std::runtime_error("COIN geometry eigenvector calculation failed");
    result.eigenvalues=solver.eigenvalues().cwiseMax(0.);
    result.eigenvectors=solver.eigenvectors();
    for(Eigen::Index i=0;i<H_translation.rows();++i){
      Vec3 normalized_row=H_translation.row(i).transpose();normalized_row.normalize();
      for(int d=0;d<3;++d){
        const float dot=static_cast<float>(std::abs(normalized_row.dot(result.eigenvectors.col(d))));
        if(dot>.5f)result.contribution(d)+=dot;
      }
    }
    for(int d=0;d<3;++d)if(result.contribution(d)<n_uninformative)
      result.global.push_back(result.eigenvectors.col(d));
  }
  for(const auto& direction:result.global)result.lidar.push_back(R_GL.transpose()*direction);
  if(result.lidar.empty()){
    result.lidar.emplace_back(1,0,0);result.lidar.emplace_back(0,1,0);result.lidar.emplace_back(0,0,1);
  }
  return result;
}

std::vector<cv::Point> CoinFeatureManager::selectPureGradient(
    const std::vector<std::pair<double,cv::Point>>& candidates,int cap){
  if(cap<=0)return {};
  auto ranked=candidates;
  std::sort(ranked.begin(),ranked.end(),[](const auto& a,const auto& b){
    if(a.first!=b.first)return a.first>b.first;
    if(a.second.y!=b.second.y)return a.second.y<b.second.y;
    return a.second.x<b.second.x;
  });
  std::vector<cv::Point> result;
  result.reserve(std::min(cap,static_cast<int>(ranked.size())));
  for(const auto& item:ranked){
    result.push_back(item.second);
    if(static_cast<int>(result.size())==cap)break;
  }
  return result;
}

double CoinFeatureManager::sampleBilinearFloat(const cv::Mat& image,double x,double y){
  x=std::clamp(x,0.,static_cast<double>(image.cols-1));
  y=std::clamp(y,0.,static_cast<double>(image.rows-1));
  const int x0=static_cast<int>(x),x1=x0+1,y0=static_cast<int>(y),y1=y0+1;
  const double ax=x-std::floor(x),ay=y-std::floor(y);
  const double value=(1-ax)*(1-ay)*image.ptr<float>(y0)[x0]+ax*(1-ay)*image.ptr<float>(y0)[x1]+
                     (1-ax)*ay*image.ptr<float>(y1)[x0]+ax*ay*image.ptr<float>(y1)[x1];
  return static_cast<float>(value);
}

bool CoinFeatureManager::projectUndistorted(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
                                             const Vec3& p_Lk,Vec3& p_Li,Vec2& uv,int& distortion_index,
                                             bool round_bucket) const{
  ProjectedPoint projected=projector_.project(p_Lk);
  if(!projected.in_fov)return false;
  // Cubemap pixels already describe the supported, deskewed end-of-scan cloud.
  // Applying the Ouster acquisition transform a second time would double deskew.
  if(projector_.cubeResolution()){
    const int n=projector_.cubeResolution();
    const int face=static_cast<int>(projected.uv.x())/n;
    const int col=std::clamp(static_cast<int>(std::lround(projected.uv.x())),face*n,(face+1)*n-1);
    const int row=std::clamp(static_cast<int>(std::lround(projected.uv.y())),0,n-1);
    distortion_index=frame.image_index.ptr<int>(row)[col];
    if(distortion_index<0 || static_cast<std::size_t>(distortion_index)>=points.size())return false;
    p_Li=p_Lk;uv=projected.uv;return true;
  }
  if(round_bucket){projected.uv.x()=std::round(projected.uv.x());projected.uv.y()=std::round(projected.uv.y());}
  int row=static_cast<int>(projected.uv.y()),col=static_cast<int>(projected.uv.x());
  constexpr std::size_t duplicate_points=10;
  std::size_t cell=(static_cast<std::size_t>(row)*projector_.cols()+col)*duplicate_points;
  if(cell>=frame.projected_index.size())return false;
  if(frame.projected_index[cell]==0){
    row=0;
    while(row<projector_.rows()){
      cell=(static_cast<std::size_t>(row)*projector_.cols()+col)*duplicate_points;
      if(cell<frame.projected_index.size()&&frame.projected_index[cell]>0)break;
      ++row;
    }
  }
  if(row>=projector_.rows())return false;
  const int count=frame.projected_index[cell];
  if(count<=0||cell+static_cast<std::size_t>(count)>=frame.projected_index.size())return false;
  distortion_index=-1;
  if(count>1){
    float min_distance=std::numeric_limits<float>::max();
    for(int i=1;i<=count;++i){
      const int candidate=frame.projected_index[cell+i];
      if(candidate<0||static_cast<std::size_t>(candidate)>=points.size())continue;
      const Vec3 delta=p_Lk-points[candidate].point_lidar;
      const float distance=static_cast<float>(delta.norm());
      if(distance<min_distance){min_distance=distance;distortion_index=candidate;}
    }
  }else distortion_index=frame.projected_index[cell+1];
  if(distortion_index<0||static_cast<std::size_t>(distortion_index)>=points.size())return false;
  const int transform_index=frame.vec_idx.empty()?0:frame.vec_idx.at(distortion_index);
  const Eigen::Matrix4d T_Li_Lk=frame.T_Li_Lk_vec.empty()?Eigen::Matrix4d::Identity():frame.T_Li_Lk_vec.at(transform_index);
  p_Li=T_Li_Lk.topLeftCorner<3,3>()*p_Lk+T_Li_Lk.topRightCorner<3,1>();
  const ProjectedPoint distorted=projector_.project(p_Li);
  if(!distorted.in_fov)return false;
  uv=distorted.uv;return true;
}

void CoinFeatureManager::update(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
                                const std::vector<Vec3>& weak_directions_lidar,const Eigen::Matrix4d& T_GL,
                                bool pure_gradient,const std::vector<Vec3>& audit_eigenvectors_lidar){
  last_stats_=CoinFeatureFrameStats{};
  last_stats_.active_before=static_cast<int>(features_.size());
  track(frame,points,T_GL);
  updateSuppressionMask();
  detect(frame,points,weak_directions_lidar,T_GL,pure_gradient,audit_eigenvectors_lidar);
  last_stats_.active_after=static_cast<int>(features_.size());
}

void CoinFeatureManager::track(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
                               const Eigen::Matrix4d& T_GL){
  const Eigen::Matrix3d R_LG=T_GL.topLeftCorner<3,3>().transpose();
  const Vec3 p_LG=-R_LG*T_GL.topRightCorner<3,1>();
  std::vector<CoinFeature> survivors;survivors.reserve(features_.size());
  for(auto& feature:features_){
    const std::size_t patch_size=feature.points_global.size();
    std::vector<double> current(patch_size,0.);std::vector<Vec2> uv_patch;uv_patch.reserve(patch_size);
    bool visible=true;int reject_reason=0;
    for(std::size_t l=0;l<patch_size;++l){
      const Vec3 p_Lk=R_LG*feature.points_global[l]+p_LG;
      Vec3 p_Li;Vec2 uv;int distortion_index=-1;
      if(!projectUndistorted(frame,points,p_Lk,p_Li,uv,distortion_index)){
        visible=false;reject_reason=1;++last_stats_.rejected_projection;break;
      }
      if(uv.x()<settings_.margin||uv.x()>frame.intensity.cols-settings_.margin||
         uv.y()<settings_.margin||uv.y()>frame.intensity.rows-settings_.margin){
        visible=false;reject_reason=2;++last_stats_.rejected_border;break;
      }
      if(frame.mask.ptr<uchar>(static_cast<int>(uv.y()))[static_cast<int>(uv.x())]==0){
        visible=false;reject_reason=3;++last_stats_.rejected_mask;break;
      }
      const double old_range=p_Li.norm();
      const double new_range=frame.range.ptr<float>(static_cast<int>(uv.y()))[static_cast<int>(uv.x())];
      if(std::abs(old_range-new_range)>settings_.range_threshold){
        visible=false;reject_reason=4;++last_stats_.rejected_range;break;
      }
      current[l]=sampleBilinearFloat(frame.intensity,uv.x(),uv.y());
      uv_patch.push_back(uv);
    }
    feature.current_uv=std::move(uv_patch);
    const double ncc=normalizedCrossCorrelation(feature.reference_intensity,current);
    last_stats_.ncc_values.push_back(ncc);
    if(feature.lifetime<settings_.max_lifetime&&visible&&ncc>settings_.ncc_threshold){
      ++feature.lifetime;feature.center=feature.current_uv.at(patch_size/2);survivors.push_back(std::move(feature));
    }else{
      ++last_stats_.removed;
      if(feature.lifetime>=settings_.max_lifetime)++last_stats_.rejected_lifetime;
      else if(visible||reject_reason==0)++last_stats_.rejected_ncc;
    }
  }
  features_=std::move(survivors);
}

void CoinFeatureManager::updateSuppressionMask(){
  suppression_mask_=margin_mask_.clone();
  for(const auto& feature:features_)
    cv::circle(suppression_mask_,cv::Point2f(feature.center.x(),feature.center.y()),
               settings_.suppression_radius,0,-1);
}

void CoinFeatureManager::detect(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
                                const std::vector<Vec3>& weak_directions_lidar,const Eigen::Matrix4d& T_GL,
                                bool pure_gradient,const std::vector<Vec3>& audit_eigenvectors_lidar){
  const int needed=settings_.num_features-static_cast<int>(features_.size());
  if(needed<=0)return;
  std::vector<Vec3> directions=weak_directions_lidar;
  if(directions.empty()){
    directions.emplace_back(1,0,0);directions.emplace_back(0,1,0);directions.emplace_back(0,0,1);
  }
  std::vector<cv::Point> centers;
  detectComplementary(frame,points,directions,needed,centers,pure_gradient);
  last_stats_.selected_centers=static_cast<int>(centers.size());
  for(const auto& center:centers){
    last_stats_.selected_center_pixels.emplace_back(center.x,center.y);
    const float gx=frame.dx.ptr<float>(center.y)[center.x];
    const float gy=frame.dy.ptr<float>(center.y)[center.x];
    last_stats_.selected_gradient_sum+=std::hypot(gx,gy);
    const int point_index=frame.image_index.ptr<int>(center.y)[center.x];
    if(point_index>=0&&static_cast<std::size_t>(point_index)<points.size()){
      const Mat23 du_dp=projector_.projectionJacobian(points[point_index].point_lidar);
      const Eigen::Matrix<double,1,2> image_gradient(gx,gy);
      for(std::size_t d=0;d<std::min<std::size_t>(3,audit_eigenvectors_lidar.size());++d){
        Vec2 motion=du_dp*audit_eigenvectors_lidar[d];
        const double norm=motion.norm();
        if(norm>1e-12)last_stats_.selected_directional_score_sum[d]+=
            std::abs(image_gradient.dot(motion/norm));
      }
      ++last_stats_.selected_metric_count;
    }
  }
  const Eigen::Matrix3d R_GL=T_GL.topLeftCorner<3,3>();
  const Vec3 t_GL=T_GL.topRightCorner<3,1>();
  for(const auto& center:centers){
    CoinFeature feature;feature.id=next_id_++;feature.lifetime=1;feature.center=Vec2(center.x,center.y);
    feature.points_global.reserve(patch_offsets_.size());
    feature.reference_intensity.reserve(patch_offsets_.size());
    feature.current_uv.reserve(patch_offsets_.size());
    for(const auto& offset:patch_offsets_){
      const int row=center.y+offset.x(),col=center.x+offset.y();
      const int point_index=frame.image_index.ptr<int>(row)[col];
      if(point_index<0||static_cast<std::size_t>(point_index)>=points.size())
        throw std::runtime_error("COIN selected patch pixel has no dense point owner");
      feature.points_global.push_back(R_GL*points[point_index].point_lidar+t_GL);
      feature.reference_intensity.push_back(frame.intensity.ptr<float>(row)[col]);
      feature.current_uv.emplace_back(col,row);
    }
    features_.push_back(std::move(feature));
  }
  last_stats_.added=static_cast<int>(centers.size());
}

void CoinFeatureManager::detectComplementary(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
                                              const std::vector<Vec3>& directions,int needed,
                                              std::vector<cv::Point>& centers,bool pure_gradient){
  cv::Mat abs_dx,abs_dy,gradient;
  cv::convertScaleAbs(frame.dx,abs_dx);cv::convertScaleAbs(frame.dy,abs_dy);
  cv::addWeighted(abs_dx,.5,abs_dy,.5,0,gradient);
  const cv::Mat valid_mask=frame.mask & suppression_mask_;
  for(int row=0;row<valid_mask.rows;++row)for(int col=0;col<valid_mask.cols;++col)
    if(valid_mask.ptr<uchar>(row)[col]==0)gradient.ptr<uchar>(row)[col]=0;

  std::vector<std::pair<double,cv::Point>> scores;
  scores.reserve(gradient.total());
  for(int row=0;row<gradient.rows;++row)for(int col=0;col<gradient.cols;++col)
    if(gradient.ptr<uchar>(row)[col]>settings_.grad_min)
      scores.emplace_back(gradient.ptr<uchar>(row)[col],cv::Point(col,row));
  std::sort(scores.begin(),scores.end(),[](const auto& a,const auto& b){return a.first>b.first;});

  cv::Mat feature_mask=valid_mask.clone();std::vector<cv::Point> candidates;candidates.reserve(scores.size());
  for(const auto& score:scores){
    const cv::Point& p=score.second;
    if(feature_mask.ptr<uchar>(p.y)[p.x]==0)continue;
    candidates.push_back(p);cv::circle(feature_mask,p,settings_.suppression_radius,0,-1);
  }
  last_stats_.candidates_after_nms=static_cast<int>(candidates.size());
  if(pure_gradient){
    std::vector<std::pair<double,cv::Point>> gradient_candidates;
    gradient_candidates.reserve(candidates.size());
    for(const auto& point:candidates)
      gradient_candidates.emplace_back(gradient.ptr<uchar>(point.y)[point.x],point);
    centers=selectPureGradient(gradient_candidates,needed);
    return;
  }
  std::vector<std::vector<std::pair<double,int>>> directional_scores(
      directions.size(),std::vector<std::pair<double,int>>(candidates.size(),std::make_pair(0.,0)));
  const int offset=settings_.patch_size/2+1;
  for(std::size_t i=0;i<candidates.size();++i){
    const cv::Point& candidate=candidates[i];
    const cv::Rect roi(candidate.x-offset,candidate.y-offset,settings_.patch_size+2,settings_.patch_size+2);
    const cv::Mat local=frame.intensity(roi);cv::Mat eigen;
    cv::cornerEigenValsAndVecs(local,eigen,5,3);
    const cv::Vec6f values=eigen.ptr<cv::Vec6f>(offset)[offset];
    const float ix=values[0]>=values[1]?values[2]:values[4];
    const float iy=values[0]>=values[1]?values[3]:values[5];
    Eigen::Matrix<double,1,2> image_gradient;image_gradient<<ix,iy;
    const int point_index=frame.image_index.ptr<int>(candidate.y)[candidate.x];
    if(point_index<0||static_cast<std::size_t>(point_index)>=points.size())continue;
    const Mat23 du_dp=projector_.projectionJacobian(points[point_index].point_lidar);
    for(std::size_t d=0;d<directions.size();++d){
      Vec2 projected_motion=du_dp*directions[d];projected_motion.normalize();
      const float score=std::abs(image_gradient*projected_motion);
      directional_scores[d][i]=std::make_pair(score,static_cast<int>(i));
    }
  }
  for(auto& direction_scores:directional_scores)
    std::sort(direction_scores.begin(),direction_scores.end(),[](const auto& a,const auto& b){return a.first>b.first;});

  std::vector<int> seen;seen.reserve(candidates.size());
  if(directional_scores.empty())return;
  for(std::size_t rank=0;rank<directional_scores.front().size()&&static_cast<int>(centers.size())<needed;++rank){
    for(std::size_t d=0;d<directional_scores.size()&&static_cast<int>(centers.size())<needed;++d){
      const int candidate_id=directional_scores[d][rank].second;
      if(std::find(seen.begin(),seen.end(),candidate_id)!=seen.end())continue;
      seen.push_back(candidate_id);centers.push_back(candidates[candidate_id]);
    }
  }
}

} // namespace cube::coin
````

### src/super_lio/src/intensity/coin/coin_image_processor.cpp

Git blob `1cc5dd416ee054ec80f238ebeee3e36f23132fb2`；SHA-256 `ee9ed63d9964a0e2156f226fc26593e5840616d5e2fec271f85164d824150607`；99 行完整文件。

````cpp
// COIN-LIO image processing reimplementation (BSD-3-Clause source authority).
#include "intensity/coin/coin_image_processor.hpp"
#include <ros/ros.h>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cube::coin {
CoinImageSettings CoinImageSettings::fromRosParams(){
  CoinImageSettings s;ros::NodeHandle nh;
  nh.param("image/reflectivity",s.reflectivity,false);
  nh.param("image/line_removal",s.line_removal,true);
  nh.param("image/brightness_filter",s.brightness_filter,true);
  nh.param("image/blur",s.blur,true);
  nh.param("image/intensity_scale",s.intensity_scale,.25);
  nh.param("image/erosion_margin",s.erosion_margin,2);
  nh.param("image/patch_size",s.patch_size,5);
  std::vector<int> window,masks;std::vector<double> hpf,lpf;
  if(!nh.getParam("image/window",window)||window.size()!=2)throw std::runtime_error("COIN image/window must contain two integers");
  if(!nh.getParam("image/masks",masks)||masks.size()%4!=0)throw std::runtime_error("COIN image/masks malformed");
  if(!nh.getParam("image/highpass",hpf)||hpf.empty())throw std::runtime_error("COIN image/highpass is missing");
  if(!nh.getParam("image/lowpass",lpf)||lpf.empty())throw std::runtime_error("COIN image/lowpass is missing");
  s.brightness_window=cv::Size(window[0],window[1]);s.highpass=std::move(hpf);s.lowpass=std::move(lpf);
  for(std::size_t i=0;i<masks.size();i+=4)s.masks.emplace_back(masks[i],masks[i+1],masks[i+2],masks[i+3]);
  nh.param("image/min_range",s.min_range,.7);nh.param("image/max_range",s.max_range,30.);
  return s;
}

CoinImageProcessor::CoinImageProcessor(CoinOusterProjector projector,CoinImageSettings settings)
  :projector_(std::move(projector)),settings_(std::move(settings)){
  if(settings_.patch_size<1||settings_.brightness_window.width<1||settings_.brightness_window.height<1||
     settings_.intensity_scale<=0||settings_.min_range<0||settings_.max_range<=settings_.min_range||
     settings_.highpass.empty()||settings_.lowpass.empty())throw std::invalid_argument("invalid official COIN image settings");
  for(const auto& r:settings_.masks)if(r.x<0||r.y<0||r.x+r.width>projector_.cols()||r.y+r.height>projector_.rows())
    throw std::invalid_argument("COIN image mask lies outside calibrated image");
}

CoinFrame CoinImageProcessor::process(std::vector<CoinScanPoint>& points) const {
  const int rows=projector_.rows(),cols=projector_.cols();
  CoinFrame frame;
  frame.intensity=cv::Mat::zeros(rows,cols,CV_32FC1);
  frame.range=cv::Mat::zeros(rows,cols,CV_32FC1);
  frame.image_index=cv::Mat::ones(rows,cols,CV_32SC1)*(-1);
  frame.projected_index.assign(static_cast<std::size_t>(rows)*cols*10,0);
  std::vector<int> u(points.size(),-1),v(points.size(),-1);
  for(std::size_t j=0;j<points.size();++j){
    auto q=projector_.project(points[j].point_lidar);
    if(!q.in_fov)continue;
    u[j]=static_cast<int>(std::round(q.uv.x()));v[j]=static_cast<int>(std::round(q.uv.y()));
  }
  for(std::size_t j=0;j<points.size();++j){
    int row,col;projector_.pixelFromRawIndex(points[j].raw_index,row,col);
    frame.range.ptr<float>(row)[col]=points[j].range;
    frame.intensity.ptr<float>(row)[col]=points[j].intensity;
    frame.image_index.ptr<int>(row)[col]=static_cast<int>(j);
  }
  frame.raw_intensity=frame.intensity.clone();
  for(std::size_t j=0;j<points.size();++j){
    if(u[j]<0||v[j]<0)continue;
    const std::size_t start=(static_cast<std::size_t>(v[j])*cols+u[j])*10;
    const int offset=frame.projected_index[start]+1;
    if(offset>=10)continue;
    frame.projected_index[start+offset]=static_cast<int>(j);
    frame.projected_index[start]=offset;
  }

  if(!settings_.reflectivity)frame.intensity*=settings_.intensity_scale;
  if(settings_.line_removal){
    cv::Mat im_hpf,im_lpf;
    cv::Mat hpf(settings_.highpass),lpf(settings_.lowpass);
    cv::filter2D(frame.intensity,im_hpf,CV_32F,hpf);
    cv::filter2D(im_hpf,im_lpf,CV_32F,lpf.t());
    frame.intensity-=im_lpf;frame.intensity.setTo(0,frame.intensity<0);
  }
  if(settings_.brightness_filter){
    cv::Mat brightness,normalized;
    cv::blur(frame.intensity,brightness,settings_.brightness_window);
    brightness+=1;normalized=140.*frame.intensity/brightness;frame.intensity=normalized;
  }
  if(settings_.blur){cv::Mat smoothed;cv::GaussianBlur(frame.intensity,smoothed,cv::Size(3,3),0);frame.intensity=smoothed;}
  cv::threshold(frame.intensity,frame.intensity,255.,255.,cv::THRESH_TRUNC);
  frame.intensity.convertTo(frame.photo_u8,CV_8UC1,1);
  cv::Mat kernel_dx=cv::Mat::zeros(1,3,CV_32F),kernel_dy=cv::Mat::zeros(3,1,CV_32F);
  kernel_dx.at<float>(0,0)=-.5f;kernel_dx.at<float>(0,2)=.5f;
  kernel_dy.at<float>(0,0)=-.5f;kernel_dy.at<float>(2,0)=.5f;
  cv::filter2D(frame.intensity,frame.dx,CV_32F,kernel_dx);
  cv::filter2D(frame.intensity,frame.dy,CV_32F,kernel_dy);
  frame.mask=cv::Mat::ones(rows,cols,CV_8UC1);
  for(const auto& r:settings_.masks)frame.mask(r)=0;
  for(int row=0;row<rows;++row)for(int col=0;col<cols;++col){
    const float range=frame.range.ptr<float>(row)[col];
    if(range<settings_.min_range||range>settings_.max_range)frame.mask.ptr<uchar>(row)[col]=0;
  }
  const int erosion=settings_.patch_size+settings_.erosion_margin;
  cv::Mat eroded;cv::erode(frame.mask,eroded,cv::Mat::ones(erosion,erosion,CV_32FC1));frame.mask=eroded;
  return frame;
}
} // namespace cube::coin
````

### src/super_lio/src/intensity/coin/coin_observation.cpp

Git blob `f3d824b71119a2d45e32b5f37d57e61a9361a5de`；SHA-256 `e4c81bd6644a6450233ba2987bf667159de3f98ab5a8f73c9c9ae000adc3409f`；360 行完整文件。

````cpp
#include "intensity/coin/coin_observation.hpp"
#include "intensity/coin/coin_acquisition.hpp"
#include "intensity/coin/super_degeneracy_gate.hpp"

#include "lio/params.h"
#include <ros/ros.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <stdexcept>

namespace cube::coin {
namespace {
using Mat3=Eigen::Matrix3d;
using Vec3d=Eigen::Vector3d;
constexpr double kOfficialMedianWeakContributionPerRow=0.01609598;

Eigen::Matrix4d transform(const Mat3& R,const Vec3d& t){
  Eigen::Matrix4d T=Eigen::Matrix4d::Identity();
  T.topLeftCorner<3,3>()=R;T.topRightCorner<3,1>()=t;return T;
}

}

CoinObservation::CoinObservation(ros::NodeHandle& nh,const Eigen::Matrix4d& T_IL)
  :T_IL_(T_IL){
  nh.param("/coin/enable",enabled_,false);
  if(!enabled_)return;
  nh.param("/coin/shadow",shadow_,false);
  nh.param<std::string>("/coin/selector_mode",selector_mode_,"original");
  if(selector_mode_!="original"&&selector_mode_!="gradient"&&
     selector_mode_!="weakest"&&selector_mode_!="normalized"&&
     selector_mode_!="s2"&&selector_mode_!="g1"&&selector_mode_!="g2")
    throw std::invalid_argument("unknown COIN selector mode: "+selector_mode_);
  nh.param("/coin/gate_g1_confidence_threshold",gate_g1_confidence_threshold_,-1.);
  nh.param("/coin/gate_g2_confidence_threshold",gate_g2_confidence_threshold_,-1.);
  if((selector_mode_=="g1"||selector_mode_=="g2")&&
     (!(gate_g1_confidence_threshold_>=0.)||!(gate_g2_confidence_threshold_>=0.)))
    throw std::invalid_argument("gated COIN selector requires geometry-derived G1 and G2 confidence thresholds");
  if(LI2Sup::g_lidar_type!=LI2Sup::LID_TYPE::OUSTER)
    throw std::runtime_error("faithful COIN observation currently requires Ouster");
  bool photo_enabled=false;nh.param("/photo/enable",photo_enabled,false);
  if(photo_enabled)throw std::runtime_error("enable either /coin/enable or /photo/enable, not both");
  nh.param("/filter/photo_scale",photo_scale_,0.00095);
  nh.param("/coin/measurement_variance",measurement_variance_,0.001);
  double sensor_z_offset=0.03618;
  std::vector<double> lidar_to_sensor;
  if((nh.getParam("/lidar_to_sensor_transform",lidar_to_sensor)||
      nh.getParam("/lidar_intrinsics/lidar_to_sensor_transform",lidar_to_sensor))&&
     lidar_to_sensor.size()==16)
    sensor_z_offset=lidar_to_sensor[11]*0.001;
  // The COIN preprocessor subtracts this sensor-origin offset from z. Adjust
  // the sensor-to-IMU translation so the transformed physical point is the
  // same point used by Super-LIO's unshifted geometry cloud.
  T_IL_.topRightCorner<3,1>()+=T_IL_.topLeftCorner<3,3>()*Eigen::Vector3d(0.,0.,sensor_z_offset);
  if(!(photo_scale_>0.)||!(measurement_variance_>0.))
    throw std::invalid_argument("invalid COIN scale or measurement variance");
  const OusterMetadata metadata=OusterMetadata::fromRosParams();
  representation_mode_=p3aRepresentation();
  const auto cube_settings=p3aCubeSettings(representation_mode_);
  const bool use_cube=representation_mode_!="coin";
  if(use_cube){
    cube_representation_=std::make_unique<CubeImage>(cube_settings,
        cube_settings.build_igm?MeasurementChannel::IntensityGradientMagnitude:MeasurementChannel::RawIntensity);
  }
  projector_=std::make_unique<CoinOusterProjector>(metadata,use_cube?cube_settings.resolution:0);
  image_settings_=CoinImageSettings::fromRosParams();
  feature_settings_=CoinFeatureSettings::fromRosParams();
  if(use_cube && cube_settings.resolution<=2*feature_settings_.margin)
    throw std::invalid_argument("cubemap too small for frozen COIN margin");
  image_processor_=std::make_unique<CoinImageProcessor>(CoinOusterProjector(metadata),image_settings_);
  feature_manager_=std::make_unique<CoinFeatureManager>(*projector_,feature_settings_);
  std::string output_dir;
  nh.getParam("/lio/offline/out_dir",output_dir);
  if(!output_dir.empty()){
    if(use_cube){
      representation_diagnostics_.open(output_dir+"/representation.csv");
      if(!representation_diagnostics_)throw std::runtime_error("cannot write representation diagnostics");
      representation_diagnostics_<<"frame,representation,input_points,raw_pixels,filled_pixels,igm_pixels,patch_mask_pixels,intensity_points\n";
    }
    diagnostics_.open(output_dir+"/coin_observation.csv");
    if(!diagnostics_)throw std::runtime_error("cannot write COIN observation diagnostics");
    diagnostics_<<"frame,timestamp,raw_points,motion_fallback_points,coin_minus_super_scan_end_s,active_before,valid_patches,photo_rows,residual_rms,photo_A_norm,photo_b_norm,weak_dirs,active_after,added,removed,status,skip_reason,ncc_count,ncc_median,rejected_ncc,selector_mode,selected_centers_xy,selected_gradient_mean,selected_score_e1,selected_score_e2,selected_score_e3"
      <<",N_geo_rows,lambda1,lambda2,lambda3,lambda1_over_lambda2,lambda1_over_lambda3,"
      "weakest_axis_stability,anisotropy_confidence,eigengap_confidence,degeneracy_confidence,"
      "gate_threshold,gate_active\n"<<std::setprecision(17);
  }
  std::string fusion_audit_path;
  nh.getParam("/p2r/fusion_audit_path",fusion_audit_path);
  if(!fusion_audit_path.empty()){
    fusion_audit_.open(fusion_audit_path);
    if(!fusion_audit_)throw std::runtime_error("cannot write P2R fusion audit");
    fusion_audit_<<std::setprecision(17);
  }
}

Eigen::Matrix4d CoinObservation::poseMatrix(const BASIC::SE3& pose) const{
  Eigen::Matrix4d T=Eigen::Matrix4d::Identity();
  T.topLeftCorner<3,3>()=pose.R_.cast<double>();
  T.topRightCorner<3,1>()=pose.t_.cast<double>();
  return T;
}

void CoinObservation::prepare(const LI2Sup::LidarData& lidar,
                              const std::vector<LI2Sup::DynamicState>& history,
                              const BASIC::SE3& predicted_pose){
  if(!enabled_)return;
  prepared_=false;
  double max_offset=0.;
  for(const auto& raw:lidar.coin_raw_points)max_offset=std::max(max_offset,raw.offset_time);
  coin_scan_end_delta_s_=lidar.coin_raw_points.empty()?0.:
    max_offset-(lidar.end_time-lidar.start_time);
  auto skip_frame=[&](const std::string& reason,int unsupported_points){
    const int active=static_cast<int>(feature_manager_->features().size());
    if(diagnostics_){
      diagnostics_<<scan_index_<<','<<lidar.end_time<<','<<lidar.coin_raw_points.size()<<','
        <<unsupported_points<<','<<coin_scan_end_delta_s_<<','<<active
        <<",0,0,0,0,0,0,"<<active<<",0,0,SKIPPED,"<<reason<<",0,0,0,"
        <<selector_mode_;
      for(int column=0;column<17;++column)diagnostics_<<',';
      diagnostics_<<'\n';
    }
    ROS_WARN_THROTTLE(5.0,"COIN observation skipped at scan %zu: %s; geometric update continues",
                      scan_index_,reason.c_str());
    ++scan_index_;
  };
  const auto input_issue=coinFrameInputIssue(lidar,history);
  if(!input_issue.empty()){
    skip_frame(input_issue,static_cast<int>(lidar.coin_raw_points.size()));
    return;
  }
  const Eigen::Matrix4d T_GI=poseMatrix(predicted_pose);
  const Mat3 R_GI=T_GI.topLeftCorner<3,3>();
  const Vec3d t_GI=T_GI.topRightCorner<3,1>();
  const Mat3 R_IL=T_IL_.topLeftCorner<3,3>();
  const Vec3d t_IL=T_IL_.topRightCorner<3,1>();
  const Mat3 R_GL_end=R_GI*R_IL;
  const Vec3d t_GL_end=R_GI*t_IL+t_GI;

  points_.clear();points_.reserve(lidar.coin_raw_points.size());
  motion_fallback_points_=0;
  std::vector<Eigen::Matrix4d> transforms;transforms.reserve(lidar.coin_raw_points.size());
  std::vector<int> transform_indices;transform_indices.reserve(lidar.coin_raw_points.size());
  for(const auto& raw:lidar.coin_raw_points){
    CoinScanPoint point;
    point.point_lidar=Vec3(raw.x,raw.y,raw.z);
    point.intensity=raw.intensity;point.range=raw.range;
    point.raw_index=raw.raw_index;point.offset_seconds=raw.offset_time;
    Mat3 R_GI_acq;Vec3d t_GI_acq;
    Eigen::Matrix4d T_Li_Lk=Eigen::Matrix4d::Identity();
    if(stateAt(history,lidar.start_time+raw.offset_time,R_GI_acq,t_GI_acq)){
      const Mat3 R_GL_acq=R_GI_acq*R_IL;
      const Vec3d t_GL_acq=R_GI_acq*t_IL+t_GI_acq;
      const Vec3d p_global=R_GI_acq*(R_IL*point.point_lidar+t_IL)+t_GI_acq;
      point.point_lidar=R_GL_end.transpose()*(p_global-t_GL_end);
      T_Li_Lk=lidarAcquisitionToEnd(R_GL_acq,t_GL_acq,R_GL_end,t_GL_end);
    }else{
      ++motion_fallback_points_;
      continue;
    }
    if(!point.point_lidar.allFinite()||!T_Li_Lk.allFinite()){
      ++motion_fallback_points_;
      continue;
    }
    points_.push_back(point);
    transforms.push_back(T_Li_Lk);
    transform_indices.push_back(static_cast<int>(transform_indices.size()));
  }
  if(motion_fallback_points_){
    skip_frame("unsupported_or_nonfinite_acquisition_transform",motion_fallback_points_);
    return;
  }
  frame_=image_processor_->process(points_);
  if(cube_representation_){
    const auto input_points=points_.size();
    frame_=buildCoinCubemap(*cube_representation_,frame_,image_processor_->projector(),image_settings_,points_);
    if(representation_diagnostics_)representation_diagnostics_<<scan_index_<<','<<representation_mode_<<','
      <<input_points<<','<<cube_representation_->rawPixelCount()<<','<<cube_representation_->filledPixelCount()<<','
      <<cube_representation_->validFeaturePixelCount()<<','<<cv::countNonZero(frame_.mask)<<','<<points_.size()<<'\n';
  }else{
    frame_.T_Li_Lk_vec=std::move(transforms);
    frame_.vec_idx=std::move(transform_indices);
  }
  prepared_=true;active_before_=static_cast<int>(feature_manager_->features().size());
  valid_patches_=photo_rows_=0;residual_square_sum_=photo_A_norm_=photo_b_norm_=0.;
}

void CoinObservation::add(const BASIC::SE3& pose,BASIC::M6& A,BASIC::V6& b){
  if(!enabled_||!prepared_)return;
  const auto T_GI=poseMatrix(pose);
  Eigen::Matrix<double,6,6> photo_A=Eigen::Matrix<double,6,6>::Zero();
  Eigen::Matrix<double,6,1> photo_b=Eigen::Matrix<double,6,1>::Zero();
  const bool audit_this_frame=fusion_audit_&&!fusion_audited_&&!shadow_;
  std::vector<Eigen::Matrix<double,1,6>> audit_H;
  std::vector<double> audit_r;
  valid_patches_=photo_rows_=0;residual_square_sum_=0.;
  const auto& features=feature_manager_->features();
  for(const auto& feature:features){
    std::vector<CoinPhotoLinearization> patch;
    patch.reserve(feature.points_global.size());bool valid=true;
    for(std::size_t i=0;i<feature.points_global.size();++i){
      auto row=CoinPhotometricModel::linearize(*feature_manager_,*projector_,frame_,points_,
          T_GI,T_IL_,feature.points_global[i],feature.reference_intensity[i],
          feature_settings_.min_range,feature_settings_.max_range,feature_settings_.margin);
      if(!row.valid){valid=false;break;}
      patch.push_back(row);
    }
    if(!valid||patch.size()!=feature.points_global.size())continue;
    ++valid_patches_;
    for(const auto& row:patch){
      const Eigen::Matrix<double,1,6> H=row.correction_jacobian_super;
      photo_A.noalias()+=H.transpose()*H;
      photo_b.noalias()+=H.transpose()*row.residual;
      if(audit_this_frame){audit_H.push_back(H);audit_r.push_back(row.residual);}
      residual_square_sum_+=row.residual*row.residual;++photo_rows_;
    }
  }
  const double factor=photo_scale_*photo_scale_/measurement_variance_;
  photo_A*=factor;photo_b*=factor;
  photo_A_norm_=photo_A.norm();photo_b_norm_=photo_b.norm();
  BASIC::M6 A_before;
  BASIC::V6 b_before;
  if(audit_this_frame&&photo_rows_){A_before=A;b_before=b;}
  if(!shadow_){A+=photo_A.cast<BASIC::scalar>();b+=photo_b.cast<BASIC::scalar>();}
  if(audit_this_frame&&photo_rows_){
    Eigen::MatrixXd H(photo_rows_,6);
    Eigen::VectorXd residual(photo_rows_);
    for(int i=0;i<photo_rows_;++i){H.row(i)=audit_H[i];residual(i)=audit_r[i];}
    const Eigen::MatrixXd H_scaled=photo_scale_*H;
    const Eigen::VectorXd r_scaled=photo_scale_*residual;
    const Eigen::Matrix<double,6,6> A_explicit=
        (H_scaled.transpose()*H_scaled)/measurement_variance_;
    const Eigen::Matrix<double,6,1> b_explicit=
        (H_scaled.transpose()*r_scaled)/measurement_variance_;
    const double A_max=(A_explicit-photo_A).cwiseAbs().maxCoeff();
    const double b_max=(b_explicit-photo_b).cwiseAbs().maxCoeff();
    const double A_rel=(A_explicit-photo_A).norm()/std::max(1.,A_explicit.norm());
    const double b_rel=(b_explicit-photo_b).norm()/std::max(1.,b_explicit.norm());
    const auto A_cast=A_explicit.cast<BASIC::scalar>();
    const auto b_cast=b_explicit.cast<BASIC::scalar>();
    const double cast_A_rel=(A_cast.cast<double>()-photo_A.cast<BASIC::scalar>().cast<double>()).norm()/
        std::max(1.,A_cast.cast<double>().norm());
    const double cast_b_rel=(b_cast.cast<double>()-photo_b.cast<BASIC::scalar>().cast<double>()).norm()/
        std::max(1.,b_cast.cast<double>().norm());
    const double actual_A_rel=((A-A_before).cast<double>()-A_cast.cast<double>()).norm()/
        std::max(1.,A_cast.cast<double>().norm());
    const double actual_b_rel=((b-b_before).cast<double>()-b_cast.cast<double>()).norm()/
        std::max(1.,b_cast.cast<double>().norm());
    const double actual_A_abs=((A-A_before).cast<double>()-A_cast.cast<double>()).cwiseAbs().maxCoeff();
    const double actual_b_abs=((b-b_before).cast<double>()-b_cast.cast<double>()).cwiseAbs().maxCoeff();
    const double eps=std::numeric_limits<BASIC::scalar>::epsilon();
    const double A_rounding_bound=4*eps*(A_before.cast<double>().cwiseAbs()+
        A_cast.cast<double>().cwiseAbs()).maxCoeff();
    const double b_rounding_bound=4*eps*(b_before.cast<double>().cwiseAbs()+
        b_cast.cast<double>().cwiseAbs()).maxCoeff();
    const bool pass=A_rel<1e-10&&b_rel<1e-10&&cast_A_rel<1e-6&&cast_b_rel<1e-6&&
        actual_A_abs<=A_rounding_bound&&actual_b_abs<=b_rounding_bound;
    fusion_audit_<<"{\"status\":\""<<(pass?"PASS":"FAIL")<<"\","
      <<"\"frame\":"<<scan_index_<<",\"pose_state_fixed_within_iteration\":true,"
      <<"\"photo_rows\":"<<photo_rows_<<",\"valid_patches\":"<<valid_patches_<<','
      <<"\"photo_scale\":"<<photo_scale_<<",\"measurement_variance\":"<<measurement_variance_<<','
      <<"\"information_factor\":"<<factor<<",\"H_column_order\":\"right_local_rotation_then_global_position\","
      <<"\"b_sign\":\"positive_H_transpose_times_raw_intensity_residual\","
      <<"\"A_max_abs_difference\":"<<A_max<<",\"b_max_abs_difference\":"<<b_max<<','
      <<"\"A_relative_norm_difference\":"<<A_rel<<",\"b_relative_norm_difference\":"<<b_rel<<','
      <<"\"scalar_cast_A_relative_difference\":"<<cast_A_rel<<','
      <<"\"scalar_cast_b_relative_difference\":"<<cast_b_rel<<','
      <<"\"actual_total_accumulator_A_relative_rounding\":"<<actual_A_rel<<','
      <<"\"actual_total_accumulator_b_relative_rounding\":"<<actual_b_rel<<','
      <<"\"actual_total_accumulator_A_max_abs_rounding\":"<<actual_A_abs<<','
      <<"\"actual_total_accumulator_b_max_abs_rounding\":"<<actual_b_abs<<','
      <<"\"float_rounding_bound_A\":"<<A_rounding_bound<<','
      <<"\"float_rounding_bound_b\":"<<b_rounding_bound<<"}\n";
    fusion_audit_.flush();
    fusion_audited_=true;
  }
}

void CoinObservation::finish(const BASIC::SE3& pose,double timestamp,
                             const Eigen::MatrixXd& geometry_translation_rows){
  if(!enabled_||!prepared_)return;
  const Eigen::Matrix4d T_GI=poseMatrix(pose);
  const Eigen::Matrix3d R_GL=T_GI.topLeftCorner<3,3>()*T_IL_.topLeftCorner<3,3>();
  const Eigen::Vector3d t_GL=T_GI.topLeftCorner<3,3>()*T_IL_.topRightCorner<3,1>()+
                             T_GI.topRightCorner<3,1>();
  const Eigen::Matrix4d T_GL=transform(R_GL,t_GL);
  const auto weak=CoinFeatureManager::weakDirectionsFromGeometry(
      geometry_translation_rows,R_GL,feature_settings_.n_uninformative);
  const bool previous_axis_is_temporally_supported=has_previous_weak_axis_&&
      timestamp>previous_weak_axis_timestamp_&&timestamp-previous_weak_axis_timestamp_<=.25;
  const auto signal=SuperDegeneracyGate::measure(weak,previous_weak_axis_global_,
                                                  previous_axis_is_temporally_supported);
  std::vector<Vec3> selector_directions=weak.lidar;
  bool pure_gradient=false;
  bool gate_active=false;
  double gate_threshold=-1.;
  if(selector_mode_=="gradient"){
    selector_directions.clear();pure_gradient=true;
  }else if(selector_mode_=="weakest"||selector_mode_=="s2"){
    selector_directions.clear();
    if(geometry_translation_rows.rows()>3)
      selector_directions.push_back(R_GL.transpose()*weak.eigenvectors.col(0));
    else selector_directions=weak.lidar;
  }else if(selector_mode_=="normalized"){
    const double threshold=kOfficialMedianWeakContributionPerRow*
        static_cast<double>(geometry_translation_rows.rows());
    selector_directions=CoinFeatureManager::weakDirectionsFromGeometry(
        geometry_translation_rows,R_GL,threshold).lidar;
  }else if(selector_mode_=="g1"||selector_mode_=="g2"){
    gate_threshold=selector_mode_=="g1"?gate_g1_confidence_threshold_:
                                           gate_g2_confidence_threshold_;
    gate_active=SuperDegeneracyGate::activate(signal,gate_threshold);
    if(gate_active){
      selector_directions.clear();
      selector_directions.push_back(R_GL.transpose()*weak.eigenvectors.col(0));
    }
  }
  if(weak.geometry_rows>3&&weak.eigenvectors.col(0).allFinite()){
    previous_weak_axis_global_=weak.eigenvectors.col(0).normalized();
    previous_weak_axis_timestamp_=timestamp;
    has_previous_weak_axis_=true;
  }
  std::vector<Vec3> audit_eigenvectors_lidar;
  for(int d=0;d<3;++d)audit_eigenvectors_lidar.push_back(R_GL.transpose()*weak.eigenvectors.col(d));
  feature_manager_->update(frame_,points_,selector_directions,T_GL,pure_gradient,audit_eigenvectors_lidar);
  const auto& stats=feature_manager_->lastStats();
  if(diagnostics_){
    const double rms=photo_rows_?std::sqrt(residual_square_sum_/photo_rows_):0.;
    std::vector<double> finite_ncc;
    for(double ncc:stats.ncc_values)if(std::isfinite(ncc))finite_ncc.push_back(ncc);
    std::sort(finite_ncc.begin(),finite_ncc.end());
    const double ncc_median=finite_ncc.empty()?0.:
        finite_ncc[finite_ncc.size()/2];
    diagnostics_<<scan_index_<<','<<timestamp<<','<<points_.size()<<','<<motion_fallback_points_<<','
      <<coin_scan_end_delta_s_<<','<<active_before_<<','
      <<valid_patches_<<','<<photo_rows_<<','<<rms<<','<<photo_A_norm_<<','<<photo_b_norm_<<','<<selector_directions.size()<<','
      <<feature_manager_->features().size()<<','<<stats.added<<','<<stats.removed
      <<",USED,,"<<finite_ncc.size()<<','<<ncc_median<<','<<stats.rejected_ncc<<','
      <<selector_mode_<<',';
    for(std::size_t i=0;i<stats.selected_center_pixels.size();++i){
      if(i)diagnostics_<<';';
      diagnostics_<<stats.selected_center_pixels[i].x()<<':'<<stats.selected_center_pixels[i].y();
    }
    auto average=[](double sum,int count){return count?sum/count:0.;};
    diagnostics_<<','<<average(stats.selected_gradient_sum,stats.selected_metric_count);
    for(const double score:stats.selected_directional_score_sum)
      diagnostics_<<','<<average(score,stats.selected_metric_count);
    diagnostics_<<','<<weak.geometry_rows<<','<<weak.eigenvalues[0]<<','<<weak.eigenvalues[1]<<','
      <<weak.eigenvalues[2]<<','<<signal.lambda1_over_lambda2<<','<<signal.lambda1_over_lambda3<<','
      <<signal.weakest_axis_stability<<','<<signal.anisotropy_confidence<<','
      <<signal.eigengap_confidence<<','<<signal.confidence<<','<<gate_threshold<<','
      <<(gate_active?1:0);
    diagnostics_<<'\n';
  }
  ++scan_index_;prepared_=false;
}

} // namespace cube::coin
````

### src/super_lio/src/intensity/coin/coin_ouster_projector.cpp

Git blob `8361b8b0de767af2d345a9e7e395852f4ebbae01`；SHA-256 `2f24c25a52fa59b9f5b69b98f64f36772dc1d44bd1eabe856f576ec729de3e72`；103 行完整文件。

````cpp
// COIN-LIO Ouster projector reimplementation (BSD-3-Clause source authority).
#include "intensity/coin/coin_ouster_projector.hpp"
#include "intensity/cube_projector.hpp"
#include <ros/ros.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cube::coin {
OusterMetadata OusterMetadata::fromRosParams() {
  OusterMetadata m;
  float rows=0,cols=0,u_shift=0;
  auto required=[](bool ok,const char* name){if(!ok)throw std::runtime_error(std::string("missing COIN Ouster param: ")+name);};
  required(ros::param::get("/lidar_data_format/pixels_per_column",rows),"pixels_per_column");
  required(ros::param::get("/lidar_data_format/columns_per_frame",cols),"columns_per_frame");
  required(ros::param::get("/beam_intrinsics/lidar_origin_to_beam_origin_mm",m.beam_offset_mm),"beam_offset_mm");
  required(ros::param::get("/lidar_data_format/pixel_shift_by_row",m.pixel_shift_by_row),"pixel_shift_by_row");
  required(ros::param::get("/beam_intrinsics/beam_altitude_angles",m.beam_altitude_degrees),"beam_altitude_angles");
  required(ros::param::get("/image/u_shift",u_shift),"image/u_shift");
  m.rows=static_cast<int>(rows);m.cols=static_cast<int>(cols);m.u_shift=static_cast<int>(u_shift);
  ros::param::param("/image/destagger",m.destagger,true);
  m.validate();return m;
}

void OusterMetadata::validate() const {
  if(rows<2||cols<2||pixel_shift_by_row.size()!=static_cast<std::size_t>(rows)||
     beam_altitude_degrees.size()!=static_cast<std::size_t>(rows)||!std::isfinite(beam_offset_mm))
    throw std::invalid_argument("invalid COIN Ouster metadata dimensions or calibration");
  if(u_shift<0||u_shift>=cols)throw std::invalid_argument("invalid COIN Ouster image/u_shift");
  for(double a:beam_altitude_degrees)if(!std::isfinite(a))throw std::invalid_argument("nonfinite Ouster elevation angle");
}

CoinOusterProjector::CoinOusterProjector(OusterMetadata metadata,int cube_resolution)
  :metadata_(std::move(metadata)),cube_resolution_(cube_resolution) {
  if(cube_resolution_ && (cube_resolution_<8 || cube_resolution_>1024))
    throw std::invalid_argument("invalid COIN cubemap resolution");
  metadata_.validate();
  elevation_radians_.reserve(metadata_.beam_altitude_degrees.size());
  for(double a:metadata_.beam_altitude_degrees)elevation_radians_.push_back(a*M_PI/180.);
  const double fy=-static_cast<double>(metadata_.rows)/std::abs(elevation_radians_.front()-elevation_radians_.back());
  const double fx=-static_cast<double>(metadata_.cols)/(2*M_PI);
  K_<<fx,0,metadata_.cols/2,0,fy,metadata_.rows/2,0,0,1;
  beam_offset_m_=metadata_.beam_offset_mm*1e-3;
  raw_to_row_.assign(metadata_.rows*metadata_.cols,0);
  raw_to_col_.assign(metadata_.rows*metadata_.cols,0);
  for(int row=0;row<metadata_.rows;++row)for(int col=0;col<metadata_.cols;++col){
    const auto raw=indexFromPixel(row,col);
    raw_to_row_[raw]=row;
    int destaggered=col-metadata_.u_shift;
    if(destaggered<0)destaggered+=metadata_.cols;
    if(destaggered>=metadata_.cols)destaggered-=metadata_.cols;
    raw_to_col_[raw]=destaggered;
  }
}

std::size_t CoinOusterProjector::indexFromPixel(int row,int col) const {
  const int v=(col+metadata_.cols-metadata_.pixel_shift_by_row.at(row))%metadata_.cols;
  return static_cast<std::size_t>(row)*metadata_.cols+(metadata_.destagger?v:col);
}

void CoinOusterProjector::pixelFromRawIndex(std::size_t raw,int& row,int& col) const {
  if(raw>=raw_to_row_.size())throw std::out_of_range("COIN Ouster raw point index exceeds calibrated image");
  row=raw_to_row_[raw];col=raw_to_col_[raw];
}

ProjectedPoint CoinOusterProjector::project(const Vec3& p) const {
  ProjectedPoint out;
  if(cube_resolution_){
    const auto q=cube::CubeProjector(cube_resolution_).project(p);
    if(q.face<0 || q.seam)return out;
    out.uv=q.uv;out.uv.x()+=q.face*cube_resolution_;
    out.in_fov=true;return out;
  }
  if(!p.allFinite())return out;
  const double L=std::sqrt(p.x()*p.x()+p.y()*p.y())-beam_offset_m_;
  const double R=std::sqrt(p.z()*p.z()+L*L);
  if(!(R>0)||!std::isfinite(R))return out;
  const double phi=std::atan2(p.y(),p.x());
  const double theta=std::asin(std::clamp(p.z()/R,-1.,1.));
  out.uv.x()=K_(0,0)*phi+K_(0,2);
  if(theta>elevation_radians_.front()){out.uv.y()=0;return out;}
  if(theta<elevation_radians_.back()){out.uv.y()=metadata_.rows-1;return out;}
  const auto greater=(std::upper_bound(elevation_radians_.rbegin(),elevation_radians_.rend(),theta)+1).base();
  const auto smaller=greater+1;
  if(greater==elevation_radians_.end())out.uv.y()=metadata_.rows-1;
  else {
    out.uv.y()=std::distance(elevation_radians_.begin(),greater);
    out.uv.y()+=(*greater-theta)/(*greater-*smaller);
  }
  out.in_fov=out.uv.x()>=0&&out.uv.x()<=metadata_.cols-1&&out.uv.y()>=0&&out.uv.y()<=metadata_.rows-1;
  return out;
}

Mat23 CoinOusterProjector::projectionJacobian(const Vec3& p) const {
  if(cube_resolution_)return cube::CubeProjector(cube_resolution_).project(p).jacobian;
  const double rxy=p.head<2>().norm(),L=rxy-beam_offset_m_,R2=L*L+p.z()*p.z();
  const double irxy=1./rxy,irxy2=irxy*irxy,fx_irxy2=K_(0,0)*irxy2;
  Mat23 J;
  J<<-fx_irxy2*p.y(),fx_irxy2*p.x(),0,
      -K_(1,1)*p.x()*p.z()/(L*R2),-K_(1,1)*p.y()*p.z()/(L*R2),K_(1,1)*L/R2;
  return J;
}
} // namespace cube::coin
````

### src/super_lio/src/intensity/coin/coin_photometric_model.cpp

Git blob `a6a8c2ec94f04870a339f60a846381687d6330a6`；SHA-256 `f18fb5aef69ae97d8f3dc6223ca27984456dc5d1e863e815b3166270dad00642`；101 行完整文件。

````cpp
// COIN-LIO photometric residual/Jacobian semantics, reimplemented from source.
#include "intensity/coin/coin_photometric_model.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cube::coin {
namespace {
Eigen::Matrix3d skew(const Eigen::Vector3d& v){
  Eigen::Matrix3d m;
  m<<0.,-v.z(),v.y(),v.z(),0.,-v.x(),-v.y(),v.x(),0.;
  return m;
}
}

double CoinPhotometricModel::sampleFloat(const cv::Mat& image,double x,double y){
  x=std::clamp(x,0.,static_cast<double>(image.cols-1));
  y=std::clamp(y,0.,static_cast<double>(image.rows-1));
  const int x0=static_cast<int>(x),x1=x0+1,y0=static_cast<int>(y),y1=y0+1;
  const double ax=x-std::floor(x),ay=y-std::floor(y);
  const double value=(1-ax)*(1-ay)*image.ptr<float>(y0)[x0]+ax*(1-ay)*image.ptr<float>(y0)[x1]+
                     (1-ax)*ay*image.ptr<float>(y1)[x0]+ax*ay*image.ptr<float>(y1)[x1];
  return static_cast<float>(value);
}

Eigen::Vector2d CoinPhotometricModel::centralImageGradient(const cv::Mat& image,double x,double y){
  return Eigen::Vector2d(.5*(sampleFloat(image,x+1.,y)-sampleFloat(image,x-1.,y)),
                         .5*(sampleFloat(image,x,y+1.)-sampleFloat(image,x,y-1.)));
}

CoinPhotoLinearization CoinPhotometricModel::linearize(const CoinFeatureManager& manager,
    const CoinOusterProjector& projector,const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
    const Eigen::Matrix4d& T_GI,const Eigen::Matrix4d& T_IL,const Vec3& point_global,
    double reference_intensity,double min_range,double max_range,int margin){
  CoinPhotoLinearization out;
  const Eigen::Matrix3d R_GI=T_GI.topLeftCorner<3,3>();
  const Eigen::Matrix3d R_IG=R_GI.transpose();
  const Eigen::Matrix3d R_GL=R_GI*T_IL.topLeftCorner<3,3>();
  const Vec3 t_GL=R_GI*T_IL.topRightCorner<3,1>()+T_GI.topRightCorner<3,1>();
  const Eigen::Matrix3d R_LG=R_GL.transpose();
  out.point_lidar_end=R_LG*(point_global-t_GL);
  if(!manager.projectUndistorted(frame,points,out.point_lidar_end,out.point_lidar_acquisition,
                                 out.uv,out.distortion_index,true))return out;
  if(out.point_lidar_acquisition.norm()<min_range||out.point_lidar_acquisition.norm()>max_range)return out;
  if(!(out.uv.x()>margin&&out.uv.x()<frame.intensity.cols-margin&&
       out.uv.y()>margin&&out.uv.y()<frame.intensity.rows-margin))return out;
  if(frame.mask.ptr<uchar>(static_cast<int>(out.uv.y()))[static_cast<int>(out.uv.x())]==0)return out;
  if(out.distortion_index<0||static_cast<std::size_t>(out.distortion_index)>=points.size())return out;
  const int transform_index=frame.vec_idx.empty()?0:frame.vec_idx.at(static_cast<std::size_t>(out.distortion_index));
  if(transform_index<0||static_cast<std::size_t>(transform_index)>=frame.T_Li_Lk_vec.size())return out;
  const Eigen::Matrix4d& T_Li_Lk=frame.T_Li_Lk_vec.at(static_cast<std::size_t>(transform_index));
  const double intensity=sampleFloat(frame.intensity,out.uv.x(),out.uv.y());
  const Eigen::Vector2d image_gradient=centralImageGradient(frame.intensity,out.uv.x(),out.uv.y());
  const Eigen::Matrix<double,1,2> dI_du=image_gradient.transpose();
  const Mat23 du_dp=projector.projectionJacobian(out.point_lidar_acquisition);

  const Eigen::Matrix3d R_Li_I=T_Li_Lk.topLeftCorner<3,3>()*T_IL.topLeftCorner<3,3>().transpose();
  const Vec3 p_I=R_IG*(point_global-T_GI.topRightCorner<3,1>());
  Eigen::Matrix<double,3,6> dp_dcorrection;
  dp_dcorrection.leftCols<3>()=R_Li_I*R_IG;
  dp_dcorrection.rightCols<3>()=-R_Li_I*skew(p_I);
  out.correction_jacobian_coin=dI_du*du_dp*dp_dcorrection;
  out.correction_jacobian_super<<out.correction_jacobian_coin.segment<3>(3),
                                  out.correction_jacobian_coin.segment<3>(0);
  out.residual=intensity-reference_intensity;
  out.valid=true;
  return out;
}

double CoinPhotometricModel::evaluateFixed(const CoinOusterProjector& projector,const CoinFrame& frame,
    const std::vector<CoinScanPoint>& points,const Eigen::Matrix4d& T_GI,const Eigen::Matrix4d& T_IL,
    const Vec3& point_global,double reference_intensity,int distortion_index){
  if(distortion_index<0||static_cast<std::size_t>(distortion_index)>=points.size())
    return std::numeric_limits<double>::quiet_NaN();
  const int transform_index=frame.vec_idx.empty()?0:frame.vec_idx.at(static_cast<std::size_t>(distortion_index));
  if(transform_index<0||static_cast<std::size_t>(transform_index)>=frame.T_Li_Lk_vec.size())
    return std::numeric_limits<double>::quiet_NaN();
  const Eigen::Matrix3d R_GI=T_GI.topLeftCorner<3,3>();
  const Eigen::Matrix3d R_GL=R_GI*T_IL.topLeftCorner<3,3>();
  const Vec3 t_GL=R_GI*T_IL.topRightCorner<3,1>()+T_GI.topRightCorner<3,1>();
  const Vec3 p_Lk=R_GL.transpose()*(point_global-t_GL);
  const Eigen::Matrix4d& T_Li_Lk=frame.T_Li_Lk_vec.at(static_cast<std::size_t>(transform_index));
  const Vec3 p_Li=T_Li_Lk.topLeftCorner<3,3>()*p_Lk+T_Li_Lk.topRightCorner<3,1>();
  const ProjectedPoint projection=projector.project(p_Li);
  if(!projection.in_fov)return std::numeric_limits<double>::quiet_NaN();
  return sampleFloat(frame.intensity,projection.uv.x(),projection.uv.y())-reference_intensity;
}

Eigen::Matrix4d CoinPhotometricModel::applyCoinCorrection(const Eigen::Matrix4d& T_GI,
    const Eigen::Matrix<double,6,1>& delta){
  Eigen::Matrix4d corrected=T_GI;
  const Eigen::Vector3d rotation=delta.tail<3>();
  const double angle=rotation.norm();
  if(angle>0.)corrected.topLeftCorner<3,3>()=T_GI.topLeftCorner<3,3>()*
      Eigen::AngleAxisd(angle,rotation/angle).toRotationMatrix();
  corrected.topRightCorner<3,1>()+=delta.head<3>();
  return corrected;
}

} // namespace cube::coin
````

### src/super_lio/src/intensity/cube_image.cpp

Git blob `ce4bf20364b07c0d3f918a4dbf6ded7bf2d2d65f`；SHA-256 `1d89dd2440c4b8d953a61750f3522c7f0513d279251a618f3218aea5b77131a1`；154 行完整文件。

````cpp
// CUBE-LIO independent implementation, GPL-3.0-or-later.
#include "intensity/cube_image.hpp"
#include <opencv2/imgproc.hpp>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <algorithm>
#include <chrono>

namespace cube {
namespace {
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
}
void Settings::validate() const {
  if(resolution<8||resolution>1024||idw_radius<1||idw_radius>12||idw_k<1||idw_k>625||
     idw_min_support<1||idw_min_support>idw_k||idw_power<=0||range_absolute<=0||
     range_relative<0||gaussian_sigma<=0||max_features<1||max_features>20000||
     max_lifetime<1||suppression_radius<0||normalization_frames<1||high_response<0||
     weight<0||sigma_min<=0||robust_gate<=0||huber_delta<=0)
    throw std::invalid_argument("invalid photometric configuration");
}
CubeImage::CubeImage(const Settings& cfg,MeasurementChannel channel)
  :projector(cfg.resolution),cfg_(cfg),channel_(channel){cfg_.validate();}
bool CubeImage::project(const Vec3& point,Projection& projection) const{
  projection=projector.project(point);
  return projection.face>=0;
}
void CubeImage::build(const std::vector<ScanPoint>& points){
  auto t=Clock::now();const int n=cfg_.resolution;
  candidate_points_.clear();
  for(auto& f:faces_){
    f.intensity=cv::Mat::zeros(n,n,CV_64F);f.depth=cv::Mat::zeros(n,n,CV_64F);
    f.raw_mask=cv::Mat::zeros(n,n,CV_8U);f.point_index.assign(n*n,-1);
  }
  for(size_t i=0;i<points.size();++i){
    const auto& p=points[i];auto q=projector.project(p.p);
    if(q.face<0||!std::isfinite(p.intensity))continue;
    int u=std::clamp(int(std::lround(q.uv.x())),0,n-1),v=std::clamp(int(std::lround(q.uv.y())),0,n-1);
    auto& f=faces_[q.face];double r=p.p.norm();
    if(!f.raw_mask.at<uint8_t>(v,u)||r<f.depth.at<double>(v,u)){
      f.intensity.at<double>(v,u)=p.intensity;f.depth.at<double>(v,u)=r;
      f.raw_mask.at<uint8_t>(v,u)=255;f.point_index[v*n+u]=int(i);
    }
  }
  raster_ms=ms(t);t=Clock::now();
  tbb::parallel_for(0,6,[&](int i){auto& f=faces_[i];f.mask=f.raw_mask.clone();if(cfg_.idw_enable)fill(f);});
  idw_ms=ms(t);t=Clock::now();
  if(cfg_.build_igm&&channel_==MeasurementChannel::IntensityGradientMagnitude)
    tbb::parallel_for(0,6,[&](int i){gradients(faces_[i]);});
  igm_ms=ms(t);
  raw_pixels=filled_pixels=valid_igm_pixels=0;
  for(const auto& f:faces_){
    raw_pixels+=cv::countNonZero(f.raw_mask);filled_pixels+=cv::countNonZero(f.mask);
    valid_igm_pixels+=cv::countNonZero(f.igm_mask);
    for(const int index:f.point_index)if(index>=0)candidate_points_.push_back(index);
  }
}
void CubeImage::fill(Face& f){
  const int n=cfg_.resolution,radius=cfg_.idw_radius;
  const cv::Mat raw_i=f.intensity.clone(),raw_d=f.depth.clone();
  // Only raw measurements support filling; no recursive extrapolation.
  tbb::parallel_for(tbb::blocked_range<int>(0,n),[&](const tbb::blocked_range<int>& rows){
    struct Neighbor{double d2,range,intensity;};
    std::vector<Neighbor> neighbors;neighbors.reserve((2*radius+1)*(2*radius+1));
    for(int v=rows.begin();v<rows.end();++v)for(int u=0;u<n;++u){
      if(f.raw_mask.at<uint8_t>(v,u))continue;
      neighbors.clear();
      for(int y=std::max(0,v-radius);y<=std::min(n-1,v+radius);++y)
        for(int x=std::max(0,u-radius);x<=std::min(n-1,u+radius);++x){
          int d2=(x-u)*(x-u)+(y-v)*(y-v);
          if(d2<=radius*radius&&f.raw_mask.at<uint8_t>(y,x))
            neighbors.push_back({double(d2),raw_d.at<double>(y,x),raw_i.at<double>(y,x)});
        }
      if(int(neighbors.size())<cfg_.idw_min_support)continue;
      std::sort(neighbors.begin(),neighbors.end(),[](auto a,auto b){if(a.d2==b.d2)return a.range<b.range;return a.d2<b.d2;});
      const double anchor=neighbors.front().range;
      double wsum=0,isum=0,dsum=0;int support=0;
      // A competing depth layer invalidates the fill rather than smearing edges.
      bool discontinuity=false;
      for(int k=0;k<std::min(int(neighbors.size()),cfg_.idw_k);++k){
        const auto& p=neighbors[k];
        if(std::abs(p.range-anchor)>cfg_.range_absolute+cfg_.range_relative*anchor){discontinuity=true;break;}
        double w=1./std::pow(p.d2,.5*cfg_.idw_power);
        wsum+=w;isum+=w*p.intensity;dsum+=w*p.range;++support;
      }
      if(discontinuity||support<cfg_.idw_min_support)continue;
      f.intensity.at<double>(v,u)=isum/wsum;f.depth.at<double>(v,u)=dsum/wsum;f.mask.at<uint8_t>(v,u)=255;
    }
  });
}
void CubeImage::gradients(Face& f){
  const int n=cfg_.resolution;cv::Mat smooth,smooth_mask=cv::Mat::zeros(n,n,CV_8U);
  // Equivalent Gaussian derivative: fixed 3x3 Gaussian followed by central difference.
  cv::GaussianBlur(f.intensity,smooth,cv::Size(3,3),cfg_.gaussian_sigma,cfg_.gaussian_sigma,cv::BORDER_CONSTANT);
  for(int v=1;v<n-1;++v)for(int u=1;u<n-1;++u){
    if(!f.mask.at<uint8_t>(v,u))continue;
    double anchor=f.depth.at<double>(v,u);bool valid=true;
    for(int y=v-1;y<=v+1&&valid;++y)for(int x=u-1;x<=u+1;++x)
      if(!f.mask.at<uint8_t>(y,x)||std::abs(f.depth.at<double>(y,x)-anchor)>cfg_.range_absolute+cfg_.range_relative*anchor){valid=false;break;}
    if(valid)smooth_mask.at<uint8_t>(v,u)=255;
  }
  f.igm=cv::Mat::zeros(n,n,CV_64F);f.igm_mask=cv::Mat::zeros(n,n,CV_8U);
  for(int v=2;v<n-2;++v)for(int u=2;u<n-2;++u){
    if(!smooth_mask.at<uint8_t>(v,u)||!smooth_mask.at<uint8_t>(v,u-1)||
       !smooth_mask.at<uint8_t>(v,u+1)||!smooth_mask.at<uint8_t>(v-1,u)||!smooth_mask.at<uint8_t>(v+1,u))continue;
    double dx=.5*(smooth.at<double>(v,u+1)-smooth.at<double>(v,u-1));
    double dy=.5*(smooth.at<double>(v+1,u)-smooth.at<double>(v-1,u));
    f.igm.at<double>(v,u)=std::hypot(dx,dy);f.igm_mask.at<uint8_t>(v,u)=255;
  }
  // Diagnostic pixel gradients; residual sampling differentiates its exact bilinear interpolant.
  cv::Sobel(f.igm,f.grad_u,CV_64F,1,0,1,.5,0,cv::BORDER_CONSTANT);
  cv::Sobel(f.igm,f.grad_v,CV_64F,0,1,1,.5,0,cv::BORDER_CONSTANT);
}
bool CubeImage::sample(const Projection& q,Sample& s)const{
  if(q.face<0||q.seam||!q.uv.allFinite())return false;
  int u=int(std::floor(q.uv.x())),v=int(std::floor(q.uv.y()));const int n=cfg_.resolution;
  if(u<2||v<2||u>=n-3||v>=n-3)return false;
  const auto& f=faces_[q.face];
  const bool use_igm=channel_==MeasurementChannel::IntensityGradientMagnitude;
  const cv::Mat& value_mask=use_igm?f.igm_mask:f.mask;
  for(int y=v;y<=v+1;++y)for(int x=u;x<=u+1;++x)if(!value_mask.at<uint8_t>(y,x))return false;
  double x=q.uv.x()-u,y=q.uv.y()-v;
  const cv::Mat& image=use_igm?f.igm:f.intensity;
  double a=image.at<double>(v,u),b=image.at<double>(v,u+1),c=image.at<double>(v+1,u),d=image.at<double>(v+1,u+1);
  s.value=(1-y)*((1-x)*a+x*b)+y*((1-x)*c+x*d);
  s.gradient << (1-y)*(b-a)+y*(d-c),(1-x)*(c-a)+x*(d-b);
  double depths[4]={f.depth.at<double>(v,u),f.depth.at<double>(v,u+1),f.depth.at<double>(v+1,u),f.depth.at<double>(v+1,u+1)};
  const auto mm=std::minmax_element(depths,depths+4);
  if(*mm.second-*mm.first>cfg_.range_absolute+cfg_.range_relative*(*mm.first))return false;
  s.depth=(1-y)*((1-x)*depths[0]+x*depths[1])+y*((1-x)*depths[2]+x*depths[3]);
  return std::isfinite(s.value)&&s.gradient.allFinite();
}
bool CubeImage::sampleIntensity(const Projection& q,double& intensity) const{
  Sample sample;
  if(!this->sample(q,sample))return false;
  intensity=sample.value;return true;
}
bool CubeImage::sampleGradient(const Projection& q,Eigen::Vector2d& gradient) const{
  Sample sample;
  if(!this->sample(q,sample))return false;
  gradient=sample.gradient.transpose();return true;
}
bool CubeImage::computeResidual(const Projection& q,double reference,double& residual) const{
  double current=0.;
  if(!std::isfinite(reference)||!sampleIntensity(q,current))return false;
  residual=current-reference;return std::isfinite(residual);
}
bool CubeImage::validityCheck(const Projection& q,double expected_depth,double range_absolute,
                              double range_relative,Sample& sample) const{
  if(!this->sample(q,sample)||!std::isfinite(expected_depth)||
     std::abs(expected_depth-sample.depth)>range_absolute+range_relative*sample.depth)return false;
  return true;
}
} // namespace cube
````

### src/super_lio/src/intensity/photo_observation.cpp

Git blob `59f6e7d73d125cb920ea85af0df0e8286de34ab9`；SHA-256 `38c21be7c43d19b1af312c20c87c89652edba459f4c5f66aa38ed95b4f25104d`；399 行完整文件。

````cpp
// Independent GPLv3 implementation; see photo_observation.hpp for provenance.
#include "intensity/photo_observation.hpp"
#include "intensity/spherical_image.hpp"
#include "intensity/coin/coin_feature_manager.hpp"
#include "intensity/coin/super_degeneracy_gate.hpp"
#include "lio/params.h"
#include <Eigen/Eigenvalues>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <numeric>
#include <chrono>
#include <iomanip>

namespace cube {
namespace {
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
double median(std::vector<double> v){
  if(v.empty())return 0;auto k=v.begin()+v.size()/2;std::nth_element(v.begin(),k,v.end());return *k;
}
struct Acc {PhotoTerms terms;std::vector<double> residuals;};
}
PhotoObservation::PhotoObservation(ros::NodeHandle& nh){
#define LOAD(group,key,field) nh.param(group "/" key,cfg_.field,cfg_.field)
  LOAD("/photo","enable",enable);LOAD("/photo","weight",weight);
  LOAD("/photo","max_features",max_features);LOAD("/photo","max_lifetime",max_lifetime);
  LOAD("/photo","high_response",high_response);LOAD("/photo","suppression_radius",suppression_radius);
  LOAD("/photo","normalization_frames",normalization_frames);LOAD("/photo","sigma_min",sigma_min);
  LOAD("/photo","robust_gate",robust_gate);LOAD("/photo","huber_delta",huber_delta);
  LOAD("/cubemap","resolution",resolution);LOAD("/cubemap","idw_enable",idw_enable);
  LOAD("/cubemap","idw_radius",idw_radius);LOAD("/cubemap","idw_k",idw_k);
  LOAD("/cubemap","idw_power",idw_power);LOAD("/cubemap","idw_min_support",idw_min_support);
  LOAD("/cubemap","range_absolute",range_absolute);LOAD("/cubemap","range_relative",range_relative);
  LOAD("/cubemap","gaussian_sigma",gaussian_sigma);
#undef LOAD
  nh.param<std::string>("/photo/projection",projection_name_,"cubemap");
  nh.param<std::string>("/photo/measurement",measurement_name_,"igm");
  nh.param<std::string>("/photo/selector",selector_mode_,"all");
  nh.param("/photo/weakest_gate_threshold",weakest_gate_threshold_,0.31913064578672057);
  if(projection_name_!="cubemap"&&projection_name_!="equirectangular")
    throw std::invalid_argument("unknown intensity projection: "+projection_name_);
  if(measurement_name_!="raw"&&measurement_name_!="igm")
    throw std::invalid_argument("unknown intensity measurement channel: "+measurement_name_);
  if(selector_mode_!="all"&&selector_mode_!="weakest")
    throw std::invalid_argument("unknown photometric selector: "+selector_mode_);
  if(!std::isfinite(weakest_gate_threshold_)||weakest_gate_threshold_<0.)
    throw std::invalid_argument("photo weakest-direction gate threshold must be finite and nonnegative");
  cfg_.build_igm=measurement_name_=="igm";
  cfg_.validate();sigma_=cfg_.sigma_min;
  std::string policy;nh.param<std::string>("/photo/information_policy",policy,"C0");
  policy_=parsePolicy(policy);
  nh.param("/photo/information_audit",audit_enabled_,false);
  nh.param("/p3r/photo_time_audit",time_audit_enabled_,false);
  nh.param("/p3r/photo_history_supported_only",history_supported_only_,false);
  if(!cfg_.enable)return;
  cv::setNumThreads(1); // TBB owns frontend parallelism; avoid nested OpenCV pools.
  R_BL_=LI2Sup::g_lidar_imu.R_.cast<double>();t_BL_=LI2Sup::g_lidar_imu.t_.cast<double>();
  const MeasurementChannel channel=cfg_.build_igm?
      MeasurementChannel::IntensityGradientMagnitude:MeasurementChannel::RawIntensity;
  if(projection_name_=="cubemap")image_=std::make_unique<CubeImage>(cfg_,channel);
  else image_=std::make_unique<SphericalImage>(cfg_,channel);
  std::string dir;nh.getParam("/lio/offline/out_dir",dir);
  if(time_audit_enabled_&&dir.empty())
    throw std::invalid_argument("P3-R time audit requires an offline output directory");
  if(!dir.empty()){
    if(time_audit_enabled_){
      time_audit_.open(dir+"/photo_time_audit.csv");
      if(!time_audit_)throw std::runtime_error("cannot write P3-R photo time audit");
      time_audit_<<std::setprecision(17);
      time_audit_<<"frame,scan_start,scan_end_geometry,history_first,history_last,history_states,frame_supported,geometry_points,photo_points_total,photo_points_inside_history,photo_points_outside_history,photo_points_before_history,photo_points_after_history,photo_points_invalid_time,photo_points_without_history,photo_points_retained,photo_points_dropped,deskew_success_points,fallback_deskew_points,fallback_percentage,min_photo_offset_s,max_photo_offset_s,max_photo_timestamp,history_supported_only\n";
    }
    if(audit_enabled_){
      audit_.open(dir+"/information.csv");
      if(!audit_)throw std::runtime_error("cannot write information audit");
      audit_<<std::setprecision(17);
    }
    diagnostics_.open(dir+"/photo.csv");
    if(!diagnostics_)throw std::runtime_error("cannot write photometric diagnostics");
    diagnostics_<<std::setprecision(17);
    diagnostics_<<"frame,timestamp,projection,measurement,selector,weakest_gate_confidence,weakest_gate_active,active_before,active_after,valid,residual_mean,residual_rms,residual_median,reject_fov,reject_invalid,reject_range,reject_outlier,sigma,frozen,trace_Ag,trace_Ap,weak_translation_eigenvalue,photo_in_weak_translation,weak_x,weak_y,weak_z,geo_eig0,geo_eig1,geo_eig2,geo_eig3,geo_eig4,geo_eig5,deskew_ms,raster_ms,idw_ms,igm_ms,photo_jacobian_ms,update_ms,replenish_ms,raw_points,raw_pixels,filled_pixels,igm_pixels,deskew_supported\n";
  }
}
void PhotoObservation::prepare(const LI2Sup::MeasureGroup& measures,
                              const std::vector<LI2Sup::DynamicState>& history,const BASIC::SE3& predicted){
  if(!cfg_.enable)return;
  ++frame_;audit_iteration_=0;predicted_pose_=predicted;photo_ms_=0;auto start=Clock::now();
  const auto& raw=measures.lidar.pc_intensity;
  if(!raw)throw std::runtime_error("missing dense intensity scan");
  frame_supported_=history.size()>=2 && history.back().time>history.front().time;
  std::vector<size_t> supported_indices;
  size_t inside=0,before=0,after=0,invalid_time=0;
  double minimum_offset=raw->empty()?0.:raw->points.front().offset_time;
  double maximum_offset=minimum_offset;
  if(time_audit_enabled_||history_supported_only_){
    if(history_supported_only_)supported_indices.reserve(raw->size());
    for(size_t i=0;i<raw->size();++i){
      const double offset=raw->points[i].offset_time;
      const double time=measures.lidar.start_time+offset;
      minimum_offset=std::min(minimum_offset,offset);
      maximum_offset=std::max(maximum_offset,offset);
      if(!std::isfinite(time)){++invalid_time;continue;}
      if(!frame_supported_)continue;
      if(time<history.front().time)++before;
      else if(time>history.back().time)++after;
      else{
        ++inside;
        if(history_supported_only_)supported_indices.push_back(i);
      }
    }
  }
  const auto write_time_audit=[&](size_t retained,size_t deskew_success){
    if(!time_audit_enabled_)return;
    const size_t fallback=frame_supported_?retained-deskew_success:0;
    time_audit_<<frame_<<','<<measures.lidar.start_time<<','<<measures.lidar.end_time<<','
      <<(history.empty()?0.:history.front().time)<<','<<(history.empty()?0.:history.back().time)<<','
      <<history.size()<<','<<frame_supported_<<','<<(measures.lidar.pc?measures.lidar.pc->size():0)<<','
      <<raw->size()<<','<<inside<<','<<raw->size()-inside<<','<<before<<','<<after<<','<<invalid_time<<','
      <<(frame_supported_?0:raw->size())<<','<<retained<<','<<raw->size()-retained<<','<<deskew_success<<','
      <<fallback<<','<<(retained?100.*fallback/retained:0.)<<','<<minimum_offset<<','<<maximum_offset<<','
      <<measures.lidar.start_time+maximum_offset<<','<<history_supported_only_<<'\n';
  };
  if(!frame_supported_){
    // An inherited synchronization outcome, e.g. a bag IMU gap. Keep the
    // geometry update and output frame; never constrain it with a stale image.
    write_time_audit(0,0);
    points_.clear();deskew_ms_=0;update_start_=Clock::now();return;
  }
  // C arm changes only the photo input set; raw order and deskew equations stay fixed.
  points_.resize(history_supported_only_?supported_indices.size():raw->size());
  std::vector<uint8_t> deskew_success;
  if(time_audit_enabled_)deskew_success.assign(points_.size(),0);
  const Mat3 R_end=predicted.R_.cast<double>();const Vec3 t_end=predicted.t_.cast<double>();
  tbb::parallel_for(tbb::blocked_range<size_t>(0,points_.size()),[&](const tbb::blocked_range<size_t>& rows){
    for(size_t i=rows.begin();i<rows.end();++i){
      const auto& p=raw->points[history_supported_only_?supported_indices[i]:i];Vec3 p_L(p.x,p.y,p.z);const Vec3 p_B=R_BL_*p_L+t_BL_;
      const double time=measures.lidar.start_time+p.offset_time;
      Vec3 corrected=p_B;
      if(time>=history.front().time&&time<=history.back().time){
        auto next=std::upper_bound(history.begin(),history.end(),time,[](double t,const LI2Sup::DynamicState& s){return t<s.time;});
        if(next==history.end())--next;
        const auto& tail=*next;const auto& head=*std::prev(next);
        double dt=tail.time-head.time,tau=time-head.time;
        if(dt>0){
          Eigen::Quaterniond r0(head.R.cast<double>()),r1(tail.R.cast<double>());
          const Mat3 R_i=r0.slerp(tau/dt,r1).toRotationMatrix();
          Vec3 t_i=head.p.cast<double>()+head.v.cast<double>()*tau+.5*tail.a.cast<double>()*tau*tau;
          corrected=R_end.transpose()*(R_i*p_B+t_i-t_end);
          if(time_audit_enabled_)deskew_success[i]=1;
        }
      }
      // Return from upstream deskew's IMU-end frame to sensor-centric LiDAR-end.
      points_[i]={R_BL_.transpose()*(corrected-t_BL_),p.intensity};
    }
  });
  if(time_audit_enabled_)
    write_time_audit(points_.size(),std::accumulate(deskew_success.begin(),deskew_success.end(),size_t{0}));
  deskew_ms_=elapsed(start);image_->build(points_);
  if(!frozen_){
    std::vector<double> samples;observe(predicted,false,&samples);
    early_residuals_.insert(early_residuals_.end(),samples.begin(),samples.end());
    if(frame_>=size_t(cfg_.normalization_frames)&&early_residuals_.size()>=30){
      double center=median(early_residuals_);std::vector<double> deviations;
      for(double r:early_residuals_)deviations.push_back(std::abs(r-center));
      sigma_=std::max(cfg_.sigma_min,1.4826*median(deviations));frozen_=true;early_residuals_.clear();
    }
    // Bound startup samples if sparse scenes never reach the minimum count.
    if(early_residuals_.size()>size_t(cfg_.max_features*cfg_.normalization_frames))
      early_residuals_.erase(early_residuals_.begin(),early_residuals_.begin()+cfg_.max_features);
  }
  update_start_=Clock::now();
}
PhotoTerms PhotoObservation::observe(const BASIC::SE3& pose,bool weighted,std::vector<double>* residuals)const{
  const Mat3 R=pose.R_.cast<double>();const Vec3 t=pose.t_.cast<double>();
  constexpr size_t kFeatureBlock=32;
  const size_t block_count=(features_.size()+kFeatureBlock-1)/kFeatureBlock;
  std::vector<Acc> accumulators(block_count);
  tbb::parallel_for(tbb::blocked_range<size_t>(0,block_count,1),[&](const tbb::blocked_range<size_t>& blocks){
    for(size_t block=blocks.begin();block<blocks.end();++block){
      auto& a=accumulators[block];
      const size_t begin=block*kFeatureBlock,end=std::min(features_.size(),begin+kFeatureBlock);
      for(size_t i=begin;i<end;++i){
      const auto& feat=features_[i];
      const Vec3 p_L=landmarkInLidar(feat.world,R,t,R_BL_,t_BL_);Projection q;
      if(!image_->project(p_L,q)||q.face<0||q.seam||p_L.norm()<std::sqrt(LI2Sup::g_blind2)||p_L.norm()>std::sqrt(LI2Sup::g_maxrange2)){++a.terms.fov;continue;}
      Sample sample;
      if(!image_->validityCheck(q,p_L.norm(),cfg_.range_absolute,cfg_.range_relative,sample)){
        if(sample.depth>0.&&std::abs(p_L.norm()-sample.depth)>cfg_.range_absolute+cfg_.range_relative*sample.depth)
          ++a.terms.range;
        else ++a.terms.invalid;
        continue;
      }
      double residual=0.;
      if(!image_->computeResidual(q,feat.reference,residual)){++a.terms.invalid;continue;}
      if(weighted&&frozen_&&std::abs(residual)>cfg_.robust_gate*sigma_){++a.terms.outlier;continue;}
      ++a.terms.valid;a.residuals.push_back(residual);
      if(weighted&&frozen_){
        const Row6 J=residualJacobian(feat.world,R,t,R_BL_,t_BL_,q,sample);
        const double z=std::abs(residual)/sigma_;
        const double robust=z<=cfg_.huber_delta?1.:cfg_.huber_delta/z;
        const double w=cfg_.weight*robust/(sigma_*sigma_);
        a.terms.A.noalias()+=w*J.transpose()*J;
        a.terms.b.noalias()-=w*J.transpose()*residual; // Exact upstream -J^T W r sign.
      }
      }
    }
  });
  PhotoTerms total;std::vector<double> values;
  for(const auto& a:accumulators){
    total.A+=a.terms.A;total.b+=a.terms.b;total.valid+=a.terms.valid;
    total.fov+=a.terms.fov;total.invalid+=a.terms.invalid;total.range+=a.terms.range;total.outlier+=a.terms.outlier;
    values.insert(values.end(),a.residuals.begin(),a.residuals.end());
  }
  if(!values.empty()){
    double square=0;for(double v:values){total.mean+=v;square+=v*v;}
    total.mean/=values.size();total.rms=std::sqrt(square/values.size());total.median=median(values);
  }
  if(residuals)*residuals=std::move(values);
  return total;
}
std::vector<ResidualContribution> PhotoObservation::auditRows(const BASIC::SE3& pose)const{
  if(!frame_supported_)return {};
  const Mat3 R=pose.R_.cast<double>();const Vec3 t=pose.t_.cast<double>();
  std::vector<ResidualContribution> slots(features_.size());
  std::vector<uint8_t> valid(features_.size(),0); // Separate bytes, never packed bits.
  tbb::parallel_for(tbb::blocked_range<size_t>(0,features_.size()),[&](const tbb::blocked_range<size_t>& rows){
    for(size_t i=rows.begin();i<rows.end();++i){
      const auto& feat=features_[i];
      const Vec3 p_L=landmarkInLidar(feat.world,R,t,R_BL_,t_BL_);Projection q;
      if(!image_->project(p_L,q)||q.face<0||q.seam||
         p_L.norm()<std::sqrt(LI2Sup::g_blind2)||p_L.norm()>std::sqrt(LI2Sup::g_maxrange2))continue;
      Sample sample;
      if(!image_->validityCheck(q,p_L.norm(),cfg_.range_absolute,cfg_.range_relative,sample))continue;
      double residual=0.;if(!image_->computeResidual(q,feat.reference,residual))continue;
      if(frozen_&&std::abs(residual)>cfg_.robust_gate*sigma_)continue;
      double weight=0;
      if(frozen_){
        const double z=std::abs(residual)/sigma_;
        const double robust=z<=cfg_.huber_delta?1.:cfg_.huber_delta/z;
        weight=cfg_.weight*robust/(sigma_*sigma_);
      }
      slots[i]={i,q.face,q.uv,feat.reference,residual,weight,
                residualJacobian(feat.world,R,t,R_BL_,t_BL_,q,sample)};
      valid[i]=1;
    }
  });
  std::vector<ResidualContribution> out;
  for(size_t i=0;i<slots.size();++i)if(valid[i])out.push_back(slots[i]);
  return out;
}
void PhotoObservation::writeAudit(const BASIC::SE3& pose,const BASIC::M6& covariance,
                                 const Vec6& geometry_b,const std::vector<ResidualContribution>& rows){
  const Mat3 delta_R=predicted_pose_.R_.cast<double>().transpose()*pose.R_.cast<double>();
  const Eigen::AngleAxisd rotation(delta_R);
  Vec6 delta;delta.head<3>()=rotation.axis()*rotation.angle();
  delta.tail<3>()=(pose.t_-predicted_pose_.t_).cast<double>();
  Mat6 G=Mat6::Identity();G.topLeftCorner<3,3>()-=.5*hat(delta.head<3>());
  const Mat6 prior=G*covariance.cast<double>()*G.transpose();
  for(auto policy:{InformationPolicy::C0,InformationPolicy::C60,InformationPolicy::C100,InformationPolicy::K100}){
    auto terms=informationBudget(rows,policy,cfg_.suppression_radius);
    auto m=auditInformation(geometry_,geometry_b,prior,G*delta,rows,terms,
                             image_?image_->coordinateResolution():cfg_.resolution);
    m["active"]=double(features_.size());m["frozen"]=frozen_;m["supported"]=frame_supported_;
    // Comparison establishes that the diagnostic sampler retains P1 robust semantics.
    const auto raw=informationBudget(rows,InformationPolicy::C0,cfg_.suppression_radius);
    m["raw_A_relative_error"]=(raw.A-last_.A).norm()/std::max(1.,last_.A.norm());
    m["raw_b_relative_error"]=(raw.b-last_.b).norm()/std::max(1.,last_.b.norm());
    m["valid_count_error"]=double(rows.size())-last_.valid;
    Eigen::SelfAdjointEigenSolver<Mat6> difference(raw.A-terms.A);
    m["authority_difference_min_eigenvalue"]=difference.eigenvalues()[0];
    if(frame_==1&&audit_iteration_==0&&policy==InformationPolicy::C0){
      audit_<<"frame,iteration,policy";for(const auto& kv:m)audit_<<','<<kv.first;audit_<<'\n';
    }
    audit_<<frame_<<','<<audit_iteration_<<','<<policyName(policy);
    for(const auto& kv:m)audit_<<','<<kv.second;audit_<<'\n';
  }
}
void PhotoObservation::add(const BASIC::SE3& pose,BASIC::M6& A,BASIC::V6& b,const BASIC::M6& prior_covariance){
  if(!cfg_.enable)return;auto start=Clock::now();
  geometry_=A.cast<double>();const Vec6 geometry_b=b.cast<double>();
  if(!frame_supported_){last_=PhotoTerms{};last_.invalid=int(features_.size());}
  else last_=observe(pose,true); // Preserve P1 raw accumulation, including C0/shadow control.
  std::vector<ResidualContribution> rows;
  if(audit_enabled_||policy_!=InformationPolicy::C0)rows=auditRows(pose);
  if(audit_enabled_)writeAudit(pose,prior_covariance,geometry_b,rows);
  if(policy_!=InformationPolicy::C0){
    const auto terms=informationBudget(rows,policy_,cfg_.suppression_radius);
    last_.A=terms.A;last_.b=terms.b;
  }
  A+=last_.A.cast<BASIC::scalar>();b+=last_.b.cast<BASIC::scalar>();
  ++audit_iteration_;photo_ms_+=elapsed(start);
}
void PhotoObservation::replenish(const BASIC::SE3& pose,double timestamp,
                                 const Eigen::MatrixXd& geometry_translation_rows){
  const Mat3 R=pose.R_.cast<double>();const Vec3 t=pose.t_.cast<double>();
  Eigen::Vector3d weak_global=Eigen::Vector3d::Zero();bool weak_valid=false;
  if(selector_mode_=="weakest"){
    const Mat3 R_GL=R*R_BL_;
    const auto geometry=coin::CoinFeatureManager::weakDirectionsFromGeometry(
        geometry_translation_rows,R_GL,25.);
    const bool previous_supported=has_previous_weak_axis_&&timestamp>previous_weak_axis_timestamp_&&
        timestamp-previous_weak_axis_timestamp_<=.25;
    const auto signal=coin::SuperDegeneracyGate::measure(
        geometry,previous_weak_axis_global_,previous_supported);
    weakest_gate_confidence_=signal.confidence;
    weakest_gate_active_=coin::SuperDegeneracyGate::activate(signal,weakest_gate_threshold_);
    if(geometry.geometry_rows>3&&geometry.eigenvectors.col(0).allFinite()){
      previous_weak_axis_global_=geometry.eigenvectors.col(0).normalized();
      previous_weak_axis_timestamp_=timestamp;has_previous_weak_axis_=true;
    }
    if(weakest_gate_active_){weak_global=geometry.eigenvectors.col(0).normalized();weak_valid=weak_global.allFinite();}
  }
  std::vector<Feature> surviving;std::vector<Projection> centers;
  for(const auto& f:features_){
    if(frame_-f.birth_frame>=size_t(cfg_.max_lifetime))continue;
    Vec3 p_L=landmarkInLidar(f.world,R,t,R_BL_,t_BL_);Projection q;Sample s;
    if(!image_->project(p_L,q)||!image_->validityCheck(q,p_L.norm(),cfg_.range_absolute,
                                                       cfg_.range_relative,s))continue;
    if(frozen_&&std::abs(s.value-f.reference)>cfg_.robust_gate*sigma_)continue;
    surviving.push_back(f);centers.push_back(q);
  }
  features_=std::move(surviving);
  std::vector<cv::Mat> suppression(image_->chartCount());
  for(int chart=0;chart<image_->chartCount();++chart)
    suppression[chart]=cv::Mat::zeros(image_->chartHeight(chart),image_->chartWidth(chart),CV_8U);
  auto suppress=[&](const Projection& q){
    if(q.face<0||q.face>=image_->chartCount())return;
    const int width=image_->chartWidth(q.face),height=image_->chartHeight(q.face);
    const int u=static_cast<int>(q.uv.x()),v=static_cast<int>(q.uv.y()),radius=cfg_.suppression_radius;
    for(int y=std::max(0,v-radius);y<=std::min(height-1,v+radius);++y)
      for(int dx=-radius;dx<=radius;++dx){
        int x=u+dx;
        if(image_->wrapsHorizontally(q.face)){x%=width;if(x<0)x+=width;}
        else if(x<0||x>=width)continue;
        suppression[q.face].at<uint8_t>(y,x)=255;
      }
  };
  for(const auto& q:centers)suppress(q);
  struct Candidate{int index;Projection projection;double reference,response,rank;};std::vector<Candidate> candidates;
  const bool use_igm=measurement_name_=="igm";
  for(const int index:image_->candidatePointIndices()){
    if(index<0||static_cast<std::size_t>(index)>=points_.size())continue;
    Projection q;Sample sample;
    if(!image_->project(points_[index].p,q)||q.face<0||q.face>=image_->chartCount()||
       !image_->validityCheck(q,points_[index].p.norm(),cfg_.range_absolute,
                              cfg_.range_relative,sample))continue;
    Eigen::Vector2d gradient=sample.gradient.transpose();
    const double response=use_igm?sample.value:gradient.norm();
    if(response>=cfg_.high_response){
      double rank=response;
      if(selector_mode_=="weakest"&&weak_valid){
        const Vec3 delta_pixel=-R_BL_.transpose()*R.transpose()*weak_global;
        const Eigen::Vector2d projected_motion=q.jacobian*delta_pixel;
        rank=std::abs((sample.gradient*projected_motion).value());
      }
      candidates.push_back({index,q,sample.value,response,rank});
    }
  }
  std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b){
    return a.rank==b.rank?a.index<b.index:a.rank>b.rank;
  });
  for(const auto& c:candidates){
    if(features_.size()>=size_t(cfg_.max_features))break;
    const auto& q=c.projection;
    int u=static_cast<int>(q.uv.x()),v=static_cast<int>(q.uv.y());
    if(image_->wrapsHorizontally(q.face)){u%=image_->chartWidth(q.face);if(u<0)u+=image_->chartWidth(q.face);}
    if(u<0||u>=image_->chartWidth(q.face)||v<0||v>=image_->chartHeight(q.face)||
       suppression[q.face].at<uint8_t>(v,u))continue;
    Vec3 p_B=R_BL_*points_[c.index].p+t_BL_;
    features_.push_back({R*p_B+t,c.reference,timestamp,frame_,q.face});suppress(q);
  }
}
void PhotoObservation::finish(const BASIC::SE3& pose,double timestamp,
                              const Eigen::MatrixXd& geometry_translation_rows){
  if(!cfg_.enable)return;
  update_ms_=elapsed(update_start_);auto start=Clock::now();size_t active=features_.size();
  weakest_gate_confidence_=0.;weakest_gate_active_=false;
  if(frame_supported_)replenish(pose,timestamp,geometry_translation_rows);
  replenish_ms_=elapsed(start);
  Eigen::SelfAdjointEigenSolver<Mat6> eig6(geometry_);
  Eigen::SelfAdjointEigenSolver<Mat3> eig3(geometry_.bottomRightCorner<3,3>());
  Vec3 weak=eig3.eigenvectors().col(0);double photo_weak=weak.dot(last_.A.bottomRightCorner<3,3>()*weak);
  if(diagnostics_){
    diagnostics_<<frame_<<','<<timestamp<<','<<projection_name_<<','<<measurement_name_<<','<<selector_mode_<<','
      <<weakest_gate_confidence_<<','<<weakest_gate_active_<<','
      <<active<<','<<features_.size()<<','<<last_.valid<<','
      <<last_.mean<<','<<last_.rms<<','<<last_.median<<','<<last_.fov<<','<<last_.invalid<<','<<last_.range<<','<<last_.outlier<<','
      <<sigma_<<','<<frozen_<<','<<geometry_.trace()<<','<<last_.A.trace()<<','<<eig3.eigenvalues()[0]<<','<<photo_weak;
    for(int k=0;k<3;++k)diagnostics_<<','<<weak[k];
    for(int k=0;k<6;++k)diagnostics_<<','<<eig6.eigenvalues()[k];
    diagnostics_<<','<<deskew_ms_<<','<<(frame_supported_?image_->rasterMilliseconds():0)<<','
      <<(frame_supported_?image_->interpolationMilliseconds():0)<<','
      <<(frame_supported_?image_->featureChannelMilliseconds():0)<<','
      <<photo_ms_<<','<<update_ms_<<','<<replenish_ms_<<','<<points_.size()<<','
      <<(frame_supported_?image_->rawPixelCount():0)<<','
      <<(frame_supported_?image_->filledPixelCount():0)<<','
      <<(frame_supported_?image_->validFeaturePixelCount():0)<<','<<frame_supported_<<'\n';
  }
}
} // namespace cube
````

### src/super_lio/src/lio/super_lio.cpp

Git blob `85a9ca5e30a27374d9d8a1cb321b329753c096e6`；SHA-256 `e4849b399058f1bbb8c9268a530fad3f42442185671936265d9f77af7d0fd660`；759 行完整文件。

````cpp

#include "lio/super_lio.h"
#include "lio/ouster_scan_history.hpp"

#include <sys/resource.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/concurrent_vector.h>
#include <tbb/enumerable_thread_specific.h>
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <limits>


using namespace BASIC;

namespace LI2Sup{

namespace {
bool auditHasAcquisitionBracket(const std::vector<DynamicState>& history,double time){
  if(history.size()<2||time>history.back().time)return false;
  auto tail=std::upper_bound(history.begin(),history.end(),time,
      [](double t,const DynamicState& s){return t<s.time;});
  if(tail==history.end())return history.back().R.allFinite()&&history.back().p.allFinite();
  auto head=tail==history.begin()?tail:std::prev(tail);
  if(head==tail){
    auto next=std::next(tail);if(next==history.end())return false;tail=next;
  }
  return tail->time>head->time&&head->R.allFinite()&&tail->R.allFinite()&&
    head->p.allFinite()&&head->v.allFinite()&&tail->a.allFinite();
}
}

inline bool calc_plane_coeff(const int N, const std::array<V3, 5>& points, std::array<double, 4>& abcd)
{
  Eigen::Vector3d normvec;
  if (N == 5) {
    Eigen::Matrix<double, 5, 3> A;
    Eigen::Matrix<double, 5, 1> b;
    for (int j = 0; j < 5; j++) {
      A.row(j) = points[j].cast<double>();
      b(j) = -1.0;
    }
    normvec = A.colPivHouseholderQr().solve(b);
  }
  else {
    Eigen::Matrix<double, 4, 3> A;
    Eigen::Matrix<double, 4, 1> b;

    for (int j = 0; j < N; j++) {
      A.row(j) = points[j].cast<double>();
      b(j) = -1.0;
    }
    normvec = A.colPivHouseholderQr().solve(b);
  }

  double n = normvec.norm();
  if (n < 1e-6f) return false;

  abcd[3] = 1.0 / n;
  normvec *= abcd[3];
  abcd[0] = normvec[0];
  abcd[1] = normvec[1];
  abcd[2] = normvec[2];
  
  for (int i = 0; i < N; ++i) {
    const V3& p = points[i];
    auto dist = abcd[0] * p(0) + abcd[1] * p(1) + abcd[2] * p(2) + abcd[3];
    if (std::abs(dist) > 0.1) return false;
  }
  return true;
}


inline bool compute_error(
  const std::array<double, 4>& abcd, const V3& point, 
  const float length, scalar& error)
{
  error = abcd[0] * point[0] + abcd[1] * point[1] + abcd[2] * point[2] + abcd[3];
  return length > 81 * error * error;
}


void SuperLIO::init(){
  ros::NodeHandle photo_nh;
  std::string time_audit_path;
  photo_nh.getParam("/p2r/time_audit_path",time_audit_path);
  if(!time_audit_path.empty()&&g_lidar_type==LID_TYPE::OUSTER){
    time_audit_.open(time_audit_path);
    if(!time_audit_)throw std::runtime_error("cannot write P2R time audit");
    time_audit_<<std::setprecision(17)
      <<"frame,phase,ros_message_stamp,max_t_all_raw_offset_s,max_t_valid_raw_offset_s,"
        "max_t_geometry_after_filter_rate_offset_s,current_lidar_end_time,proposed_full_scan_end_time,"
        "imu_last_available_to_sync,imu_last_consumed,propagated_history_first,propagated_history_last,"
        "raw_coin_points,raw_coin_points_after_current_end,raw_coin_points_after_history_back,"
        "current_motion_fallback_points,geometry_points,imu_bracket_span_s,imu_support_issue\n";
  }
  std::string geometry_rows_path;
  photo_nh.getParam("/p2s/geometry_rows_path",geometry_rows_path);
  if(!geometry_rows_path.empty()){
    geometry_rows_audit_.open(geometry_rows_path,std::ios::binary|std::ios::trunc);
    if(!geometry_rows_audit_)throw std::runtime_error("cannot write P2-S geometry row audit");
  }
  photo_ = std::make_unique<cube::PhotoObservation>(photo_nh);
  Eigen::Matrix4d T_IL=Eigen::Matrix4d::Identity();
  T_IL.topLeftCorner<3,3>()=g_lidar_imu.R_.cast<double>();
  T_IL.topRightCorner<3,1>()=g_lidar_imu.t_.cast<double>();
  coin_=std::make_unique<cube::coin::CoinObservation>(photo_nh,T_IL);
  ivox_.reset(new OctVoxMapType(OctVoxMapType::Options{g_ivox_resolution, g_ivox_capacity}));
  kf_.reset(new ESKF());
  data_wrapper_->setESKF(kf_);
  
  scan_undistort_full_.reset(new PointCloudType());
  ds_undistort_.reset(new PointCloudType());
  world_pc_.reset(new PointCloudType());
  ds_world_.reset(new PointCloudType());

  if(g_save_map){
    point_map_.reset(new PointCloudType());
  }
  
  points_world_v3_.reserve(21000);
  abcd_vec_.resize(20000);
  effect_knn_idxs_.resize(20000);
  voxel_grid_fliter_.setLeafSize(g_voxel_fliter_size);

  state_fn_ = &SuperLIO::stateWaitKFInit;

  LOG(INFO) << GREEN << " ---> [SuperLIO]: initialized." << RESET;
}


void SuperLIO::stateWaitKFInit()
{
  if (kf_init()) {
    state_fn_ = &SuperLIO::stateWaitMapInit;
    LOG(INFO) << GREEN << " ---> [SuperLIO]: KF init done" << RESET;
  }
}

void SuperLIO::stateWaitMapInit()
{
  if (map_init()) {
    kf_->init_ = true;
    state_fn_ = &SuperLIO::stateProcess;
    LOG(INFO) << GREEN << " ---> [SuperLIO]: Map init done" << RESET;
  }
}

void SuperLIO::process(){
  if(!data_wrapper_->sync_measure(measures_)){
    return;
  }
  const bool normal_frame=state_fn_==&SuperLIO::stateProcess;
  (this->*state_fn_)();
  if(time_audit_&&measures_.lidar.time_audit.enabled){
    const auto& audit=measures_.lidar.time_audit;
    const double start=measures_.lidar.start_time;
    const double end=measures_.lidar.end_time;
    const bool has_history=normal_frame&&!propagate_states_.empty();
    const double first=has_history?propagate_states_.front().time:
      std::numeric_limits<double>::quiet_NaN();
    const double back=has_history?propagate_states_.back().time:
      std::numeric_limits<double>::quiet_NaN();
    std::size_t after_end=0,after_back=0,fallback=0;
    for(double offset:audit.valid_raw_offsets){
      const double acquisition=start+offset;
      after_end+=acquisition>end;
      if(has_history){
        after_back+=acquisition>back;
        fallback+=!auditHasAcquisitionBracket(propagate_states_,acquisition);
      }
    }
    time_audit_<<time_audit_frame_++<<','<<(normal_frame?"normal":"initialization")<<','
      <<start<<','<<audit.max_offset_all_raw<<','<<audit.max_offset_valid_raw<<','
      <<audit.max_offset_geometry<<','<<end<<','<<start+audit.max_offset_valid_raw<<','
      <<audit.imu_last_available<<','<<audit.imu_last_consumed<<','<<first<<','<<back<<','
      <<audit.valid_raw_points<<','<<after_end<<',';
    if(has_history)time_audit_<<after_back<<','<<fallback;
    else time_audit_<<',';
    time_audit_<<','<<audit.geometry_points<<','<<audit.imu_bracket_span_s<<','
      <<measures_.lidar.imu_support_issue<<std::endl;
  }
}


bool SuperLIO::kf_init(){
  static int imu_cout = 0;
  static V3 mean_gyro = V3::Zero();
  static V3 mean_acce = V3::Zero();

  for(auto& imu: measures_.imu){
    imu_cout ++;
    mean_gyro += (imu.gyr - mean_gyro) / imu_cout;
    mean_acce += (imu.acc - mean_acce) / imu_cout;
  }

  /// 100 Hz for 1 second.
  if(imu_cout < 50){
    return false;
  }

  V3 gravity = - mean_acce * g_gravity_norm / mean_acce.norm();
  V3 ref_gravity(0, 0, - g_gravity_norm);
  M3 init_rot = Quat::FromTwoVectors(gravity, ref_gravity).toRotationMatrix();
  V3 n = init_rot.col(0);
  double yaw = atan2(n(1), n(0));

  M3 R_yaw_inv = Eigen::AngleAxis<scalar>(-yaw, V3::UnitZ()).toRotationMatrix(); 

  // init_rot represents the IMU orientation after gravity alignment (level orientation).
  // Perform LiDAR leveling correction, then transform the orientation into the robot frame.
  M3 rot = g_lidar_robo_yaw * R_yaw_inv * init_rot;  

  ESKF::Options options;
  options.gyro_var_ = g_imu_ng;
  options.acce_var_ = g_imu_na;
  options.bias_gyro_var_ = g_imu_nbg;
  options.bias_acce_var_ = g_imu_nba;
  options.num_iterations_ = g_kf_max_iterations;
  options.quit_eps_ = g_kf_quit_eps;

  float imu_scale = g_gravity_norm / mean_acce.norm();
  kf_->SetInitialConditions(options, mean_gyro, V3::Zero(), imu_scale, ref_gravity);
  auto state = kf_->GetSysState();
  state.R = SO3(rot);
  state.p = g_odom_robo.t_;        // By default, the robot frame is used as the reference origin.
  state.timestamp = measures_.imu.back().secs;
  kf_->SetX(state);
  sys_init_pose_ = kf_->GetSE3();
  return true;
}


bool SuperLIO::map_init(){
  frame_num_++;

  std::size_t ptsize = measures_.lidar.pc->size();
  points_world_v3_.resize(ptsize);

  const SE3 transform = sys_init_pose_ * g_lidar_imu;

  tbb::parallel_for(
    tbb::blocked_range<size_t>(0, ptsize),
    [&](const tbb::blocked_range<size_t>& r) {
      for (size_t idx = r.begin(); idx < r.end(); ++idx) {
        auto& point_pcl = measures_.lidar.pc->points[idx];
        V3 point_body(point_pcl.x, point_pcl.y, point_pcl.z);
        points_world_v3_[idx] = transform * point_body;
      }
    }
  );

  ivox_->insert(points_world_v3_);
  kf_->SetLastObsTime(measures_.lidar.end_time);

  if(frame_num_ > 3){
    g_flg_map_init = false;
    return true;
  }
  return false;
}


void SuperLIO::stateProcess(){
  frame_num_++;
  if(g_time_eva){
    time_record_.Evaluate([this](){Propagation_Undistort();}, "Undistort");
    time_record_.Evaluate([this]() { DownSample(); }, "DownSample");
    time_record_.Evaluate([this]() { Observe(); }, "Observe");
    time_record_.Evaluate([this]() { UpdateMap(); }, "UpdateMap");
  }else{
    Propagation_Undistort();
    DownSample();
    Observe();
    UpdateMap();
  }
  Output();
  caceData();
}


void SuperLIO::caceData(){
  if(!g_save_map) return;
  auto state = kf_->GetNavState();
  Eigen::Matrix4f transformation = Eigen::Matrix4f::Identity();
  transformation.block<3, 3>(0, 0) = state.R.R_.cast<float>();
  transformation.block<3, 1>(0, 3) = state.p.cast<float>();

  if(g_if_filter){
    pcl::transformPointCloud(*ds_undistort_, *world_pc_, transformation);
  }else{
    pcl::transformPointCloud(*scan_undistort_full_, *world_pc_, transformation);
  }

  static int scan_wait_num = 0;
  if(!world_pc_->empty()){
    *point_map_ += *world_pc_;
    scan_wait_num++;
  }

  if(g_pcd_save_interval < 0) {
    scan_wait_num = 0;
    return;
  }

  static bool rm_PCD_dir = false;
  if(!rm_PCD_dir){
    rm_PCD_dir = true;
    std::string cmd = "rm -rf " + g_save_map_dir + "/PCD";
    [[maybe_unused]] int res;
    res = system(cmd.c_str());
    cmd = "mkdir -p " + g_save_map_dir + "/PCD";
    res = system(cmd.c_str());
  }

  if (point_map_->size() > 0 && scan_wait_num >= g_pcd_save_interval) {
    pcd_index_++;
    std::string map_name(std::string(g_save_map_dir + "/PCD/scans_") + std::to_string(pcd_index_) +
                               std::string(".pcd"));
    LOG(INFO) << GREEN << " ---> current scan saved to /PCD/scans_" << pcd_index_ << "  size:  " << point_map_->size() << RESET;
    pcl::io::savePCDFileBinary(map_name, *point_map_);
    point_map_->clear();
    scan_wait_num = 0;
  }
}


void SuperLIO::ProcessCaceMap(){
  namespace fs = std::filesystem;

  std::string pcd_folder = g_save_map_dir + "/PCD";
  std::string output_map_name = g_save_map_dir + "/" + g_map_name;

  LOG(INFO) << YELLOW << " ---> Merging PCD fragments in: " << pcd_folder << RESET;

  PointCloudType::Ptr merged_map(new PointCloudType());

  int count = 0;
  for (const auto& entry : fs::directory_iterator(pcd_folder)) {
    if (entry.path().extension() == ".pcd" &&
      entry.path().filename().string().find("scans_") != std::string::npos) {
      PointCloudType::Ptr tmp_cloud(new PointCloudType());
      if (pcl::io::loadPCDFile<PointType>(entry.path().string(), *tmp_cloud) == 0) {
        *merged_map += *tmp_cloud;
        count++;
        // LOG(INFO) << GREEN << " ---> Merged: " << entry.path().filename().string() 
        //           << "   size: " << tmp_cloud->size() << RESET;
      } else {
        LOG(WARNING) << RED << " ---> Failed to load: " << entry.path().string() << RESET;
      }
    }
  }

  LOG(INFO) << YELLOW << " ---> Total merged fragments: " << count << RESET;

  PointCloudType filtered_map;

  if(g_if_filter){
    LOG(INFO) << YELLOW << " ---> Downsampling merged map before final save..." << RESET;
    pcl::VoxelGrid<PointType> voxel_filter;
    voxel_filter.setLeafSize(g_map_ds_size, g_map_ds_size, g_map_ds_size);
    
    voxel_filter.setInputCloud(merged_map);
    voxel_filter.filter(filtered_map);
  }else{
    LOG(INFO) << YELLOW << " ---> Not Downsampling merged map before final save..." << RESET;
    filtered_map = *merged_map;
  }
  
  if (filtered_map.size() > 0) {
    filtered_map.width = filtered_map.size();
    filtered_map.height = 1;
    filtered_map.is_dense = false;
  }

  pcl::io::savePCDFileBinary(output_map_name, filtered_map);

  LOG(INFO) << GREEN << " ---> Final map saved to: " << output_map_name << RESET;
  LOG(INFO) << GREEN << " ---> Final map size: " << filtered_map.size() << RESET;
}


void SuperLIO::saveMap(){
  if(!g_save_map) return;
  if(g_pcd_save_interval > 0){
    LOG(INFO) << YELLOW << " ---> Saving last cace ... " << RESET;
    if (point_map_->size() > 0) {
      pcd_index_++;
      std::string map_name(std::string(g_save_map_dir + "/PCD/scans_") + std::to_string(pcd_index_) +
                                 std::string(".pcd"));
      LOG(INFO) << GREEN << " ---> current scan saved to /PCD/scans_" << pcd_index_ << "  size:  " << point_map_->size() << RESET;
      pcl::io::savePCDFileBinary(map_name, *point_map_);
      point_map_->clear();
    }
    LOG(INFO) << GREEN << " ---> Save last cace success. " << RESET;
    LOG(INFO) << YELLOW << " ---> Process cace map ... " << RESET;
    ProcessCaceMap();
    return;
  }

  LOG(INFO) << YELLOW << " ---> Saving map..... " << RESET;
  if(!point_map_->empty()){
    std::string map_name = g_save_map_dir + "/" + g_map_name;
    LOG(INFO) << YELLOW << " ---> Save map to: " << map_name << RESET;
    pcl::VoxelGrid<PointType> voxel_fliter;
    PointCloudType latst_map;
    voxel_fliter.setInputCloud(point_map_);
    voxel_fliter.setLeafSize(g_map_ds_size, g_map_ds_size, g_map_ds_size);
    voxel_fliter.filter(latst_map);
    if(latst_map.size() > 0){
      latst_map.width = latst_map.size();
      latst_map.height = 1;
      latst_map.is_dense = false;
    }
    pcl::io::savePCDFileBinary(map_name, latst_map);
    LOG(INFO) << GREEN << " ---> Save map success. File: " << map_name << RESET;
    LOG(INFO) << GREEN << " ---> Map size: " << latst_map.size() << RESET;
  }
}


void SuperLIO::Propagation_Undistort(){
  const auto corrected_start=kf_->GetDynamicState();
  if(g_lidar_type==LID_TYPE::OUSTER)
    retainRebasedOusterHistory(propagate_states_,corrected_start,measures_.lidar.start_time);
  else{
    propagate_states_.clear();
    propagate_states_.emplace_back(corrected_start);
  }
  kf_->SetObsTime(measures_.lidar.end_time);
  for (auto &imu : measures_.imu) {
    kf_->Predict(imu);
    const auto state=kf_->GetDynamicState();
    if(g_lidar_type!=LID_TYPE::OUSTER)propagate_states_.emplace_back(state);
    else if(state.time>propagate_states_.back().time+1e-9)
      propagate_states_.emplace_back(state);
    else if(std::abs(state.time-propagate_states_.back().time)<=1e-9)
      propagate_states_.back()=state;
  }

  static const M3 TLI_R = g_lidar_imu.R_;
  static const V3 TLI_t = g_lidar_imu.t_;
  const SE3 T_end = kf_->GetSE3();
  const M3  R_inv = T_end.R_.transpose();
  const V3  T_end_t = T_end.t_;
  const double start_time = measures_.lidar.start_time;
  auto& raw_pc = measures_.lidar.pc;

  std::size_t ptsize = raw_pc->points.size();
  scan_undistort_full_->resize(ptsize); 

  tbb::parallel_for(
  tbb::blocked_range<size_t>(0, ptsize),
  [&](const tbb::blocked_range<size_t>& r) {
    for (size_t idx = r.begin(); idx < r.end(); ++idx) {  
      auto& pt_full = scan_undistort_full_->points[idx];
      const auto& pt = raw_pc->points[idx];
      pt_full.intensity = pt.intensity;
      double query_time = start_time + pt.offset_time;
      if(g_lidar_type!=LID_TYPE::OUSTER){
        // Preserve the frozen non-Ouster deskew path byte for byte.
        if(query_time>propagate_states_.back().time){
          V3 raw(pt.x,pt.y,pt.z);
          V3 eigen_point=TLI_R*raw+TLI_t;
          pt_full.x=eigen_point[0];pt_full.y=eigen_point[1];pt_full.z=eigen_point[2];
          continue;
        }
        auto match_iter=propagate_states_.begin();
        for(auto iter=propagate_states_.begin();iter!=propagate_states_.end();++iter){
          auto next_iter=std::next(iter);
          if(iter->time<query_time&&next_iter->time>=query_time){
            match_iter=iter;
            break;
          }
        }
        auto match_iter_n=std::next(match_iter);
        double dt=match_iter_n->time-match_iter->time;
        double tau=query_time-match_iter->time;
        double s=tau/dt;
        M3 R_h=match_iter->R,R_t=match_iter_n->R;
        V3 p_h=match_iter->p,v_h=match_iter->v,acc_t=match_iter_n->a;
        M3 R_i=Quat(R_h).slerp(s,Quat(R_t)).toRotationMatrix();
        V3 p_i=p_h+v_h*tau+0.5*acc_t*tau*tau;
        V3 t_ei=p_i-T_end_t;
        V3 raw(pt.x,pt.y,pt.z);
        V3 eigen_point=R_inv*(R_i*(TLI_R*raw+TLI_t)+t_ei);
        pt_full.x=eigen_point[0];pt_full.y=eigen_point[1];pt_full.z=eigen_point[2];
        continue;
      }
      auto use_raw_point=[&](){
        V3 raw(pt.x, pt.y, pt.z);
        V3 eigen_point = TLI_R * raw + TLI_t;
        pt_full.x = eigen_point[0];
        pt_full.y = eigen_point[1];
        pt_full.z = eigen_point[2];
      };
      if(propagate_states_.size()<2||
         query_time<propagate_states_.front().time-1e-6||
         query_time>propagate_states_.back().time+1e-6){
        use_raw_point();
        continue;
      }
      M3 R_i;
      V3 p_i;
      if(query_time>=propagate_states_.back().time){
        R_i=propagate_states_.back().R;
        p_i=propagate_states_.back().p;
      }else{
        auto tail=std::upper_bound(propagate_states_.begin(),propagate_states_.end(),query_time,
            [](double t,const DynamicState& s){return t<s.time;});
        if(tail==propagate_states_.begin())++tail;
        auto head=std::prev(tail);
        const double dt=tail->time-head->time;
        if(!(dt>0.)){
          use_raw_point();
          continue;
        }
        const double tau=query_time-head->time;
        const double s=tau/dt;
        R_i=Quat(head->R).slerp(s,Quat(tail->R)).toRotationMatrix();
        p_i=head->p+head->v*tau+0.5*tail->a*tau*tau;
      }
      V3 t_ei = p_i - T_end_t;
      V3 raw(pt.x, pt.y, pt.z);
      V3 eigen_point = R_inv * (R_i * (TLI_R * raw + TLI_t) + t_ei);
      pt_full.x = eigen_point[0];
      pt_full.y = eigen_point[1];
      pt_full.z = eigen_point[2];
    }
  });
}


void SuperLIO::DownSample(){
  voxel_grid_fliter_.setInputCloud(scan_undistort_full_);
  voxel_grid_fliter_.filter(ds_undistort_);
}


struct ThreadACC{
  M6d HTVH = M6d::Zero();
  V6d HTVr = V6d::Zero();
  ThreadACC(): HTVH(M6d::Zero()), HTVr(V6d::Zero()) {}
};


void SuperLIO::Observe(){
  if(photo_->enabled()) photo_->prepare(measures_, propagate_states_, kf_->GetSE3());
  if(coin_->enabled()) coin_->prepare(measures_.lidar,propagate_states_,kf_->GetSE3());
  const bool capture_geometry_rows=coin_->enabled()||photo_->needsGeometryRows()||geometry_rows_audit_.is_open();
  size_t ptsize = ds_undistort_->size();
  
  static std::vector<float> _lengths;
  points_body_v3_.resize(ptsize);
  _lengths.resize(ptsize);

  effect_knn_num_ = ptsize;
  std::iota(effect_knn_idxs_.begin(), effect_knn_idxs_.begin() + ptsize, 0);

  for(size_t i = 0; i < ptsize; ++i){
    const auto& point_body_pcl = ds_undistort_->points[i];
    points_body_v3_[i] = V3(point_body_pcl.x, point_body_pcl.y, point_body_pcl.z);
    _lengths[i] = points_body_v3_[i].norm();
  }

  ivox_->reset_max_group();
  int iter_num = 0;
  Eigen::MatrixXd geometry_translation_rows_final(0,3);
  Eigen::VectorXd geometry_translation_residuals_final(0);

  kf_->UpdateObserve([&, this](const ESKF::KFState &kf_state, M6 &HTVH, V6 &HTVr) {
    const SE3 pose = kf_state.pose;
    const bool need_converge = kf_state.need_converge;
    const M3d R_transpose = (pose.R_.transpose()).cast<double>();

    tbb::enumerable_thread_specific<ThreadACC> tls_acc;
    std::vector<Eigen::Vector3d> geometry_translation_rows;
    std::vector<std::uint8_t> geometry_translation_valid;
    std::vector<double> geometry_translation_residuals;
    if(capture_geometry_rows){
      geometry_translation_rows.assign(points_body_v3_.size(),Eigen::Vector3d::Zero());
      geometry_translation_valid.assign(points_body_v3_.size(),0);
      geometry_translation_residuals.assign(points_body_v3_.size(),0.);
    }

    tbb::parallel_for(
      tbb::blocked_range<size_t>(0, effect_knn_num_),
      [&](const tbb::blocked_range<size_t>& r) {
        KNNHeapType top_K;
        auto& local_acc = tls_acc.local();
        for (size_t r_s = r.begin(); r_s < r.end(); ++r_s) {
          int idx = effect_knn_idxs_[r_s];
          V3& point_body = points_body_v3_[idx];
          V3 point_world = pose * point_body;

          if(!need_converge){
            top_K.reset();
            ivox_->getTopK(point_world, top_K);
            if(top_K.count < 4){
              effect_mask_[idx] = false;
              effect_knn_mask_[idx] = false;
              continue;
            }
            effect_knn_mask_[idx] = true;
            effect_mask_[idx] = calc_plane_coeff(top_K.count, top_K.points_, abcd_vec_[idx]);
          }

          if(!effect_mask_[idx]) continue;

          auto& abcd = abcd_vec_[idx];
          scalar error;
          effect_mask_[idx] = compute_error(abcd, point_world, _lengths[idx], error);
          if(!effect_mask_[idx]) continue;
          
          {
            V3d normvec(abcd[0], abcd[1], abcd[2]);
            V3d nb = R_transpose * normvec;
            V3d point_body_d = point_body.cast<double>();
            V6d J;
            J.head<3>() = point_body_d.cross(nb);
            J.tail<3>() = normvec;
            if(capture_geometry_rows){
              geometry_translation_rows[idx]=normvec.cast<double>();
              geometry_translation_valid[idx]=1;
              geometry_translation_residuals[idx]=static_cast<double>(error);
            }
      
            local_acc.HTVH += J * 1000 * J.transpose();
            local_acc.HTVr -= J * 1000 * error;
          }
        }
    });

    M6d sum_HTVH = M6d::Zero();
    V6d sum_HTVr = V6d::Zero();
    for(const auto& local_acc : tls_acc){
      sum_HTVH += local_acc.HTVH;
      sum_HTVr += local_acc.HTVr;
    }
    HTVH = sum_HTVH.cast<scalar>();
    HTVr = sum_HTVr.cast<scalar>();
    if(photo_->enabled()) photo_->add(pose, HTVH, HTVr, kf_->GetCov().topLeftCorner<6,6>());
    if(capture_geometry_rows){
      std::size_t row_count=0;
      for(std::size_t r_s=0;r_s<effect_knn_num_;++r_s){
        const int idx=effect_knn_idxs_[r_s];row_count+=geometry_translation_valid[idx]!=0;
      }
      geometry_translation_rows_final.resize(row_count,3);
      geometry_translation_residuals_final.resize(static_cast<Eigen::Index>(row_count));
      std::size_t row=0;
      for(std::size_t r_s=0;r_s<effect_knn_num_;++r_s){
        const int idx=effect_knn_idxs_[r_s];
        if(!geometry_translation_valid[idx])continue;
        geometry_translation_rows_final.row(row++)=geometry_translation_rows[idx].transpose();
        geometry_translation_residuals_final[static_cast<Eigen::Index>(row-1)]=
            geometry_translation_residuals[idx];
      }
      coin_->add(pose,HTVH,HTVr);
    }

    if(need_converge) return;

    int _effect_knn_num = 0;
    for(size_t i = 0; i < effect_knn_num_; ++i){
      int idx = effect_knn_idxs_[i];
      if(!effect_knn_mask_[idx]) continue;
      effect_knn_idxs_[_effect_knn_num] = idx;
      _effect_knn_num++;
    }

    // LOG(INFO) << "effect_knn_num_: " << effect_knn_num_ << ", _effect_knn_num: " << _effect_knn_num;
    effect_knn_num_ = _effect_knn_num;

    iter_num++;
  });

  if(geometry_rows_audit_){
    const std::uint32_t frame=static_cast<std::uint32_t>(geometry_rows_audit_frame_++);
    const double timestamp=measures_.lidar.end_time;
    const std::uint32_t effective_points=static_cast<std::uint32_t>(ds_undistort_->size());
    const std::uint32_t row_count=static_cast<std::uint32_t>(geometry_translation_rows_final.rows());
    geometry_rows_audit_.write(reinterpret_cast<const char*>(&frame),sizeof(frame));
    geometry_rows_audit_.write(reinterpret_cast<const char*>(&timestamp),sizeof(timestamp));
    geometry_rows_audit_.write(reinterpret_cast<const char*>(&effective_points),sizeof(effective_points));
    geometry_rows_audit_.write(reinterpret_cast<const char*>(&row_count),sizeof(row_count));
    for(std::uint32_t row=0;row<row_count;++row){
      for(int col=0;col<3;++col){
        const double value=geometry_translation_rows_final(row,col);
        geometry_rows_audit_.write(reinterpret_cast<const char*>(&value),sizeof(value));
      }
      const double residual=geometry_translation_residuals_final[row];
      geometry_rows_audit_.write(reinterpret_cast<const char*>(&residual),sizeof(residual));
    }
    geometry_rows_audit_.flush();
  }

  if(photo_->enabled()) photo_->finish(kf_->GetSE3(), kf_->GetNavState().timestamp,
                                       geometry_translation_rows_final);
  if(coin_->enabled()) coin_->finish(kf_->GetSE3(),kf_->GetNavState().timestamp,
                                     geometry_translation_rows_final);
  frame_num_++;
}


void SuperLIO::UpdateMap() {
  const size_t ptsize = ds_undistort_->size();
  if (ptsize == 0) return;
  
  last_pose_ = kf_->GetSE3();
  points_world_v3_.resize(ptsize);
  
  const auto R = last_pose_.R_;
  const auto t = last_pose_.t_;
  
  for (size_t i = 0; i < ptsize; ++i) {
    const auto& pt = points_body_v3_[i];
    points_world_v3_[i] = R * pt + t;
  }
  
  ivox_->insert(points_world_v3_);

}


void SuperLIO::Output(){
  auto state = kf_->GetNavState();
  data_wrapper_->pub_odom(state);  

  Eigen::Matrix4f transformation = Eigen::Matrix4f::Identity();
  transformation.block<3, 3>(0, 0) = state.R.R_.cast<float>();
  transformation.block<3, 1>(0, 3) = state.p.cast<float>();

  CloudPtr world_pc(new PointCloudType());
  
  if(g_visual_map){
    static int count = -1;
    count++;
    if(count % g_pub_step != 0){
      return;
    }
    count = 0;
    if(g_visual_dense){
      pcl::transformPointCloud(*scan_undistort_full_, *world_pc, transformation);
      data_wrapper_->pub_cloud_world(world_pc, state.timestamp);
    }else{
      pcl::transformPointCloud(*ds_undistort_, *world_pc, transformation);
      data_wrapper_->pub_cloud_world(world_pc, state.timestamp);
    }
  }
}

void SuperLIO::printTimeRecord(){
  if(!g_time_eva) return;
  time_record_.PrintAll();
}

} // namespace END.
````

### src/super_lio/src/ros/ROSWrapper.cpp

Git blob `7d61a176f02b72f574e1d3f43c70b18b6ba73735`；SHA-256 `8a45ab77ca977c8d871110bbd34b7c4a7f4fe31fb2b3dcab1c5da9589b83670d`；809 行完整文件。

````cpp

#include "ros/ROSWrapper.h"
#include "super_lio/CloudPose.h"
#include "super_lio/CloudPose2.h"
#include "ros/ouster_time_support.hpp"

#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <cmath>

using namespace BASIC;

namespace LI2Sup{

void LoadParamFromRos(ros::NodeHandle& nh){
  nh.getParam("/lio/map/save_map", g_save_map);
  LOG(INFO) << GREEN << " ---> [Param] map/save_map: " << (g_save_map ? "true" : "false") << RESET;
  nh.getParam("/lio/map/if_filter", g_if_filter);
  nh.getParam("/lio/map/save_map_dir", g_save_map_dir);
  g_save_map_dir = g_root_dir + g_save_map_dir;
  nh.getParam("/lio/map/map_name", g_map_name);
  nh.getParam("/lio/map/ds_size", g_map_ds_size);
  nh.getParam("/lio/map/save_interval", g_pcd_save_interval);

  nh.getParam("/lio/eva/timer", g_time_eva);
  
  // ROS Topic input
  nh.getParam("/lio/ros/lidar_topic",  g_lidar_topic);
  nh.getParam("/lio/ros/imu_topic",    g_imu_topic);

  // sensor cfg
  nh.getParam("/lio/sensor/lidar_type", g_lidar_type);
  double temp_range_dis;
  nh.getParam("/lio/sensor/blind", temp_range_dis);
  g_blind2 = temp_range_dis * temp_range_dis;
  nh.getParam("/lio/sensor/maxrange", temp_range_dis);
  g_maxrange2 = temp_range_dis * temp_range_dis;
  nh.getParam("/lio/sensor/filter_rate", g_filter_rate);
  nh.getParam("/lio/sensor/enable_downsample", g_enable_downsample);
  nh.getParam("/lio/sensor/voxel_fliter_size", g_voxel_fliter_size);

  nh.getParam("/lio/sensor/gravity_norm", g_gravity_norm);
  nh.getParam("/lio/sensor/imu_type", g_imu_type);
  nh.getParam("/lio/sensor/imu_na",   g_imu_na);
  nh.getParam("/lio/sensor/imu_ng",   g_imu_ng);
  nh.getParam("/lio/sensor/imu_nba",  g_imu_nba);
  nh.getParam("/lio/sensor/imu_nbg",  g_imu_nbg);

  // extrinsic
  std::vector<scalar> extrinsic_lidar_imu, extrinsic_odom_robo;
  nh.getParam("/lio/extrinsic/lidar_imu", extrinsic_lidar_imu);      // 3 + 9
  V3 __t = V3(extrinsic_lidar_imu[0], 
              extrinsic_lidar_imu[1], 
              extrinsic_lidar_imu[2]);
  M3 __R = M3(extrinsic_lidar_imu.data() + 3);
  g_lidar_imu = SE3(__R, __t);  // lidar in imu frame·
  nh.getParam("/lio/extrinsic/odom_robo", extrinsic_odom_robo);     // 3 + 3 x,y,z,r,p,y
  __t = V3(extrinsic_odom_robo[0], 
           extrinsic_odom_robo[1], 
           extrinsic_odom_robo[2]);
  auto temp_R = Eigen::AngleAxisd(extrinsic_odom_robo[5]/180 * M_PI, Eigen::Vector3d::UnitZ()) *
                Eigen::AngleAxisd(extrinsic_odom_robo[4]/180 * M_PI, Eigen::Vector3d::UnitY()) *
                Eigen::AngleAxisd(extrinsic_odom_robo[3]/180 * M_PI, Eigen::Vector3d::UnitX());
  g_odom_robo.R_ = temp_R.cast<scalar>();

  /// ATTENTION:
  /// The transpose is intentionally applied here and represents the inverse of R.
  /// Misinterpreting this will lead to incorrect transformations.
  M3 _R = g_odom_robo.R_.transpose();
  g_odom_robo.R_  = _R;
  g_odom_robo = SE3(_R, __t);  // lidar in robot frame

  auto temp_R_yaw = Eigen::AngleAxisd(extrinsic_odom_robo[5]/180 * M_PI, Eigen::Vector3d::UnitZ()).toRotationMatrix();
  g_lidar_robo_yaw = temp_R_yaw.cast<scalar>();

  // hash_map
  int hash_capacity;
  nh.getParam("/lio/hash_map/hash_capacity", hash_capacity);
  g_ivox_capacity = hash_capacity;
  nh.getParam("/lio/hash_map/vox_resolution", g_ivox_resolution);
  
  // kf
  nh.getParam("/lio/kf/kf_type", g_kf_type);
  nh.getParam("/lio/kf/kf_max_iterations", g_kf_max_iterations);
  nh.getParam("/lio/kf/kf_align_gravity", g_kf_align_gravity);
  nh.getParam("/lio/kf/kf_quit_eps", g_kf_quit_eps);

  // submaps
  nh.getParam("/lio/submap/submap_resolution", g_submap_resolution);
  nh.getParam("/lio/submap/submap_capacity", g_submap_capacity);

  // visual
  nh.getParam("/lio/output/robot",  g_2_robot);
  nh.getParam("/lio/output/planner", g_planner_enable);
  nh.getParam("/lio/output/plan_env_world",  g_2_plan_env_world);
  nh.getParam("/lio/output/plan_env_body",  g_2_plan_env_body);
  nh.getParam("/lio/output/ml_map",         g_2_ml_map);
  nh.getParam("/lio/output/map",    g_visual_map);
  nh.getParam("/lio/output/dense",  g_visual_dense);
  nh.getParam("/lio/output/pub_step", g_pub_step);

  g_update_map = false;
  nh.getParam("/lio/relocation/update_map", g_update_map);
  std::vector<float> init_pose;
  nh.getParam("/lio/relocation/init_pose", init_pose);
  if(init_pose.size() == 6){
    g_init_px    = init_pose[0];
    g_init_py    = init_pose[1];
    g_init_pz    = init_pose[2];
    g_init_roll  = init_pose[3];
    g_init_pitch = init_pose[4];
    g_init_yaw   = init_pose[5];
  }else{
    g_init_px = 0.0f;
    g_init_py = 0.0f;
    g_init_pz = 0.0f;
    g_init_roll = 0.0f;
    g_init_pitch = 0.0f;
    g_init_yaw = 0.0f;
  }
  
}

std::tuple<float, float, float> getColorFromVelocity(float velocity, float max_velocity = 2.0f) {
  float ratio = std::clamp(velocity / max_velocity, 0.0f, 1.0f);
  float r, g, b;
  if (ratio < 0.5f) {
    float t = ratio / 0.5f;
    r = 0.0f;
    g = t;
    b = 1.0f;
  } else {
    float t = (ratio - 0.5f) / 0.5f;
    r = t;
    g = 1.0f - t;
    b = 1.0f - t;
  }
  return {r, g, b};
}


void livox2pcl(const livox_ros_driver::CustomMsg::ConstPtr& msg, CloudPtr& point_cloud){
  point_cloud->clear();
  CloudPtr cloud_full(new PointCloudType());
  int plsize = msg->point_num;
  cloud_full->resize(plsize);
  point_cloud->reserve(plsize);
  std::vector<bool> is_valid_pt(plsize, false);
  std::vector<std::size_t> index(plsize - 1);
  std::iota(std::begin(index), std::end(index), 1);

  std::for_each(std::execution::par_unseq, index.begin(), index.end(), [&](const uint &i) {
    if((msg->points[i].tag & 0x30) == 0x10 || (msg->points[i].tag & 0x30) == 0x00)
    {
      // if (i % g_filter_rate == 0) 
      {
        cloud_full->at(i).x = msg->points[i].x;
        cloud_full->at(i).y = msg->points[i].y;
        cloud_full->at(i).z = msg->points[i].z;
        cloud_full->at(i).intensity = msg->points[i].reflectivity;

        if ((abs(cloud_full->at(i).x - cloud_full->at(i - 1).x) > 1e-7) ||
            (abs(cloud_full->at(i).y - cloud_full->at(i - 1).y) > 1e-7) ||
            (abs(cloud_full->at(i).z - cloud_full->at(i - 1).z) > 1e-7))
        {
          double normal_dis = cloud_full->at(i).x * cloud_full->at(i).x + 
                              cloud_full->at(i).y * cloud_full->at(i).y +
                              cloud_full->at(i).z * cloud_full->at(i).z;
          if(normal_dis > g_blind2 and normal_dis < g_maxrange2){
            is_valid_pt[i] = true;
          }
        }
      }
    }
  });

  for (int i = 1; i < plsize; i++) {
    if (is_valid_pt[i]) {
      point_cloud->points.push_back(cloud_full->at(i));
    }
  }
}


std::string lidarTypeToString(int type) {
  if (type <= 0 || type >= static_cast<int>(LID_TYPE_NAMES.size())) return "UNKNOWN";
  return LID_TYPE_NAMES[type];
}

ROSWrapper::ROSWrapper(){
  nh_.param("/photo/enable", photo_enabled_, false);
  nh_.param("/coin/enable", coin_enabled_, false);
  nh_.param("/preprocess/blind", coin_preprocess_blind_, 0.65);
  std::string time_audit_path;
  nh_.getParam("/p2r/time_audit_path",time_audit_path);
  time_audit_enabled_=!time_audit_path.empty();
  std::vector<double> lidar_to_sensor;
  if ((nh_.getParam("/lidar_to_sensor_transform", lidar_to_sensor) ||
       nh_.getParam("/lidar_intrinsics/lidar_to_sensor_transform", lidar_to_sensor)) &&
      lidar_to_sensor.size() == 16) {
    coin_sensor_z_offset_ = lidar_to_sensor[11] * 0.001;
  }
  ros::SubscribeOptions ops;
  ops.transport_hints = ros::TransportHints().tcpNoDelay();
  
  if(g_lidar_type == LID_TYPE::LIVOX){
    ops.init<livox_ros_driver::CustomMsg>(
      g_lidar_topic, 1000, 
      boost::bind(&ROSWrapper::livoxHandler, this, _1));
  }else{
    ops.init<sensor_msgs::PointCloud2>(
      g_lidar_topic, 1000,
      boost::bind(&ROSWrapper::stdMsgHandler, this, _1));
  }

  LOG(INFO) << GREEN << " ---> Using Lidar type: " << lidarTypeToString(g_lidar_type) << RESET;

  nh_.setCallbackQueue(&self_queue_);

  subLidar_ = nh_.subscribe(ops);
  subIMU_   = nh_.subscribe<sensor_msgs::Imu>(g_imu_topic, 10000,    // 100Hz x 10s
               &ROSWrapper::imuHandler, this, ros::TransportHints().tcpNoDelay());

  /// output
  pub_odom_      = nh_.advertise<nav_msgs::Odometry>("/lio/odom", 100);  /// imu frame -> lidar freq
  pub_path_      = nh_.advertise<nav_msgs::Path>("/lio/path", 1);
  pub_path_robot_ = nh_.advertise<sensor_msgs::PointCloud2>("/lio/path_robot", 1);
  
  msg_path_point_.header.frame_id = "world";
  msg2uav_.header.frame_id = "world";
  path_.header.frame_id = "world";
}


void ROSWrapper::livoxHandler(const livox_ros_driver::CustomMsg::ConstPtr& msg){
  if(msg->point_num < 10) return;
  LidarData lidar_data;
  std::size_t ptsize = msg->point_num;
  lidar_data.pc.reset(new pcl::PointCloud<LI2Sup::PointXTZIT>());
  lidar_data.pc->reserve(ptsize / g_filter_rate + 1);

  double offset_time = 0.0;
  for(std::size_t _i = 0; _i < ptsize; _i += g_filter_rate){
    auto& pt = msg->points[_i];
    auto tag = pt.tag & 0x30;
    if (tag == 0x10 || tag == 0x00){
      auto dis = pt.x * pt.x + pt.y * pt.y + pt.z * pt.z;
      if(dis > g_blind2 && dis < g_maxrange2){
        offset_time = pt.offset_time * 1e-9;
        lidar_data.pc->emplace_back(pt.x, pt.y, pt.z, pt.reflectivity, offset_time);
      }
    }
  }
  if(photo_enabled_){
    lidar_data.pc_intensity.reset(new pcl::PointCloud<LI2Sup::PointXTZIT>());
    lidar_data.pc_intensity->reserve(ptsize);
    for(const auto& pt:msg->points){
      const auto tag=pt.tag & 0x30;
      const double d2=pt.x*pt.x+pt.y*pt.y+pt.z*pt.z;
      if((tag==0x10||tag==0x00)&&std::isfinite(d2)&&d2>g_blind2&&d2<g_maxrange2)
        lidar_data.pc_intensity->emplace_back(pt.x,pt.y,pt.z,pt.reflectivity,pt.offset_time*1e-9);
    }
  }
  lidar_data.start_time = msg->header.stamp.toSec();
  lidar_data.end_time   = lidar_data.start_time + offset_time;
  lidar_buffer_.push_back(lidar_data);
}


inline bool validPoint(double x, double y, double z)
{
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
    return false;

  double d2 = x * x + y * y + z * z;
  return (d2 > g_blind2 && d2 < g_maxrange2);
}

void ROSWrapper::stdMsgHandler(const sensor_msgs::PointCloud2::ConstPtr& msg){
  if(msg->data.size() < 10) return;
  
  LidarData lidar_data;
  lidar_data.pc.reset(new pcl::PointCloud<LI2Sup::PointXTZIT>());

  double offset_time = 0.0;
  double dis = 0.0;

  switch (g_lidar_type) {

  case LID_TYPE::HESAI16:
  {
    pcl::PointCloud<hesai_ros::Point> pl_orig;
    pcl::fromROSMsg(*msg, pl_orig);
    lidar_data.pc->reserve(pl_orig.size() / g_filter_rate + 1);
    const double time_begin = pl_orig.points[0].timestamp;
    lidar_data.start_time = time_begin;
    for(std::size_t i = 0; i < pl_orig.size(); i += g_filter_rate)
    {
      auto& pt = pl_orig.points[i];
      if (!validPoint(pt.x, pt.y, pt.z)) continue;
      offset_time = pt.timestamp - time_begin;
      lidar_data.pc->emplace_back(
          pt.x, pt.y, pt.z, pt.intensity, offset_time);
    }
    lidar_data.end_time = time_begin + offset_time;
    break;
  }
  case LID_TYPE::VEL_NCLT:
  {
    pcl::PointCloud<NCLT::Point> pl_orig;
    pcl::fromROSMsg(*msg, pl_orig);
    lidar_data.pc->reserve(pl_orig.size() / g_filter_rate + 1);
    lidar_data.start_time = msg->header.stamp.toSec();
    
    for(std::size_t i = 0; i < pl_orig.size(); i += g_filter_rate){
      auto& pt = pl_orig.points[i];
      if (!validPoint(pt.x, pt.y, pt.z)) continue;
      offset_time = pt.time * 1e-6;
      lidar_data.pc->emplace_back(
          pt.x, pt.y, pt.z, 1.0, offset_time);
    }
    lidar_data.end_time = lidar_data.start_time + offset_time;
    break;
  }
  case LID_TYPE::VELO16:
  case LID_TYPE::VELO32:
  {
    pcl::PointCloud<velodyne_ros::Point> pl_orig;
    pcl::fromROSMsg(*msg, pl_orig);
    lidar_data.pc->reserve(pl_orig.size() / g_filter_rate + 1);
    lidar_data.start_time = msg->header.stamp.toSec();

    for(std::size_t i = 0; i < pl_orig.size(); i += g_filter_rate){
      auto& pt = pl_orig.points[i];
      if (!validPoint(pt.x, pt.y, pt.z)) continue;
      lidar_data.pc->emplace_back(
          pt.x, pt.y, pt.z, pt.intensity, pt.time);
    }
    lidar_data.end_time = lidar_data.start_time + lidar_data.pc->points.back().offset_time;
    break;
  }
  case OUSTER:
  {
    pcl::PointCloud<ouster_ros::Point> pl_orig;
    pcl::fromROSMsg(*msg, pl_orig);
    if(time_audit_enabled_){
      auto& audit=lidar_data.time_audit;
      audit.enabled=true;
      audit.raw_points=pl_orig.size();
      audit.valid_raw_offsets.reserve(pl_orig.size());
      for(const auto& pt:pl_orig.points){
        const double offset=pt.t*1e-9;
        audit.max_offset_all_raw=std::max(audit.max_offset_all_raw,offset);
        if(std::isnan(pt.x)||std::isnan(pt.y)||std::isnan(pt.z))continue;
        const double range=std::sqrt(double(pt.x)*pt.x+double(pt.y)*pt.y+double(pt.z)*pt.z);
        if(range<coin_preprocess_blind_)continue;
        ++audit.valid_raw_points;
        audit.max_offset_valid_raw=std::max(audit.max_offset_valid_raw,offset);
        audit.valid_raw_offsets.push_back(offset);
      }
    }
    // COIN's temporal support is all valid raw Ouster samples, irrespective
    // of Super's geometry filter rate or message point ordering.
    const auto support=ousterScanTimeSupport(pl_orig.points,coin_preprocess_blind_);
    if(!support.has_valid_raw){
      ROS_WARN("Ouster scan has no valid raw temporal-support points; skipping scan");
      return;
    }
    if(coin_enabled_){
      lidar_data.coin_raw_points.reserve(pl_orig.size());
      for(std::size_t raw_index=0;raw_index<pl_orig.size();++raw_index){
        const auto& pt=pl_orig.points[raw_index];
        if(std::isnan(pt.x)||std::isnan(pt.y)||std::isnan(pt.z))continue;
        const double range=std::sqrt(double(pt.x)*pt.x+double(pt.y)*pt.y+double(pt.z)*pt.z);
        if(range<coin_preprocess_blind_)continue;
        CoinRawPoint sample;
        sample.x=pt.x;sample.y=pt.y;sample.z=pt.z-static_cast<float>(coin_sensor_z_offset_);
        sample.intensity=pt.intensity;sample.range=range;sample.offset_time=pt.t*1e-9;
        sample.raw_index=raw_index;
        lidar_data.coin_raw_points.push_back(sample);
      }
    }
    if(photo_enabled_){
      lidar_data.pc_intensity.reset(new pcl::PointCloud<LI2Sup::PointXTZIT>());
      lidar_data.pc_intensity->reserve(pl_orig.size());
      for(const auto& pt:pl_orig.points)
        if(validPoint(pt.x,pt.y,pt.z))
          lidar_data.pc_intensity->emplace_back(pt.x,pt.y,pt.z,pt.intensity,pt.t*1e-9);
    }
    lidar_data.pc->reserve(pl_orig.size() / g_filter_rate + 1);
    lidar_data.start_time = msg->header.stamp.toSec();

    for(std::size_t i = 0; i < pl_orig.size(); i += g_filter_rate){
      auto& pt = pl_orig.points[i];
      if (!validPoint(pt.x, pt.y, pt.z)) continue;
      offset_time = pt.t * 1e-9;
      if(time_audit_enabled_){
        ++lidar_data.time_audit.geometry_points;
        lidar_data.time_audit.max_offset_geometry=std::max(
            lidar_data.time_audit.max_offset_geometry,offset_time);
        lidar_data.time_audit.last_offset_geometry=offset_time;
      }
      lidar_data.pc->emplace_back(
          pt.x, pt.y, pt.z, pt.intensity, offset_time);
    }
    lidar_data.end_time = lidar_data.start_time + support.max_offset_s;
    break;
  }
  default:
    return;
  }
  
  lidar_buffer_.push_back(lidar_data);
}



void ROSWrapper::imuHandler(const sensor_msgs::Imu::ConstPtr& msg){
  IMUData data;
  data.secs = msg->header.stamp.toSec();
  data.acc  = V3(msg->linear_acceleration.x,
                 msg->linear_acceleration.y,
                 msg->linear_acceleration.z);
  data.gyr  = V3(msg->angular_velocity.x,
                 msg->angular_velocity.y,
                 msg->angular_velocity.z);

  if (data.secs < last_timestamp_imu_) {
    LOG(WARNING) << "imu loop back, clear buffer";
    imu_buffer_.clear();
    imu_buffer_.push_back(data);
    last_timestamp_imu_ = data.secs;
    // eskf_->Reset();   // todo:
    return;
  }

  imu_buffer_.push_back(data);
  last_timestamp_imu_ = data.secs;

  static ros::Publisher pub_imu_odom  = nh_.advertise<nav_msgs::Odometry>("/lio/imu/odom", 10);    /// imu frame -> imu freq
  static ros::Publisher pub_robo_odom = nh_.advertise<nav_msgs::Odometry>("/lio/robo/odom", 10);   /// robot frame -> imu freq
  
  DynamicState imu_state, robo_state;
  if(eskf_->Predict(data, imu_state, robo_state)){
    nav_msgs::Odometry odom_imu, odom_robo;

    {
      odom_imu.pose.pose.position.x = imu_state.p(0);
      odom_imu.pose.pose.position.y = imu_state.p(1);
      odom_imu.pose.pose.position.z = imu_state.p(2);

      Quat q(imu_state.R);
      q.normalize();

      odom_imu.pose.pose.orientation.x = q.x();
      odom_imu.pose.pose.orientation.y = q.y();
      odom_imu.pose.pose.orientation.z = q.z();
      odom_imu.pose.pose.orientation.w = q.w();

      odom_imu.twist.twist.linear.x = imu_state.v(0);
      odom_imu.twist.twist.linear.y = imu_state.v(1);
      odom_imu.twist.twist.linear.z = imu_state.v(2);

      odom_imu.twist.twist.angular.x = imu_state.w(0);
      odom_imu.twist.twist.angular.y = imu_state.w(1);
      odom_imu.twist.twist.angular.z = imu_state.w(2);
    }

    {
      odom_robo.pose.pose.position.x = robo_state.p(0);
      odom_robo.pose.pose.position.y = robo_state.p(1);
      odom_robo.pose.pose.position.z = robo_state.p(2);

      Quat q(robo_state.R);
      q.normalize();

      odom_robo.pose.pose.orientation.x = q.x();
      odom_robo.pose.pose.orientation.y = q.y();
      odom_robo.pose.pose.orientation.z = q.z();
      odom_robo.pose.pose.orientation.w = q.w();
    }

    odom_imu.header.stamp = msg->header.stamp;
    odom_robo.header.stamp = msg->header.stamp;
    odom_imu.header.frame_id = "world";
    odom_robo.header.frame_id = "world";
    pub_imu_odom.publish(odom_imu);
    pub_robo_odom.publish(odom_robo);
  }
}


bool ROSWrapper::sync_measure(MeasureGroup& meas){
  if (lidar_buffer_.empty() || imu_buffer_.empty()) {
    return false;
  }else{
  }

  /*** push a lidar scan ***/
  if (!lidar_pushed_) {
    meas.lidar = lidar_buffer_.front();
    lidar_pushed_ = true;
  }

  if(last_timestamp_lidar_ > meas.lidar.end_time){
    lidar_buffer_.pop_front();
    lidar_pushed_ = false;
    return false;
  }

  if (!imuHasReachedScanEnd(last_timestamp_imu_,meas.lidar.end_time)) {
    return false;
  }

  meas.imu.clear();
  if(g_lidar_type==LID_TYPE::OUSTER){
    const auto window=collectOusterImuWindow(imu_buffer_,meas.lidar.end_time,
        have_last_synced_imu_?&last_synced_imu_:nullptr);
    meas.imu=window.samples;
    meas.lidar.imu_support_issue=window.issue;
    meas.lidar.time_audit.imu_bracket_span_s=window.bracket_span_s;
    if(window.has_end_sample){
      last_synced_imu_=window.end_sample;
      have_last_synced_imu_=true;
    }
  }else{
    /*** Historical non-Ouster synchronization remains unchanged. ***/
    double imu_time=imu_buffer_.front().secs;
    while(!imu_buffer_.empty()&&imu_time<meas.lidar.end_time){
      imu_time=imu_buffer_.front().secs;
      if(imu_time>meas.lidar.end_time)break;
      meas.imu.push_back(imu_buffer_.front());
      imu_buffer_.pop_front();
    }
  }

  last_timestamp_lidar_ = meas.lidar.end_time;
  if(meas.lidar.time_audit.enabled){
    meas.lidar.time_audit.imu_last_available=last_timestamp_imu_;
    if(!meas.imu.empty())meas.lidar.time_audit.imu_last_consumed=meas.imu.back().secs;
  }
  lidar_buffer_.pop_front();
  lidar_pushed_ = false;
  return true;
}


void ROSWrapper::pub_odom(const NavState& state){
  nav_msgs::Odometry odom;
  odom.header.frame_id = "world";
  odom.header.stamp = ros::Time().fromSec(state.timestamp);
  odom.pose.pose.position.x = state.p[0];
  odom.pose.pose.position.y = state.p[1];
  odom.pose.pose.position.z = state.p[2];

  V4 temp_q = state.R.coeffs();
  odom.pose.pose.orientation.x = temp_q[0];
  odom.pose.pose.orientation.y = temp_q[1];
  odom.pose.pose.orientation.z = temp_q[2];
  odom.pose.pose.orientation.w = temp_q[3];

  odom.twist.twist.linear.x = state.v[0];
  odom.twist.twist.linear.y = state.v[1];
  odom.twist.twist.linear.z = state.v[2];

  pub_odom_.publish(odom);    // imu frame -> lidar frequency

  V3 robo_position = state.R.R_ * ( - g_odom_robo.R_ * g_odom_robo.t_) + state.p;

  if(g_2_robot){
    static ros::Publisher pub_msg2uav_ = nh_.advertise<geometry_msgs::PoseStamped>("/mavros/vision_pose/pose", 10);
    M3 robo_rotation = state.R.R_ * g_odom_robo.R_;
    msg2uav_.header.stamp = odom.header.stamp;
    msg2uav_.pose.position.x = robo_position[0];
    msg2uav_.pose.position.y = robo_position[1];
    msg2uav_.pose.position.z = robo_position[2];
    Quat robo_quat(robo_rotation);
    msg2uav_.pose.orientation.w = robo_quat.w();
    msg2uav_.pose.orientation.x = robo_quat.x();
    msg2uav_.pose.orientation.y = robo_quat.y();
    msg2uav_.pose.orientation.z = robo_quat.z();
    pub_msg2uav_.publish(msg2uav_);
  }

  // if(1)
  if((last_path_point_ - robo_position).norm() > 0.1)
  {
    /// nav_msgs::Path
    path_.header.stamp = odom.header.stamp;
    geometry_msgs::PoseStamped point;
    point.pose = odom.pose.pose;
    path_.poses.push_back(point);
    pub_path_.publish(path_);

    /// sensor_msgs::PointCloud2
    // pcl::PointXYZRGB point_robot;
    // point_robot.x = robo_position[0];
    // point_robot.y = robo_position[1];
    // point_robot.z = robo_position[2];
    // auto [r, g, b] = getColorFromVelocity(state.v.norm());
    // point_robot.r = static_cast<uint8_t>(r * 255);
    // point_robot.g = static_cast<uint8_t>(g * 255);
    // point_robot.b = static_cast<uint8_t>(b * 255);
    // point_robot.a = 255;
    // pcl::PointCloud<pcl::PointXYZRGB> path_point_;
    // path_point_.push_back(point_robot);
    // pcl::toROSMsg(path_point_, msg_path_point_);
    // msg_path_point_.header.stamp = odom.header.stamp;
    // msg_path_point_.header.frame_id = "world";
    // pub_path_robot_.publish(msg_path_point_);

    last_path_point_ = robo_position;
  }

  // tf::Transform transform;
  // tf::Quaternion q;
  // transform.setOrigin(tf::Vector3(state.p[0], state.p[1], state.p[2]));
  // q.setX(temp_q[0]);
  // q.setY(temp_q[1]);
  // q.setZ(temp_q[2]);
  // q.setW(temp_q[3]);
  // transform.setRotation(q);
  // br_.sendTransform(tf::StampedTransform(transform, odom.header.stamp, "world", "body"));

  // // Visual: for the best field of view in rviz.
  // q.setX(0);
  // q.setY(0);
  // q.setZ(0);
  // q.setW(1);
  // transform.setRotation(q);
  // br_.sendTransform(tf::StampedTransform(transform, odom.header.stamp, "world", "god"));
}



void ROSWrapper::pub_cloud_world(const CloudPtr& pc,double time){
  static ros::Publisher pub_cloud_world_ = nh_.advertise<sensor_msgs::PointCloud2>
                                            ("/lio/cloud_world", 10);
  sensor_msgs::PointCloud2 cloud;
  pcl::toROSMsg(*pc, cloud);
  cloud.header.frame_id = "world";
  cloud.header.stamp = ros::Time().fromSec(time);
  pub_cloud_world_.publish(cloud);
}


void ROSWrapper::pub_cloud2planner(const CloudPtr& pc, double time){
  static ros::Publisher pub_cloud2robot_ = nh_.advertise<sensor_msgs::PointCloud2>
                                            ("/lio/robo/cloud_world", 10);
  sensor_msgs::PointCloud2 cloud;
  pcl::toROSMsg(*pc, cloud);
  cloud.header.frame_id = "world";
  cloud.header.stamp = ros::Time().fromSec(time);
  pub_cloud2robot_.publish(cloud);
}


void ROSWrapper::pub_cloud_body_pose(const CloudPtr& pc, 
  const NavState& state)
{
  static ros::Publisher pub_output2robot_ = nh_.advertise<super_lio::CloudPose>
                                            ("/lio/body/cloud_pose", 10);
  super_lio::CloudPose cloud_pose;
  pcl::toROSMsg(*pc, cloud_pose.cloud);
  cloud_pose.cloud.header.stamp = ros::Time().fromSec(state.timestamp);  
  cloud_pose.pose.position.x = state.p[0];
  cloud_pose.pose.position.y = state.p[1];
  cloud_pose.pose.position.z = state.p[2];
  V4 temp_q = state.R.coeffs();
  cloud_pose.pose.orientation.x = temp_q[0];
  cloud_pose.pose.orientation.y = temp_q[1];
  cloud_pose.pose.orientation.z = temp_q[2];
  cloud_pose.pose.orientation.w = temp_q[3];

  pub_output2robot_.publish(cloud_pose);
}


void ROSWrapper::pub_cloud_world_pose(const CloudPtr& pc, 
   const NavState& state)
{
  static ros::Publisher pub_output2robot_ = nh_.advertise<super_lio::CloudPose>
                                            ("/lio/world/cloud_pose", 10);
  super_lio::CloudPose cloud_pose;
  pcl::toROSMsg(*pc, cloud_pose.cloud);
  cloud_pose.cloud.header.stamp = ros::Time().fromSec(state.timestamp);  
  cloud_pose.pose.position.x = state.p[0];
  cloud_pose.pose.position.y = state.p[1];
  cloud_pose.pose.position.z = state.p[2];
  V4 temp_q = state.R.coeffs();
  cloud_pose.pose.orientation.x = temp_q[0];
  cloud_pose.pose.orientation.y = temp_q[1];
  cloud_pose.pose.orientation.z = temp_q[2];
  cloud_pose.pose.orientation.w = temp_q[3];
  
  pub_output2robot_.publish(cloud_pose);
}


void ROSWrapper::pub_cloud_body_pose( 
      const BASIC::VV3& pc_body,
      const NavState& state)
{
  static ros::Publisher pub_msg_ = nh_.advertise<super_lio::CloudPose2>
                                            ("/lio/dense/cloud_pose", 10);

  super_lio::CloudPose2 cloud_pose;
  cloud_pose.header.stamp = ros::Time().fromSec(state.timestamp);

  cloud_pose.pose.reserve(12);
  cloud_pose.pose.push_back(state.p[0]);
  cloud_pose.pose.push_back(state.p[1]);
  cloud_pose.pose.push_back(state.p[2]);

  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c)
      cloud_pose.pose.push_back(state.R.R_(r, c));

  cloud_pose.cloud_lidar.reserve(pc_body.size() * 3);
  for (const auto& pt : pc_body) {
    cloud_pose.cloud_lidar.push_back(pt[0]);
    cloud_pose.cloud_lidar.push_back(pt[1]);
    cloud_pose.cloud_lidar.push_back(pt[2]);
  }

  pub_msg_.publish(cloud_pose);
}


void ROSWrapper::pub_processing_time(double time, double current_time, double mean_time, double std_time)
{
  static ros::Publisher pub_process_time_ = nh_.advertise<geometry_msgs::PoseStamped>
                                            ("/lio/processing_time", 10);
  geometry_msgs::PoseStamped msg;
  msg.header.stamp = ros::Time().fromSec(time);
  msg.pose.position.x = current_time;
  msg.pose.position.y = mean_time;
  msg.pose.position.z = std_time;
  pub_process_time_.publish(msg);
}


void ROSWrapper::set_global_map(const BASIC::CloudPtr& global_map){
  pcl::toROSMsg(*global_map, global_map_msg_);
  global_map_msg_.header.frame_id = "world";

  static ros::Publisher global_map_pub =
    nh_.advertise<sensor_msgs::PointCloud2>("/lio/global_map", 1, true);

  static ros::Timer global_map_timer =
    nh_.createTimer(
      ros::Duration(1.0),
      [this](const ros::TimerEvent&) {
        static int count = -1;
        static int publish_interval = 1;
        count++;
        if (count % publish_interval != 0) {
          return;
        }
        count = 0;
        publish_interval++;
        if(publish_interval > 10) publish_interval = 10;
        global_map_msg_.header.stamp = ros::Time::now();
        global_map_pub.publish(global_map_msg_);
      });
}

void ROSWrapper::set_initial_data(BASIC::SE3& init_pose, bool& flg_get_init_guess, bool flg_finish_init)
{
  static ros::Subscriber init_pose_sub =
    nh_.subscribe<geometry_msgs::PoseWithCovarianceStamped>(
      "/initialpose", 1,
      [this, &init_pose, &flg_get_init_guess]
      (const geometry_msgs::PoseWithCovarianceStampedConstPtr& msg)
      {
        V3 init_translation;
        init_translation << 
            msg->pose.pose.position.x,
            msg->pose.pose.position.y,
            0.2;

        double x = msg->pose.pose.orientation.x;
        double y = msg->pose.pose.orientation.y;
        double z = msg->pose.pose.orientation.z;
        double w = msg->pose.pose.orientation.w;

        Quat init_rotation(w, x, y, z);

        init_pose = SE3(SO3(init_rotation.toRotationMatrix()), init_translation);

        flg_get_init_guess = true;
        
        LOG(INFO) << YELLOW
                  << " ---> GET Initial guess: "
                  << init_translation.transpose()
                  << " yaw: "
                  << init_rotation.toRotationMatrix()
                          .eulerAngles(0, 1, 2)
                          .transpose()
                  << RESET;
      });

  if (flg_finish_init) {
    init_pose_sub = ros::Subscriber();
  }
}



} // namespace END.
````

### src/super_lio/test/test_coin_feature_math.cpp

Git blob `2b350e91c25e4a14bf959176b2acddd57087a7d9`；SHA-256 `d723d5d94d05c8e74f4c2a087e3ec96003c4081d7685da96af9b52455dceeb18`；162 行完整文件。

````cpp
#include "intensity/coin/coin_feature_manager.hpp"
#include "intensity/coin/coin_intensity_representation.hpp"
#include "intensity/coin/coin_cubemap_representation.hpp"
#include "intensity/coin/super_degeneracy_gate.hpp"
#include "intensity/intensity_representation.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
}

int main(){
  using namespace cube::coin;
  try{
    using PureGradientSelectorSignature=std::vector<cv::Point>(*) (
        const std::vector<std::pair<double,cv::Point>>&,int);
    static_assert(std::is_same<decltype(&CoinFeatureManager::selectPureGradient),
                               PureGradientSelectorSignature>::value,
                  "pure-gradient feature selector must not accept GT or trajectory inputs");
    const std::vector<double> reference{1,2,4,8,16};
    std::vector<double> brightened;for(double x:reference)brightened.push_back(3*x+11);
    require(std::abs(CoinFeatureManager::normalizedCrossCorrelation(reference,brightened)-1.)<1e-12,
            "NCC must be invariant to positive affine brightness changes");
    const std::vector<double> monotonic{1,2,3,4,5};
    std::vector<double> reversed=monotonic;std::reverse(reversed.begin(),reversed.end());
    require(CoinFeatureManager::normalizedCrossCorrelation(monotonic,reversed)<-.999999,
            "NCC must reject a reversed contrast pattern");
    require(std::isnan(CoinFeatureManager::normalizedCrossCorrelation({2,2,2},{5,5,5})),
            "constant patches must remain undefined under official NCC semantics");

    Eigen::MatrixXd H=Eigen::MatrixXd::Zero(15,3);int row=0;
    for(int i=0;i<6;++i)H(row++,0)=1;
    for(int i=0;i<5;++i)H(row++,1)=1;
    for(int i=0;i<4;++i)H(row++,2)=1;
    const Eigen::Matrix3d R=Eigen::AngleAxisd(.63,Eigen::Vector3d::UnitY()).toRotationMatrix();
    const auto weak=CoinFeatureManager::weakDirectionsFromGeometry(H,R,5.);
    require((weak.contribution-Eigen::Vector3d(4,5,6)).norm()<1e-12,
            "COIN contribution counts must follow ascending-eigenvalue vector order");
    require(weak.global.size()==1&&std::abs(weak.global[0].dot(Eigen::Vector3d::UnitZ()))>1.-1e-12,
            "only the weak z translation direction should be retained");
    require(weak.lidar.size()==1&&std::abs(weak.lidar[0].dot(R.transpose()*Eigen::Vector3d::UnitZ()))>1.-1e-12,
            "weak global direction must be rotated into LiDAR coordinates");
    require((weak.eigenvalues-Eigen::Vector3d(4.,5.,6.)).norm()<1e-12&&weak.geometry_rows==15,
            "geometry audit must retain ascending Super translation eigenvalues and row count");
    const auto gate=SuperDegeneracyGate::measure(weak,Eigen::Vector3d::UnitZ(),true);
    const auto gate_sign_flipped=SuperDegeneracyGate::measure(weak,-Eigen::Vector3d::UnitZ(),true);
    require(gate.valid&&gate.weakest_axis_stability==1.&&gate.confidence>0.,
            "a distinct, anisotropic and temporally aligned weakest axis must have confidence");
    require(std::abs(gate.confidence-gate_sign_flipped.confidence)<1e-15,
            "weakest-axis temporal confidence must be invariant to eigenvector sign");
    require(SuperDegeneracyGate::activate(gate,gate.confidence)&&
            !SuperDegeneracyGate::activate(gate,gate.confidence+1e-9),
            "geometry-only gate activation must be deterministic at its frozen threshold");
    const auto first_gate=SuperDegeneracyGate::measure(weak,Eigen::Vector3d::UnitX(),false);
    require(first_gate.weakest_axis_stability==0.&&first_gate.confidence==0.,
            "the first frame without temporal support must not claim confident stability");
    static_assert(std::is_abstract<cube::IntensityRepresentation>::value,
                  "future intensity projection contract must remain an interface");
    static_assert(std::is_base_of<cube::IntensityRepresentation,CoinIntensityRepresentation>::value,
                  "COIN must have a documented opt-in representation adapter");
    static_assert(!std::is_abstract<CoinIntensityRepresentation>::value,
                  "COIN adapter must implement projection, samples, residual and validity checks");

    // Exercise the production representation seam, including filled-pixel
    // landmark ownership and the identity transform that prevents double deskew.
    OusterMetadata metadata;metadata.rows=128;metadata.cols=1024;
    metadata.pixel_shift_by_row.assign(128,0);
    for(int i=0;i<128;++i)metadata.beam_altitude_degrees.push_back(22.5-45.*i/127.);
    CoinOusterProjector native_projector(metadata),packed_projector(metadata,96);
    const Vec3 test_point(20.,2.,1.);
    const auto projected=packed_projector.project(test_point);
    const auto jacobian=packed_projector.projectionJacobian(test_point);
    for(int k=0;k<3;++k){
      Vec3 plus=test_point,minus=test_point;plus[k]+=1e-5;minus[k]-=1e-5;
      const Vec2 numerical=(packed_projector.project(plus).uv-packed_projector.project(minus).uv)/2e-5;
      require((numerical-jacobian.col(k)).norm()<1e-7,"packed cube Jacobian must differentiate pixel coordinates");
    }
    require(projected.in_fov&&!packed_projector.project(Vec3(1.,1.,0.)).in_fov,
            "cube seam must not be sampled across charts");
    CoinFrame calibrated;calibrated.intensity=cv::Mat(128,1024,CV_32F,cv::Scalar(100.));
    std::vector<CoinScanPoint> cube_points;
    for(int v=30;v<=65;++v)for(int u=30;u<=65;++u){
      if(u==47&&v==47)continue;
      CoinScanPoint point;point.point_lidar=Vec3(20.,20.*(2.*u/95.-1.),20.*(2.*v/95.-1.));
      point.raw_index=cube_points.size();point.range=point.point_lidar.norm();cube_points.push_back(point);
    }
    cube::Settings cube_cfg;cube_cfg.build_igm=false;
    cube::CubeImage cube_image(cube_cfg,cube::MeasurementChannel::RawIntensity);
    CoinImageSettings image_settings;image_settings.masks.clear();
    const auto packed=buildCoinCubemap(cube_image,calibrated,native_projector,image_settings,cube_points);
    const int filled_index=packed.image_index.at<int>(47,47);
    require(filled_index>=0&&static_cast<std::size_t>(filled_index)<cube_points.size(),
            "IDW patch pixel must own a private depth-derived intensity landmark");
    const auto reprojection=packed_projector.project(cube_points[filled_index].point_lidar);
    require((reprojection.uv-Vec2(47.,47.)).norm()<1e-10,"filled landmark must reproject to its reference pixel");
    require(packed.mask.at<uchar>(47,47)&&!packed.mask.at<uchar>(47,95),
            "patch mask must preserve a supported interior and exclude chart boundaries");
    require(packed.T_Li_Lk_vec.size()==1&&packed.T_Li_Lk_vec[0].isIdentity()&&
            packed.vec_idx.size()==cube_points.size(),"end-frame representation must use identity acquisition transform");
    CoinFeatureManager packed_manager(packed_projector);
    Vec3 acquisition;Vec2 uv;int index=-1;
    require(packed_manager.projectUndistorted(packed,cube_points,cube_points[filled_index].point_lidar,
                                             acquisition,uv,index,true)&&
            (acquisition-cube_points[filled_index].point_lidar).norm()==0.,
            "cubemap matching must never deskew the end-frame point twice");
    const Eigen::Matrix4d identity=Eigen::Matrix4d::Identity();
    const auto photo_row=CoinPhotometricModel::linearize(packed_manager,packed_projector,packed,cube_points,
        identity,identity,cube_points[filled_index].point_lidar,100.,.7,30.,10);
    require(photo_row.valid&&photo_row.residual==0.&&photo_row.correction_jacobian_super.allFinite(),
            "cubemap patch must enter the shared COIN residual and Jacobian path");
    std::cout<<"{\"packed_cube_jacobian\":\"PASS\",\"idw_landmark_ownership\":\"PASS\","
               "\"chart_boundary_mask\":\"PASS\",\"no_double_deskew\":\"PASS\",\"shared_coin_residual\":\"PASS\"}\n";

    Eigen::MatrixXd duplicated(2*H.rows(),3);
    duplicated.topRows(H.rows())=H;duplicated.bottomRows(H.rows())=H;
    const auto duplicated_weak=CoinFeatureManager::weakDirectionsFromGeometry(duplicated,R,5.);
    require((duplicated_weak.contribution-2.*weak.contribution).norm()<1e-12,
            "official absolute contributions must double when every geometry row is duplicated");
    const Eigen::Vector3d normalized=weak.contribution/static_cast<double>(H.rows());
    const Eigen::Vector3d normalized_duplicate=duplicated_weak.contribution/
        static_cast<double>(duplicated.rows());
    require((normalized-normalized_duplicate).norm()<1e-12,
            "row-count-normalized contributions must be invariant to duplicate rows");
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eig(H.transpose()*H);
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eig_duplicate(duplicated.transpose()*duplicated);
    require((eig.eigenvalues()/eig.eigenvalues().maxCoeff()-
             eig_duplicate.eigenvalues()/eig_duplicate.eigenvalues().maxCoeff()).norm()<1e-12,
            "eigenvalue ratios must be invariant to duplicate rows");
    for(int d=0;d<3;++d){
      const Eigen::Vector3d a=eig.eigenvectors().col(d),b=eig_duplicate.eigenvectors().col(d);
      require(std::min((a-b).norm(),(a+b).norm())<1e-12,
              "weakest-eigenvector orientation must be deterministic up to sign");
    }

    const std::vector<std::pair<double,cv::Point>> gradient_candidates{
      {5.,cv::Point(2,1)},{7.,cv::Point(4,4)},{5.,cv::Point(1,2)},{6.,cv::Point(3,0)}};
    const auto gradient_selected=CoinFeatureManager::selectPureGradient(gradient_candidates,3);
    const auto gradient_repeat=CoinFeatureManager::selectPureGradient(gradient_candidates,3);
    require(gradient_selected.size()==3&&gradient_repeat.size()==gradient_selected.size(),
            "pure-gradient selector must honor the feature cap");
    require(gradient_selected[0]==cv::Point(4,4)&&gradient_selected[1]==cv::Point(3,0)&&
            gradient_selected[2]==cv::Point(2,1),
            "pure-gradient selector must rank response and break ties by image row/column");
    for(std::size_t i=0;i<gradient_selected.size();++i)
      require(gradient_repeat[i]==gradient_selected[i],"pure-gradient candidate ordering must repeat exactly");
    std::vector<std::pair<double,cv::Point>> sixty_candidates;
    for(int i=0;i<100;++i)sixty_candidates.emplace_back(100.-i,cv::Point(i%20,i/20));
    require(CoinFeatureManager::selectPureGradient(sixty_candidates,60).size()==60,
            "production selector must respect COIN's 60-center cap");

    Eigen::MatrixXd short_H=Eigen::MatrixXd::Zero(3,3);
    const auto fallback=CoinFeatureManager::weakDirectionsFromGeometry(short_H,R,25.);
    require(fallback.global.empty()&&fallback.lidar.size()==3,
            "COIN must use LiDAR XYZ when geometry has no weak direction set");
    std::cout<<"{\"ncc_affine\":\"PASS\",\"ncc_contrast\":\"PASS\",\"weak_direction_contribution\":\"PASS\",\"row_count_scaling\":\"PASS\",\"weakest_eigenvector_sign\":\"PASS\",\"super_degeneracy_gate\":\"PASS\",\"intensity_representation_interface\":\"PASS\",\"pure_gradient_order_and_60_cap\":\"PASS\",\"no_gt_selector_input\":\"PASS\",\"frame_rotation\":\"PASS\",\"fallback_axes\":\"PASS\"}\n";
    return 0;
  }catch(const std::exception& e){std::cerr<<"COIN feature math test: "<<e.what()<<'\n';return 1;}
}
````

### src/super_lio/test/test_cube_photo.cpp

Git blob `f2e2117a94a01e52799dfab49a46fce6eaab8748`；SHA-256 `2c87f247b90e7b774ced1e529d032f29ab78988c382a4d210bea7eb0498b3213`；96 行完整文件。

````cpp
// Independent GPLv3 validation: actual projector/image interpolation/pose chain.
#include "intensity/cube_image.hpp"
#include <Eigen/Geometry>
#include <random>
#include <iostream>
#include <stdexcept>
#include <iomanip>
using namespace cube;
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
Vec3 ray(int face,double a,double b){
  switch(face){case 0:return {1,a,b};case 1:return {-1,-a,b};
    case 2:return {-a,1,b};case 3:return {a,-1,b};
    case 4:return {a,b,1};default:return {a,-b,-1};}
}
int main(){
  try {
    CubeProjector projector(64);std::mt19937 rng(20261001);
    std::uniform_real_distribution<double> uniform(-.7,.7),depth(3,30);
    double worst_projection=0,worst_residual=0;
    for(int face=0;face<6;++face){
      require(projector.project(ray(face,0,0)).face==face,"six-face center selection");
      for(int n=0;n<1000;++n){
        Vec3 p=depth(rng)*ray(face,uniform(rng),uniform(rng));auto q=projector.project(p);
        require(q.face==face&&!q.seam,"random interior face");
        Eigen::Matrix<double,2,3> fd;
        for(int k=0;k<3;++k){Vec3 plus=p,minus=p;plus[k]+=1e-6;minus[k]-=1e-6;
          fd.col(k)=(projector.project(plus).uv-projector.project(minus).uv)/(2e-6);}
        worst_projection=std::max(worst_projection,(fd-q.jacobian).cwiseAbs().maxCoeff());
      }
    }
    require(worst_projection<2e-7,"projection Jacobian error");
    require(projector.project(Vec3::Zero()).face==-1,"zero point invalid");
    require(projector.project({NAN,1,1}).face==-1,"NaN point invalid");
    // Exact ties and tiny one-sided perturbations around every signed seam.
    for(int a=0;a<3;++a)for(int b=a+1;b<3;++b)for(int sa:{-1,1})for(int sb:{-1,1}){
      Vec3 p=Vec3::Zero();p[a]=sa;p[b]=sb;
      auto q=projector.project(p);require(q.face==2*a+(sa<0)&&q.seam,"deterministic seam tie");
      Vec3 left=p,right=p;left[a]*=1.+1e-5;right[b]*=1.+1e-5;
      require(projector.project(left).face==2*a+(sa<0)&&projector.project(right).face==2*b+(sb<0),"seam one-sided faces");
    }
    Settings cfg;cfg.resolution=64;cfg.idw_enable=false;CubeImage image(cfg);
    std::vector<ScanPoint> points;
    for(int face=0;face<6;++face)for(int v=0;v<64;++v)for(int u=0;u<64;++u){
      Vec3 p=ray(face,u/31.5-1.,v/31.5-1.);p=10.*p.normalized();
      points.push_back({p,250+20*std::sin(.14*u)+15*std::cos(.2*v)+.05*u*v});
    }
    image.build(points);
    Mat3 R=(Eigen::AngleAxisd(.3,Vec3::UnitZ())*Eigen::AngleAxisd(-.2,Vec3::UnitY())).toRotationMatrix();
    Vec3 t(1,-2,.5);Mat3 extr=Eigen::AngleAxisd(.1,Vec3::UnitX()).toRotationMatrix();Vec3 et(.04,-.03,.07);
    auto residual=[&](const Vec3& world,const Mat3& rotation,const Vec3& position,double ref){
      Vec3 p=landmarkInLidar(world,rotation,position,extr,et);Sample s;
      require(image.sample(image.projector.project(p),s),"residual sample validity");return s.value-ref;
    };
    int count=0;
    for(int face=0;face<6;++face)for(int j=0;j<200;++j){
      double a=uniform(rng),b=uniform(rng);
      double u=31.5*(a+1),v=31.5*(b+1);
      if(std::abs(u-std::round(u))<.02||std::abs(v-std::round(v))<.02)continue;
      Vec3 p=10.*ray(face,a,b).normalized(),world=R*(extr*p+et)+t;
      auto q=image.projector.project(p);Sample s;require(image.sample(q,s),"interior sample");
      Row6 analytic=residualJacobian(world,R,t,extr,et,q,s),fd;
      const double h=1e-6,reference=s.value+3.;
      for(int k=0;k<6;++k){
        Mat3 rp=R,rm=R;Vec3 tp=t,tm=t;
        if(k<3){rp=R*Eigen::AngleAxisd(h,Vec3::Unit(k)).toRotationMatrix();rm=R*Eigen::AngleAxisd(-h,Vec3::Unit(k)).toRotationMatrix();}
        else {tp[k-3]+=h;tm[k-3]-=h;}
        fd[k]=(residual(world,rp,tp,reference)-residual(world,rm,tm,reference))/(2*h);
      }
      worst_residual=std::max(worst_residual,(fd-analytic).cwiseAbs().maxCoeff());++count;
      // Information sign must be a descent step for this residual.
      Vec6 step=-analytic.transpose()*(s.value-reference)*1e-6;
      Mat3 moved=R;
      if(step.head<3>().norm()>0)moved=R*Eigen::AngleAxisd(step.head<3>().norm(),step.head<3>().normalized()).toRotationMatrix();
      double after=residual(world,moved,t+step.tail<3>(),reference);
      require(std::abs(after)<std::abs(s.value-reference),"information-form sign must descend");
    }
    require(worst_residual<2e-6,"full 6DoF residual Jacobian error");
    Sample sample;require(!image.sample(projector.project({1,1,0}),sample),"seam residual is explicitly rejected");
    // Empty masks never create intensity constraints.
    image.build({});require(!image.sample(projector.project({1,0,0}),sample),"invalid mask rejection");
    // IDW cannot fill a competing foreground/background layer.
    cfg.idw_enable=true;cfg.idw_radius=3;cfg.idw_k=6;cfg.idw_min_support=3;
    CubeImage sparse(cfg);std::vector<ScanPoint> sparse_points;
    for(int u:{30,32})for(int v:{30,32}){
      Vec3 p=ray(0,u/31.5-1.,v/31.5-1.).normalized();
      sparse_points.push_back({p*(u==30?5.:15.),double(u)});
    }
    sparse.build(sparse_points);require(!sparse.face(0).mask.at<uint8_t>(31,31),"IDW depth discontinuity rejection");
    for(auto& p:sparse_points)p.p=5.*p.p.normalized();
    sparse.build(sparse_points);require(sparse.face(0).mask.at<uint8_t>(31,31),"IDW same-surface interpolation");
    std::cout<<std::setprecision(12)<<"{\"projection_points\":6000,\"projection_max_abs_error\":"<<worst_projection
      <<",\"residual_points\":"<<count<<",\"residual_max_abs_error\":"<<worst_residual
      <<",\"all_six_dof_and_faces\":true,\"boundary_tests\":\"PASS\",\"idw_visibility\":\"PASS\",\"information_sign\":\"PASS\"}\n";
    return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
````

### tools/cube_lio/config/photo.yaml

Git blob `4fa353896b15208dc502805ab60072d558a6fba1`；SHA-256 `729127604870ec89a77f3aadab08645bef5a5c7b5ac3bfe4d1ae141ca4ccd44c`；26 行完整文件。

````yaml
# One conservative default; no ground-truth tuning. All values are engineering choices.
photo:
  enable: false
  projection: cubemap
  measurement: igm
  selector: all
  weakest_gate_threshold: 0.31913064578672057
  weight: 1.0
  max_features: 1200
  max_lifetime: 10
  high_response: 5.0
  suppression_radius: 2
  normalization_frames: 20
  sigma_min: 1.0
  robust_gate: 4.685
  huber_delta: 1.345
cubemap:
  resolution: 96
  idw_enable: true
  idw_radius: 3
  idw_k: 6
  idw_power: 2.0
  idw_min_support: 3
  range_absolute: 0.3
  range_relative: 0.02
  gaussian_sigma: 1.0
````

### tools/cube_lio/config/tunnel_d.yaml

Git blob `45af316ab8331e1250edbbab393532ee744b0c7b`；SHA-256 `c56df8526b3642208aa02b07141ab069f83abb4ec78e8f6dde4ea349aabeaaad`；63 行完整文件。

````yaml
lio:
  map:
    save_map: false
    if_filter: false
    save_map_dir: map
    map_name: map.pcd
    ds_size: 0.3
    save_interval: 500
  ros:
    lidar_topic: /ouster/points
    imu_topic: /ouster/imu
  sensor:
    lidar_type: 7
    blind: 2.0
    maxrange: 150.0
    filter_rate: 3.0
    enable_downsample: true
    voxel_fliter_size: 0.5
    gravity_norm: 9.7946
    imu_type: 0
    imu_na: 0.1
    imu_ng: 0.1
    imu_nba: 0.0001
    imu_nbg: 0.0001
  extrinsic:
    lidar_imu:
    - -0.00625
    - 0.011775
    - -0.007645
    - 1.0
    - 0.0
    - 0.0
    - 0.0
    - 1.0
    - 0.0
    - 0.0
    - 0.0
    - 1.0
    odom_robo:
    - 0
    - 0
    - 0
    - 0
    - 0
    - 0
  hash_map:
    hash_capacity: 100000000
    vox_resolution: 0.5
  kf:
    kf_type: 0
    kf_max_iterations: 4
    kf_align_gravity: true
    kf_quit_eps: 0.001
  output:
    robot: false
    plan_env_world: false
    plan_env_body: false
    planner: false
    map: false
    dense: true
    pub_step: 3
  eva:
    timer: true
````

### tools/offline/run.py

Git blob `65174bd0db2849e26e89c8182f34f91692c611d0`；SHA-256 `71c67778f03f9ce1fd8c7d3782f6f5bc9b0630ca6051f7ef60d5f5a546cb4b91`；222 行完整文件。

````python
#!/usr/bin/env python3
"""Run CUBE-LIO directly from a ROS1 bag with a bounded TBB scheduler."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import xmlrpc.client

ROOT = Path(__file__).resolve().parents[2]
BAGS = {
    'eee_01': Path('/home/lc/super_livo/bag/NTU/eee_01/eee_01.bag'),
    'tunnel_d': Path('/home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag'),
    'shield1': Path('/home/lc/algorithm_versa/bag/GEODE/Shield_tunnel1_gamma.bag'),
    'shield4': Path('/home/lc/algorithm_versa/bag/GEODE/Shield_tunnel4_gamma.bag'),
}
CONFIGS = {name: ROOT / 'tools/cube_lio/config' / f'{name}.yaml' for name in BAGS}
PHOTO_POLICIES = ('C0', 'C60', 'C100', 'K100')
COIN_SELECTORS = ('original', 'gradient', 'weakest', 'normalized', 's2', 'g1', 'g2')
PROJECTIONS = ('cubemap', 'equirectangular')
MEASUREMENTS = ('raw', 'igm')
PHOTO_SELECTORS = ('all', 'weakest')


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def positive_int(value):
    parsed = int(value)
    if parsed < 1:
        raise argparse.ArgumentTypeError('must be a positive integer')
    return parsed


def repo_path(path):
    path = Path(path)
    return path if path.is_absolute() else ROOT / path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dataset', choices=BAGS)
    parser.add_argument('--name', required=True, help='unique output folder name under runtime/')
    parser.add_argument('--port', type=int, default=11562)
    parser.add_argument('--threads', type=positive_int, default=32,
                        help='TBB worker limit (default: 32; use 1 for serial parity checks)')
    parser.add_argument('--photo', action='store_true', help='enable CUBE-LIO photometric updates')
    parser.add_argument('--photo-time-audit', action='store_true',
                        help='P3-R Shield4 per-scan photo/IMU support diagnostics')
    parser.add_argument('--photo-history-supported-only', action='store_true',
                        help='P3-R C arm: retain only photo points within propagated IMU history')
    parser.add_argument('--policy', choices=PHOTO_POLICIES, default='C0')
    parser.add_argument('--audit', action='store_true', help='write photometric information diagnostics')
    parser.add_argument('--idw-off', action='store_true')
    parser.add_argument('--projection', choices=PROJECTIONS, default='cubemap',
                        help='photo raster projection (default: cubemap)')
    parser.add_argument('--measurement', choices=MEASUREMENTS, default='igm',
                        help='sample raw intensity or IGM (default: igm)')
    parser.add_argument('--photo-selector', choices=PHOTO_SELECTORS, default='all',
                        help='feature selector for the photo side channel')
    parser.add_argument('--photo-gate-threshold', type=float, default=0.31913064578672057,
                        help='frozen Super-native weakest-direction gate threshold')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--coin', action='store_true', help='inject faithful COIN measurements')
    mode.add_argument('--shadow', action='store_true', help='run COIN features without injecting measurements')
    parser.add_argument('--selector', choices=COIN_SELECTORS, default='original')
    parser.add_argument('--gate-g1-threshold', type=float)
    parser.add_argument('--gate-g2-threshold', type=float)
    parser.add_argument('--audit-csv', type=Path, help='write Ouster timing diagnostics')
    parser.add_argument('--fusion-audit-json', type=Path)
    parser.add_argument('--geometry-rows', type=Path, help='diagnostic-only final geometry rows binary trace')
    args = parser.parse_args()

    if args.photo_time_audit or args.photo_history_supported_only:
        if args.dataset != 'shield4' or not args.photo:
            parser.error('P3-R timing controls require Shield4 and --photo')

    if Path(args.name).name != args.name or args.name in ('', '.', '..'):
        parser.error('--name must be a single folder name under runtime/')

    bag = BAGS[args.dataset]
    if not bag.is_file():
        parser.error(f'bag does not exist: {bag}')
    if (args.coin or args.shadow) and args.dataset != 'tunnel_d':
        parser.error('COIN selection is scoped to TunnelD')
    if args.coin and args.selector in ('g1', 'g2') and (
            args.gate_g1_threshold is None or args.gate_g2_threshold is None):
        parser.error('gated COIN selectors require geometry-derived G1 and G2 thresholds')

    out = ROOT / 'runtime' / args.name
    out.mkdir(parents=True, exist_ok=False)
    audit_csv = repo_path(args.audit_csv) if args.audit_csv else None
    fusion_audit_json = repo_path(args.fusion_audit_json) if args.fusion_audit_json else None
    geometry_rows = repo_path(args.geometry_rows) if args.geometry_rows else None
    for path in (audit_csv, fusion_audit_json, geometry_rows):
        if path:
            path.parent.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, ROS_MASTER_URI=f'http://127.0.0.1:{args.port}',
               ROS_HOSTNAME='127.0.0.1', ROS_HOME=str(out / 'ros_home'),
               ROS_LOG_DIR=str(out / 'ros_log'))
    config = CONFIGS[args.dataset]
    photo_config = ROOT / 'tools/cube_lio/config/photo.yaml'
    coin_configs = [ROOT / 'refs/COIN-LIO/config' / name
                    for name in ('params.yaml', 'line_removal.yaml', 'os_enwide.json')]
    paths = [config, photo_config] + (coin_configs if args.dataset == 'tunnel_d' else [])
    identity = {
        'dataset': args.dataset, 'bag': str(bag), 'bag_sha256': sha(bag),
        'threads': args.threads, 'photo': args.photo, 'photo_policy': args.policy,
        'photo_time_audit': args.photo_time_audit,
        'photo_history_supported_only': args.photo_history_supported_only,
        'photo_audit': args.audit, 'idw_enable': not args.idw_off,
        'projection': args.projection, 'measurement': args.measurement,
        'photo_selector': args.photo_selector,
        'photo_gate_threshold': args.photo_gate_threshold,
        'coin': args.coin, 'shadow': args.shadow, 'selector': args.selector,
        'gate_g1_threshold': args.gate_g1_threshold,
        'gate_g2_threshold': args.gate_g2_threshold,
        'audit_csv': str(audit_csv) if audit_csv else None,
        'fusion_audit_json': str(fusion_audit_json) if fusion_audit_json else None,
        'geometry_rows': str(geometry_rows) if geometry_rows else None,
        'source_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'source_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip()),
        'config_sha256': {str(p.relative_to(ROOT)): sha(p) for p in paths},
        'p3_source_sha256': {relative: sha(ROOT / relative) for relative in (
            'src/super_lio/include/intensity/intensity_representation.hpp',
            'src/super_lio/include/intensity/cube_image.hpp',
            'src/super_lio/include/intensity/spherical_image.hpp',
            'src/super_lio/include/intensity/photo_observation.hpp',
            'src/super_lio/src/intensity/cube_image.cpp',
            'src/super_lio/src/intensity/spherical_image.cpp',
            'src/super_lio/src/intensity/photo_observation.cpp',
            'src/super_lio/src/lio/super_lio.cpp',
            'tools/offline/run.py', 'eval/evaluate.py',
        )},
        'evaluator_sha256': sha(ROOT / 'eval/evaluate.py'),
        'binary_sha256': sha(ROOT / 'devel/lib/super_lio/cube_offline_node'),
        'lio_library_sha256': sha(ROOT / 'devel/lib/liblio.so'),
    }
    (out / 'identity.json').write_text(json.dumps(identity, indent=2) + '\n')
    master_log = (out / 'master.log').open('w')
    master = subprocess.Popen(['roscore', '-p', str(args.port)], cwd=ROOT, env=env,
                              stdout=master_log, stderr=master_log)
    result = {'status': 'NOT_STARTED', 'node_exit_code': None, 'evaluator_exit_code': None}
    try:
        for _ in range(150):
            try:
                xmlrpc.client.ServerProxy(env['ROS_MASTER_URI']).getPid('/offline_runner')
                break
            except OSError:
                if master.poll() is not None:
                    raise RuntimeError('roscore exited')
                time.sleep(.1)
        else:
            raise RuntimeError('roscore startup timeout')

        for path in paths:
            subprocess.run(['rosparam', 'load', str(path)], env=env, check=True)
        params = {
            '/lio/offline/bag': str(bag), '/lio/offline/out_dir': str(out),
            '/lio/offline/threads': str(args.threads),
            '/photo/enable': str(args.photo).lower(),
            '/p3r/photo_time_audit': str(args.photo_time_audit).lower(),
            '/p3r/photo_history_supported_only': str(args.photo_history_supported_only).lower(),
            '/photo/projection': args.projection,
            '/photo/measurement': args.measurement,
            '/photo/selector': args.photo_selector,
            '/photo/weakest_gate_threshold': str(args.photo_gate_threshold),
            '/photo/information_policy': args.policy,
            '/photo/information_audit': str(args.audit).lower(),
            '/coin/enable': str(args.coin or args.shadow).lower(),
            '/coin/shadow': str(args.shadow).lower(), '/coin/selector_mode': args.selector,
            '/image/u_shift': '0', '/coin/measurement_variance': '0.001',
            '/p2r/time_audit_path': str(audit_csv) if audit_csv else '',
            '/p2r/fusion_audit_path': str(fusion_audit_json) if fusion_audit_json else '',
            '/p2s/geometry_rows_path': str(geometry_rows) if geometry_rows else '',
        }
        if args.idw_off:
            params['/cubemap/idw_enable'] = 'false'
        if args.gate_g1_threshold is not None:
            params['/coin/gate_g1_confidence_threshold'] = str(args.gate_g1_threshold)
        if args.gate_g2_threshold is not None:
            params['/coin/gate_g2_confidence_threshold'] = str(args.gate_g2_threshold)
        for key, value in params.items():
            subprocess.run(['rosparam', 'set', key, value], env=env, check=True)

        with (out / 'node.log').open('w') as log:
            node = subprocess.run([str(ROOT / 'devel/lib/super_lio/cube_offline_node')],
                                  cwd=ROOT, env=env, stdout=log, stderr=log)
        result['node_exit_code'] = node.returncode
        if node.returncode == 0:
            with (out / 'evaluator.log').open('w') as log:
                evaluator = subprocess.run(['python3', str(ROOT / 'eval/evaluate.py'),
                                            args.dataset, str(out)], cwd=ROOT, env=env,
                                           stdout=log, stderr=subprocess.STDOUT)
            result['evaluator_exit_code'] = evaluator.returncode
            result['status'] = 'SUCCESS' if evaluator.returncode == 0 else 'EVALUATION_FAILED'
        else:
            result['status'] = 'ESTIMATOR_FAILED'
    except Exception as exc:
        result.update(status='RUNNER_FAILED', error=str(exc))
    finally:
        master.terminate()
        try:
            master.wait(timeout=10)
        except subprocess.TimeoutExpired:
            master.kill()
            master.wait()
        master_log.close()
        (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
    return 0 if result['status'] == 'SUCCESS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
````

### tools/offline/run_experiment.sh

Git blob `e5b6b0a34690409253cd070ffb2d07d84847eea9`；SHA-256 `36967d27596c7f8f0b4235db92cfcb10b8edf6295befe3bf564640c9f680edff`；10 行完整文件。

````bash
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
if [[ $# -lt 1 ]]; then
  echo "Usage: tools/offline/run_experiment.sh {eee_01|tunnel_d|shield1} --name RUN_NAME [offline runner options]" >&2
  exit 2
fi
cd "$ROOT"
exec python3 "$ROOT/tools/offline/run.py" "$@"
````

### tools/p3a/validate.py

Git blob `0d13f9393fd4649424a69295b71ffb70d442003f`；SHA-256 `d39331a5594a458730e784b5b195ccec4e11c1c844b623c7e0fffc4484fdabd4`；238 行完整文件。

````python
#!/usr/bin/env python3
"""P3-A orchestration/archival; delegates replay and evaluation unchanged."""
import csv
import argparse
import hashlib
import json
import math
import os
import re
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'artifacts/p3a'
PREFIX = 'p3a'
BASE = '04a48588bcdc4a2210b189aaefe2cf3a29f48302'
G1 = '0.31913064578672057'
G2 = '0.359189249724233'
MODES = {'R0': 'coin', 'R1': 'cube_raw_no_idw',
         'R2': 'cube_raw_idw', 'R3': 'cube_igm_idw'}


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def load(path):
    return json.loads(path.read_text())


def diagnostics(path):
    with path.open() as stream:
        rows = list(csv.DictReader(stream))
    def values(key):
        return [float(row[key]) for row in rows if row.get(key, '') != '']
    result = {'scans': len(rows), 'csv_sha256': sha(path)}
    for key in ('active_before', 'active_after', 'valid_patches', 'photo_rows',
                'residual_rms', 'ncc_median', 'gate_active', 'motion_fallback_points'):
        data = values(key)
        result[key] = {'mean': sum(data)/len(data), 'min': min(data),
                       'max': max(data), 'sum': sum(data)} if data else None
    photo_rows = sum(float(row['photo_rows']) for row in rows)
    result['pooled_residual_rms'] = math.sqrt(sum(
        float(row['photo_rows'])*float(row['residual_rms'])**2 for row in rows)/photo_rows) if photo_rows else None
    result['skipped_scans'] = sum(row['status'] != 'USED' for row in rows)
    return result


def summarize(name, representation, selector):
    folder = ROOT / 'runtime' / name
    result = load(folder / 'result.json')
    if result['status'] != 'SUCCESS':
        raise RuntimeError(f'{name} failed: {result}')
    evaluation = load(folder / 'evaluation.json')
    if not math.isfinite(evaluation['ate_rmse_m']):
        raise RuntimeError(f'{name}: nonfinite ATE')
    record = {'name': name, 'representation': representation, 'selector': selector,
              'trajectory_sha256': sha(folder / 'trajectory.tum'),
              'evaluation_sha256': sha(folder / 'evaluation.json'),
              'evaluation': evaluation, 'runtime': load(folder / 'run.json'),
              'identity': load(folder / 'identity.json'),
              'diagnostics': diagnostics(folder / 'coin_observation.csv')}
    audit = folder / 'fusion.json'
    if audit.exists():
        record['fusion_audit'] = load(audit)
        if record['fusion_audit']['status'] != 'PASS':
            raise RuntimeError(f'{name}: fusion audit failed')
    representation_csv = folder / 'representation.csv'
    if representation_csv.exists():
        with representation_csv.open() as stream:
            rows = list(csv.DictReader(stream))
        record['representation_diagnostics'] = {
            'scans': len(rows), 'sha256': sha(representation_csv),
            'sum': {key: sum(int(row[key]) for row in rows) for key in
                    ('input_points', 'raw_pixels', 'filled_pixels', 'igm_pixels',
                     'patch_mask_pixels', 'intensity_points')}}
        shutil.copyfile(representation_csv, OUT / 'diagnostics' / f'{name}_representation.csv')
    shutil.copyfile(folder / 'coin_observation.csv', OUT / 'diagnostics' / f'{name}_coin.csv')
    return record


def run(representation, selector, repeat, port):
    name = f'{PREFIX}_{representation.lower()}_{selector}_run{repeat}'
    command = ['tools/offline/run_experiment.sh', 'tunnel_d', '--name', name,
               '--port', str(port), '--threads', '32', '--coin', '--selector', selector,
               '--gate-g1-threshold', G1, '--gate-g2-threshold', G2,
               '--fusion-audit-json', f'runtime/{name}/fusion.json']
    mode = MODES[representation]
    with (OUT / 'commands.md').open('a') as stream:
        stream.write('\n```bash\nCUBE_P3A_REPRESENTATION=' + mode + ' ' + ' '.join(command) + '\n```\n')
    env = dict(os.environ, CUBE_P3A_REPRESENTATION=mode)
    print(json.dumps({'starting': name, 'mode': mode, 'threads': 32}), flush=True)
    with (ROOT / 'runtime' / f'{name}_launch.log').open('w') as log:
        subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=log, check=True)
    record = summarize(name, mode, selector)
    record['command'] = command
    record['representation_environment'] = {'CUBE_P3A_REPRESENTATION': mode}
    record['build_source_sha256'] = load(OUT / 'source_manifest.json')['working_source_sha256']
    print(json.dumps({'finished': name, 'ate_rmse_m': record['evaluation']['ate_rmse_m'],
                      'sha256': record['trajectory_sha256']}), flush=True)
    return record


def source_manifest():
    paths = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard',
                                      'src/super_lio', 'src/basic', 'tools/offline', 'eval',
                                      'tools/cube_lio/config'], cwd=ROOT, text=True).splitlines()
    working = {path: sha(ROOT / path) for path in paths if (ROOT / path).is_file()}
    changed = {}
    for path, digest in working.items():
        original = subprocess.run(['git', 'show', f'{BASE}:{path}'], cwd=ROOT,
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        previous = hashlib.sha256(original.stdout).hexdigest() if original.returncode == 0 else None
        if previous != digest:
            changed[path] = {'base_sha256': previous, 'working_sha256': digest}
    protected = ['src/super_lio/src/lio/', 'src/super_lio/include/lio/',
                 'src/super_lio/src/ros/', 'src/super_lio/include/ros/', 'src/basic/',
                 'src/super_lio/src/apps/', 'tools/offline/', 'eval/', 'tools/cube_lio/config/']
    if any(any(path.startswith(prefix) for prefix in protected) or
           path.endswith('super_degeneracy_gate.hpp') for path in changed):
        raise RuntimeError('frozen component changed')
    functions = {
        'fusion_add': ('src/super_lio/src/intensity/coin/coin_observation.cpp',
                       'void CoinObservation::add(', 'void CoinObservation::finish('),
        'gate_and_selector_finish': ('src/super_lio/src/intensity/coin/coin_observation.cpp',
                                     'void CoinObservation::finish(', None),
        'feature_track_detect': ('src/super_lio/src/intensity/coin/coin_feature_manager.cpp',
                                 'void CoinFeatureManager::update(', None),
    }
    function_checks = {}
    for label, (path, start, end) in functions.items():
        before = subprocess.check_output(['git', 'show', f'{BASE}:{path}'], cwd=ROOT, text=True)
        after = (ROOT / path).read_text()
        def section(value):
            value = value[value.index(start):]
            return value[:value.index(end)] if end else value
        previous, current = section(before), section(after)
        function_checks[label] = {'identical': previous == current,
                                 'sha256': hashlib.sha256(current.encode()).hexdigest()}
        if previous != current:
            raise RuntimeError(f'frozen function changed: {label}')
    return {'base_commit': BASE, 'working_source_sha256': working,
            'changed_source_files': changed, 'frozen_components_unchanged': True,
            'frozen_function_checks': function_checks,
            'binary_sha256': sha(ROOT / 'devel/lib/super_lio/cube_offline_node'),
            'lio_library_sha256': sha(ROOT / 'devel/lib/liblio.so'),
            'switch': 'CUBE_P3A_REPRESENTATION; opt-in, default coin',
            'frozen_settings': {'selector': 'g1', 'g1_threshold': float(G1), 'g2_threshold': float(G2),
                'photo_scale': .00095, 'measurement_variance': .001, 'num_features': 60,
                'patch_size': 5, 'max_lifetime': 25, 'suppression_radius': 10, 'grad_min': 16.5,
                'ncc_threshold': .7075, 'margin': 10, 'range_threshold': .2,
                'resolution': 96, 'idw_radius': 3, 'idw_k': 6, 'idw_min_support': 3,
                'idw_power': 2., 'range_absolute': .3, 'range_relative': .02, 'gaussian_sigma': 1.}}


def repeat_check(representation, selector, first, port):
    repeats = [first] + [run(representation, selector, repeat, port+repeat-2) for repeat in (2, 3)]
    identical_sha = len({record['trajectory_sha256'] for record in repeats}) == 1
    identical_metrics = all(record['evaluation'] == first['evaluation'] for record in repeats)
    identical_diagnostics = len({record['diagnostics']['csv_sha256'] for record in repeats}) == 1
    return {'representation': representation, 'selector': selector, 'runs': repeats,
            'trajectory_sha_identical': identical_sha, 'evaluation_metrics_identical': identical_metrics,
            'coin_diagnostics_identical': identical_diagnostics,
            'pass': identical_sha and identical_metrics and identical_diagnostics}


def main():
    global OUT, PREFIX
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=Path('artifacts/p3a'))
    parser.add_argument('--prefix', default='p3a', help='fresh runtime folder prefix for repeats/ablations')
    parser.add_argument('--baseline-run', default='p3a_r0_coin_g1_run1', help='completed native COIN G1 Phase 0 run')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_]+', args.prefix) or Path(args.baseline_run).name != args.baseline_run:
        parser.error('prefix and baseline-run must be single safe folder names')
    PREFIX = args.prefix
    OUT = args.output_dir if args.output_dir.is_absolute() else ROOT / args.output_dir
    OUT.mkdir(exist_ok=False, parents=True)
    (OUT / 'diagnostics').mkdir()
    write('source_manifest.json', source_manifest())
    (OUT / 'commands.md').write_text('# P3-A commands\n\nAll replays use the frozen offline runner and 32T.\n'
        'The explicit environment switch is recorded separately because the frozen runner does not record it.\n'
        'Source and binary hashes are in `source_manifest.json` and each result. No configurations are edited.\n\n'
        '```bash\ngit switch -c p3a-tunneld-cubemap-validation\n'
        'tools/offline/run_experiment.sh tunnel_d --name p3a_r0_coin_g1_run1 --port 11740 --threads 32 '
        '--coin --selector g1 --gate-g1-threshold ' + G1 + ' --gate-g2-threshold ' + G2 + ' '
        '--fusion-audit-json runtime/p3a_r0_coin_g1_run1/fusion.json\n'
        'source /opt/ros/noetic/setup.bash\nsource devel/setup.bash\n'
        'catkin_make --pkg super_lio --make-args cube_offline_node test_coin_feature_math test_cube_photo -j2\n'
        'devel/lib/super_lio/test_coin_feature_math\ndevel/lib/super_lio/test_cube_photo\n'
        'python3 tools/p3a/validate.py\n```\n')
    baseline = summarize(args.baseline_run, 'coin', 'g1')
    write('baseline_coin.json', baseline)
    # Verify opt-in isolation before any Cubemap run; these are also R0's
    # required three repeats, including the clean, pre-integration Phase 0 run.
    checks = [repeat_check('R0', 'g1', baseline, 11745)]
    if not checks[0]['pass']:
        raise RuntimeError('default COIN baseline changed after representation integration')
    records = {'R0': baseline}
    for index, representation in enumerate(('R1', 'R2', 'R3')):
        records[representation] = run(representation, 'g1', 1, 11741+index)
        write('cubemap_results.json', {'same_selector_feature_limit_fusion': True, 'results': records})
    best = min(('R1', 'R2', 'R3'), key=lambda key: records[key]['evaluation']['ate_rmse_m'])
    all_use = run(best, 'gradient', 1, 11744)
    write('selector_ablation.json', {'best_cubemap': best, 'selection_rule': 'minimum R1/R2/R3 full-bag ATE; no tuning',
          'weakest_with_super_native_gate': records[best], 'all_use_no_direction_dependency': all_use,
          'all_use_means': 'existing COIN gradient mode: pure gradient ranking, same cap/NCC/fusion; gate not used'})
    candidates = [(key, record['selector'], record) for key, record in records.items()
                  if key != 'R0' and record['evaluation']['ate_rmse_m'] < 10.]
    if all_use['evaluation']['ate_rmse_m'] < 10.:
        candidates.append((best, 'gradient', all_use))
    for index, (representation, selector, first) in enumerate(candidates):
        checks.append(repeat_check(representation, selector, first, 11747+index*2))
    write('determinism.json', {'criterion': 'three runs for every ATE<10 candidate', 'checks': checks,
          'runtime_not_required_identical': True, 'pass': all(check['pass'] for check in checks)})
    if not all(check['pass'] for check in checks):
        raise RuntimeError('determinism failed')
    original = ROOT / 'runtime/offline_infra_tunneld_g1_threads32'
    write('baseline_parity.json', {'reference': str(original.relative_to(ROOT)),
        'reference_trajectory_sha256': sha(original / 'trajectory.tum'),
        'phase0_trajectory_sha256': baseline['trajectory_sha256'],
        'identical': sha(original / 'trajectory.tum') == baseline['trajectory_sha256']})
    print(json.dumps({'complete': True, 'best_cubemap': best,
        'best_ate_rmse_m': records[best]['evaluation']['ate_rmse_m'],
        'cubemap_below_10m': any(records[key]['evaluation']['ate_rmse_m'] < 10. for key in ('R1', 'R2', 'R3'))}), flush=True)


if __name__ == '__main__':
    main()
````

