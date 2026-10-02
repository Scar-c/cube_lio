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
