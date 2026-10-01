# P3 command ledger

Run from `/home/lc/cube_lio` in the configured ROS/catkin environment. P3 bag runs use the offline runner with TBB limited to 32. Run folders and full trajectories are ignored under `runtime/`; compact evaluation, identity, and trajectory SHA records are archived under `artifacts/p3/`.

Build and verification:

```bash
catkin_make --pkg super_lio --make-args cube_offline_node test_spherical_photo test_cube_photo test_coin_feature_math -j2
devel/lib/super_lio/test_spherical_photo
devel/lib/super_lio/test_cube_photo
devel/lib/super_lio/test_coin_feature_math
python3 -m py_compile tools/offline/run.py tools/offline/archive_p3_results.py eval/evaluate.py
git diff --check
```

TunnelD runs, executed first:

```bash
python3 tools/offline/run.py tunnel_d --name p3_tunneld_geometry32 --threads 32
python3 tools/offline/run.py tunnel_d --name p3_tunneld_coin_weakest32 --threads 32 --coin --selector weakest
python3 tools/offline/run.py tunnel_d --name p3_tunneld_coin_alluse32 --threads 32 --coin --selector gradient
python3 tools/offline/run.py tunnel_d --name p3_tunneld_cube_raw_c0_all32 --threads 32 --photo --projection cubemap --measurement raw --photo-selector all --idw-off
python3 tools/offline/run.py tunnel_d --name p3_tunneld_cube_raw_c1_all32 --threads 32 --photo --projection cubemap --measurement raw --photo-selector all
python3 tools/offline/run.py tunnel_d --name p3_tunneld_sphere_raw_c1_all32 --threads 32 --photo --projection equirectangular --measurement raw --photo-selector all
python3 tools/offline/run.py tunnel_d --name p3_tunneld_cube_igm_all32_det1 --threads 32 --photo --projection cubemap --measurement igm --photo-selector all
python3 tools/offline/run.py tunnel_d --name p3_tunneld_cube_igm_all32_det2 --threads 32 --photo --projection cubemap --measurement igm --photo-selector all
python3 tools/offline/run.py tunnel_d --name p3_tunneld_cube_igm_weakest32 --threads 32 --photo --projection cubemap --measurement igm --photo-selector weakest
python3 tools/offline/run.py tunnel_d --name p3_tunneld_sphere_igm_c2_all32 --threads 32 --photo --projection equirectangular --measurement igm --photo-selector all
```

The archived COIN+G1 32T offline baseline is `runtime/offline_infra_tunneld_g1_threads32`. Its prior stream-replay comparison and the SHA-identical 4T reference are retained in `artifacts/offline_infra/g1_cross_thread_parity.json`; no duplicate baseline replay was needed for this report.

NTU `eee_01` runs, executed second:

```bash
python3 tools/offline/run.py eee_01 --name p3_ntu_geometry32 --threads 32
python3 tools/offline/run.py eee_01 --name p3_ntu_cube_raw_c0_all32 --threads 32 --photo --projection cubemap --measurement raw --photo-selector all --idw-off
python3 tools/offline/run.py eee_01 --name p3_ntu_cube_raw_c1_all32 --threads 32 --photo --projection cubemap --measurement raw --photo-selector all
python3 tools/offline/run.py eee_01 --name p3_ntu_sphere_raw_c1_all32 --threads 32 --photo --projection equirectangular --measurement raw --photo-selector all
python3 tools/offline/run.py eee_01 --name p3_ntu_cube_igm_all32 --threads 32 --photo --projection cubemap --measurement igm --photo-selector all
python3 tools/offline/run.py eee_01 --name p3_ntu_cube_igm_weakest32 --threads 32 --photo --projection cubemap --measurement igm --photo-selector weakest
```

Shield4 runs, executed last:

```bash
python3 tools/offline/run.py shield4 --name p3_shield4_geometry32 --threads 32
python3 tools/offline/run.py shield4 --name p3_shield4_geometry32_repeat --threads 32
python3 tools/offline/run.py shield4 --name p3_shield4_cube_igm_all32 --threads 32 --photo --projection cubemap --measurement igm --photo-selector all
python3 tools/offline/run.py shield4 --name p3_shield4_cube_igm_weakest32 --threads 32 --photo --projection cubemap --measurement igm --photo-selector weakest
python3 tools/offline/run.py shield4 --name p3_shield4_cube_raw_c0_all32 --threads 32 --photo --projection cubemap --measurement raw --photo-selector all --idw-off
python3 tools/offline/run.py shield4 --name p3_shield4_cube_raw_c1_all32 --threads 32 --photo --projection cubemap --measurement raw --photo-selector all
python3 tools/offline/run.py shield4 --name p3_shield4_cube_raw_c1_all32_repeat --threads 32 --photo --projection cubemap --measurement raw --photo-selector all
python3 tools/offline/run.py shield4 --name p3_shield4_cube_raw_c1_weakest32 --threads 32 --photo --projection cubemap --measurement raw --photo-selector weakest
python3 tools/offline/run.py shield4 --name p3_shield4_sphere_raw_c1_all32 --threads 32 --photo --projection equirectangular --measurement raw --photo-selector all
python3 tools/offline/run.py shield4 --name p3_shield4_sphere_igm_c2_all32 --threads 32 --photo --projection equirectangular --measurement igm --photo-selector all
```

Archive compact results from the ignored runtime folders:

```bash
python3 tools/offline/archive_p3_results.py
```

The historical P2-T G1 replay and offline parity commands are recorded in [`artifacts/offline_infra/commands.md`](../offline_infra/commands.md). The P3 representation unit tests include cubemap finite-difference tests, spherical projection/residual Jacobian checks, longitude-seam sampling, IDW wrap, pole rejection, and the COIN interface/math tests.
