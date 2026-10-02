# P3 commit file list

Commit: `f3060768abee4d2ee2343b07edf62346c7cd0f87`

The following blocks are the outputs of `git show --stat f3060768` and `git show --name-only f3060768` at the audited commit.

## `git show --stat f3060768`

```text
commit f3060768abee4d2ee2343b07edf62346c7cd0f87
Author: Scar-c <lc1365142579@gmail.com>
Date:   Fri Oct 2 01:42:11 2026 +0800

    Add P3 intensity representation cubemap ablations

 artifacts/p3/baseline.json                         | 282 +++++++
 artifacts/p3/commands.md                           |  65 ++
 artifacts/p3/determinism.json                      |  61 ++
 artifacts/p3/ntu_results.json                      | 345 +++++++++
 artifacts/p3/projection_results.json               | 840 +++++++++++++++++++++
 artifacts/p3/selector_ablation.json                | 660 ++++++++++++++++
 artifacts/p3/shield4_results.json                  | 575 ++++++++++++++
 artifacts/p3/tunneld_results.json                  | 593 +++++++++++++++
 eval/evaluate.py                                   |   3 +-
 spec/p3/CUBE_DESIGN_REVIEW.md                      |  44 ++
 spec/p3/P3_FINAL_REPORT.md                         |  47 ++
 spec/p3/PROJECTION_DESIGN_REVIEW.md                |  20 +
 spec/p3/SHIELD4_ADAPTATION.md                      |  36 +
 src/super_lio/CMakeLists.txt                       |   6 +
 .../coin/coin_intensity_representation.hpp         |  64 +-
 src/super_lio/include/intensity/cube_image.hpp     |  35 +-
 src/super_lio/include/intensity/cube_projector.hpp |   7 +-
 .../include/intensity/intensity_representation.hpp |  60 +-
 .../include/intensity/photo_observation.hpp        |  15 +-
 .../include/intensity/spherical_image.hpp          |  51 ++
 src/super_lio/src/intensity/cube_image.cpp         |  46 +-
 src/super_lio/src/intensity/photo_observation.cpp  | 163 +++-
 src/super_lio/src/intensity/spherical_image.cpp    | 193 +++++
 src/super_lio/src/lio/super_lio.cpp                |   5 +-
 src/super_lio/test/test_coin_feature_math.cpp      |   2 +
 src/super_lio/test/test_spherical_photo.cpp        | 116 +++
 tools/cube_lio/config/photo.yaml                   |   4 +
 tools/cube_lio/config/shield4.yaml                 |  63 ++
 tools/offline/archive_p3_results.py                | 248 ++++++
 tools/offline/run.py                               |  34 +-
 30 files changed, 4590 insertions(+), 93 deletions(-)
```

## `git show --name-only f3060768`

```text
commit f3060768abee4d2ee2343b07edf62346c7cd0f87
Author: Scar-c <lc1365142579@gmail.com>
Date:   Fri Oct 2 01:42:11 2026 +0800

    Add P3 intensity representation cubemap ablations

artifacts/p3/baseline.json
artifacts/p3/commands.md
artifacts/p3/determinism.json
artifacts/p3/ntu_results.json
artifacts/p3/projection_results.json
artifacts/p3/selector_ablation.json
artifacts/p3/shield4_results.json
artifacts/p3/tunneld_results.json
eval/evaluate.py
spec/p3/CUBE_DESIGN_REVIEW.md
spec/p3/P3_FINAL_REPORT.md
spec/p3/PROJECTION_DESIGN_REVIEW.md
spec/p3/SHIELD4_ADAPTATION.md
src/super_lio/CMakeLists.txt
src/super_lio/include/intensity/coin/coin_intensity_representation.hpp
src/super_lio/include/intensity/cube_image.hpp
src/super_lio/include/intensity/cube_projector.hpp
src/super_lio/include/intensity/intensity_representation.hpp
src/super_lio/include/intensity/photo_observation.hpp
src/super_lio/include/intensity/spherical_image.hpp
src/super_lio/src/intensity/cube_image.cpp
src/super_lio/src/intensity/photo_observation.cpp
src/super_lio/src/intensity/spherical_image.cpp
src/super_lio/src/lio/super_lio.cpp
src/super_lio/test/test_coin_feature_math.cpp
src/super_lio/test/test_spherical_photo.cpp
tools/cube_lio/config/photo.yaml
tools/cube_lio/config/shield4.yaml
tools/offline/archive_p3_results.py
tools/offline/run.py
```
