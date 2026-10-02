# P3-A efeb69bf commit file list

Target: `efeb69bf6f4814b83cc8709eb06d792ffb47e41e`

Parent: `04a48588bcdc4a2210b189aaefe2cf3a29f48302`

## git show --stat

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

## git show --name-only

```text
commit efeb69bf6f4814b83cc8709eb06d792ffb47e41e
Author: Scar-c <lc1365142579@gmail.com>
Date:   Fri Oct 2 11:29:32 2026 +0800

    Validate TunnelD cubemap representations with frozen COIN fusion

artifacts/p3a/baseline_coin.json
artifacts/p3a/baseline_parity.json
artifacts/p3a/checks.json
artifacts/p3a/commands.md
artifacts/p3a/control_validation.json
artifacts/p3a/cubemap_results.json
artifacts/p3a/determinism.json
artifacts/p3a/diagnostics/p3a_r0_coin_g1_run1_coin.csv
artifacts/p3a/diagnostics/p3a_r0_g1_run2_coin.csv
artifacts/p3a/diagnostics/p3a_r0_g1_run3_coin.csv
artifacts/p3a/diagnostics/p3a_r1_g1_run1_coin.csv
artifacts/p3a/diagnostics/p3a_r1_g1_run1_representation.csv
artifacts/p3a/diagnostics/p3a_r2_g1_run1_coin.csv
artifacts/p3a/diagnostics/p3a_r2_g1_run1_representation.csv
artifacts/p3a/diagnostics/p3a_r3_g1_run1_coin.csv
artifacts/p3a/diagnostics/p3a_r3_g1_run1_representation.csv
artifacts/p3a/diagnostics/p3a_r3_gradient_run1_coin.csv
artifacts/p3a/diagnostics/p3a_r3_gradient_run1_representation.csv
artifacts/p3a/selector_ablation.json
artifacts/p3a/source_manifest.json
spec/p3a/P3A_PROMPT.md
spec/p3a/P3A_TUNNELD_CUBEMAP_VALIDATION_REPORT.md
src/super_lio/include/intensity/coin/coin_cubemap_representation.hpp
src/super_lio/include/intensity/coin/coin_observation.hpp
src/super_lio/include/intensity/coin/coin_ouster_projector.hpp
src/super_lio/src/intensity/coin/coin_cubemap_representation.cpp
src/super_lio/src/intensity/coin/coin_feature_manager.cpp
src/super_lio/src/intensity/coin/coin_observation.cpp
src/super_lio/src/intensity/coin/coin_ouster_projector.cpp
src/super_lio/test/test_coin_feature_math.cpp
tools/p3a/validate.py
```

## git diff --name-status

```text
A	artifacts/p3a/baseline_coin.json
A	artifacts/p3a/baseline_parity.json
A	artifacts/p3a/checks.json
A	artifacts/p3a/commands.md
A	artifacts/p3a/control_validation.json
A	artifacts/p3a/cubemap_results.json
A	artifacts/p3a/determinism.json
A	artifacts/p3a/diagnostics/p3a_r0_coin_g1_run1_coin.csv
A	artifacts/p3a/diagnostics/p3a_r0_g1_run2_coin.csv
A	artifacts/p3a/diagnostics/p3a_r0_g1_run3_coin.csv
A	artifacts/p3a/diagnostics/p3a_r1_g1_run1_coin.csv
A	artifacts/p3a/diagnostics/p3a_r1_g1_run1_representation.csv
A	artifacts/p3a/diagnostics/p3a_r2_g1_run1_coin.csv
A	artifacts/p3a/diagnostics/p3a_r2_g1_run1_representation.csv
A	artifacts/p3a/diagnostics/p3a_r3_g1_run1_coin.csv
A	artifacts/p3a/diagnostics/p3a_r3_g1_run1_representation.csv
A	artifacts/p3a/diagnostics/p3a_r3_gradient_run1_coin.csv
A	artifacts/p3a/diagnostics/p3a_r3_gradient_run1_representation.csv
A	artifacts/p3a/selector_ablation.json
A	artifacts/p3a/source_manifest.json
A	spec/p3a/P3A_PROMPT.md
A	spec/p3a/P3A_TUNNELD_CUBEMAP_VALIDATION_REPORT.md
A	src/super_lio/include/intensity/coin/coin_cubemap_representation.hpp
M	src/super_lio/include/intensity/coin/coin_observation.hpp
M	src/super_lio/include/intensity/coin/coin_ouster_projector.hpp
A	src/super_lio/src/intensity/coin/coin_cubemap_representation.cpp
M	src/super_lio/src/intensity/coin/coin_feature_manager.cpp
M	src/super_lio/src/intensity/coin/coin_observation.cpp
M	src/super_lio/src/intensity/coin/coin_ouster_projector.cpp
M	src/super_lio/test/test_coin_feature_math.cpp
A	tools/p3a/validate.py
```

