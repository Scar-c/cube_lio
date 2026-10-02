# P3-A efeb69bf code audit package

目标代码提交：[`efeb69bf6f4814b83cc8709eb06d792ffb47e41e`](https://github.com/Scar-c/cube_lio/commit/efeb69bf6f4814b83cc8709eb06d792ffb47e41e)。

本次补充供外部 reviewer 审查的实际源码材料。导出与 byte/hash 核验已完成；没有修改生产代码、重跑实验或声称独立代码审查已经通过。

## 下载 / 阅读

- [单文件 Markdown：完整核心 diff + 35 个完整源码/配置](P3A_EFEB69BF_CODE_REVIEW_BUNDLE.md)，可直接发给 reviewer。
- [ZIP：完整 commit patch + 源码前后快照 + JSON 证据 + SHA 清单](../../artifacts/p3a/efeb69bf_code_audit.zip)。大小 2,692,629 bytes；SHA-256 `69cdeee2f5d38701a39fb0ea74fc32220559fe51e2d5fa0c25ca2eb9b3f52eec`。
- [原始 full patch：全部 31 个文件](review_efeb69bf/COMMIT_FULL.patch)。
- [核心 code diff：9 个文件](review_efeb69bf/CORE_CODE.diff)。
- [审计定位、实际调用链与测试覆盖边界](review_efeb69bf/README.md)。
- [文件清单](review_efeb69bf/COMMIT_FILE_LIST.md)、[export manifest](review_efeb69bf/EXPORT_MANIFEST.json)、[字节/冻结函数核验](review_efeb69bf/EXPORT_VERIFICATION.json)。

## 需要 reviewer 特别注意

P3A 的 CubeImage 结果导出给 COIN patch pipeline，实际 residual/J 使用 CoinPhotometricModel。
此前 1120 residual finite-difference points 验证的是旧 Cube exact-bilinear 路径；新增 bridge test 的 intensity 为常数，不能代替新 production-path 非恒定 raw/IGM 六自由度 Jacobian audit。
未修改融合函数与参数不意味着图像采样、有效行或每条 row 的 J/r 不变。
同 SHA 的 R3 selector 对照发生在特征 cap 不约束且定位失败的情形，不能解释为成功减少弱方向依赖。

原报告保持原始实验记录；以上说明补充其测试覆盖的准确边界。证据不足的地方仍交由代码级 review 判断。
