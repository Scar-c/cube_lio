# Complete source snapshot for P3 review

Every file below is copied in full from commit `f3060768abee4d2ee2343b07edf62346c7cd0f87` with `git archive`; no source excerpts or transformations are used. `Git blob` is computed from the exported file so reviewers can verify the snapshot against the commit tree. `P3 diff` indicates whether the path itself was added or modified by this commit; unchanged paths are included only as implementation dependencies or adaptation context.

| Repository path (also the path under `f3060768/`) | Lines | Git blob | P3 diff |
|---|---:|---|:---:|
| `src/super_lio/include/intensity/coin/coin_image_processor.hpp` | 49 | `ee5c206b9a68d692b8c3043027f08130c6525feb` | no |
| `src/super_lio/include/intensity/coin/coin_intensity_representation.hpp` | 79 | `990685778648cba2ad56183fb8f780b3279341d8` | yes |
| `src/super_lio/include/intensity/coin/coin_ouster_projector.hpp` | 49 | `0de83ef251e7f3deac57726b1ca14f3186791c6e` | no |
| `src/super_lio/include/intensity/coin/coin_photometric_model.hpp` | 41 | `fba1ce0c0a7359bda4acccdbff80e0eb3ffa0050` | no |
| `src/super_lio/include/intensity/cube_image.hpp` | 75 | `db16f7a72630c0e5c3090dd2c25c6ec94dcc8415` | yes |
| `src/super_lio/include/intensity/cube_projector.hpp` | 51 | `e93414e3ef86e1da333715f48b1bd07814b247f0` | yes |
| `src/super_lio/include/intensity/intensity_representation.hpp` | 64 | `458207091ee23ce6d3149530c70cf64a9a3bc841` | yes |
| `src/super_lio/include/intensity/photo_observation.hpp` | 68 | `bacd716b9465a877a0fa09386995dd3a20bd63b8` | yes |
| `src/super_lio/include/intensity/spherical_image.hpp` | 51 | `6006e26735dbaf320fd377e6136c2c6ea760c52c` | yes |
| `src/super_lio/include/ros/ROSWrapper.h` | 140 | `a275b2fda34506a16cc066f16ad5b69e109fe895` | no |
| `src/super_lio/src/intensity/coin/coin_image_processor.cpp` | 99 | `1cc5dd416ee054ec80f238ebeee3e36f23132fb2` | no |
| `src/super_lio/src/intensity/coin/coin_ouster_projector.cpp` | 92 | `a04fd02c9f066de58587f6c66a05475390c01c07` | no |
| `src/super_lio/src/intensity/coin/coin_photometric_model.cpp` | 101 | `a6a8c2ec94f04870a339f60a846381687d6330a6` | no |
| `src/super_lio/src/intensity/cube_image.cpp` | 154 | `ce4bf20364b07c0d3f918a4dbf6ded7bf2d2d65f` | yes |
| `src/super_lio/src/intensity/photo_observation.cpp` | 350 | `a61abcd9884098326b30fcd76e35a3ba31fce36b` | yes |
| `src/super_lio/src/intensity/spherical_image.cpp` | 193 | `4a4c5b8a827c40374607f7e369249b31d694607a` | yes |
| `src/super_lio/src/lio/super_lio.cpp` | 759 | `85a9ca5e30a27374d9d8a1cb321b329753c096e6` | yes |
| `src/super_lio/src/ros/ROSWrapper.cpp` | 809 | `7d61a176f02b72f574e1d3f43c70b18b6ba73735` | no |
| `src/super_lio/test/test_coin_feature_math.cpp` | 111 | `02c4610fe8f5eb5b75bdb5ee709560786ec6dc43` | yes |
| `src/super_lio/test/test_spherical_photo.cpp` | 116 | `5af536618764675ab36af4f91a8e72ce23676e5f` | yes |
| `tools/cube_lio/config/photo.yaml` | 26 | `4fa353896b15208dc502805ab60072d558a6fba1` | yes |
| `tools/cube_lio/config/shield4.yaml` | 63 | `b057e16c7c1d18a4b8abfc181d6a8bb03d7b3142` | yes |
| `tools/offline/run.py` | 208 | `70b0c2c8fe0d38cad2b1c446a944518d410c50a9` | yes |
