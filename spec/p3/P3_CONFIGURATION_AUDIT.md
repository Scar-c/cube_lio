# P3 configuration audit

All values below are read from the complete f306 source snapshot in [`source_snapshot/f3060768/`](source_snapshot/README.md). The offline runner’s command ledger is [`artifacts/p3/commands.md`](../../artifacts/p3/commands.md). No run was launched to produce this document.

## Photo/Cubemap parameters

| ROS parameter / CLI control | f306 default or Shield4 value | Meaning in source |
|---|---:|---|
| `/photo/enable` / `--photo` | YAML `false`; CLI default off | The runner sets the live ROS parameter from the `--photo` flag. A P3 photo run must pass `--photo`; loading `photo.yaml` alone does not enable updates. |
| `/photo/projection` / `--projection` | `cubemap` | Choices are `cubemap` and `equirectangular`; runner default is `cubemap`. |
| `/photo/measurement` / `--measurement` | `igm` | Choices are `igm` and `raw`; runner default is `igm`. `igm` selects the intensity-gradient-magnitude channel. |
| `/photo/selector` / `--photo-selector` | `all` | Choices are `all` and `weakest`; runner default is `all`. This is the all-use/weakest-direction switch. |
| `/photo/weakest_gate_threshold` / `--photo-gate-threshold` | `0.31913064578672057` | Frozen confidence threshold used by the weakest-direction selector. |
| `/photo/weight` | `1.0` | Scale applied to robust photometric normal-equation terms. |
| `/photo/max_features` | `1200` | Maximum active selected features, not a guaranteed selected count. |
| `/photo/max_lifetime` | `10` frames | Feature expiration interval. |
| `/photo/high_response` | `5.0` | Minimum response before a candidate can be ranked. For IGM mode response is sampled IGM value; for raw mode it is sampled image-gradient magnitude. |
| `/photo/suppression_radius` | `2` pixels | Local feature suppression radius. |
| `/photo/normalization_frames` | `20` | Startup frame count used by robust residual-scale estimation. |
| `/photo/sigma_min` | `1.0` | Lower bound on robust residual scale. |
| `/photo/robust_gate` | `4.685` | Residual outlier gate in units of estimated sigma. |
| `/photo/huber_delta` | `1.345` | Huber transition in normalized-residual units. |
| `/cubemap/resolution` | `96` | Width and height of each of the six cube faces; 96×96 per face. |
| `/cubemap/idw_enable` / `--idw-off` | YAML `true`; runner default enabled | `--idw-off` overrides the live ROS parameter to `false`. |
| `/cubemap/idw_radius` | `3` pixels | Circular neighborhood radius; code checks squared pixel distance ≤ 9. |
| `/cubemap/idw_k` | `6` | Maximum nearest neighbors used in a fill. |
| `/cubemap/idw_min_support` | `3` | Minimum depth-consistent support count. |
| `/cubemap/idw_power` | `2.0` | Weight is `1 / distance^power`. |
| `/cubemap/range_absolute` | `0.3 m` | Absolute depth consistency tolerance. |
| `/cubemap/range_relative` | `0.02` | Relative depth consistency tolerance, scaled by the anchor range. |
| `/cubemap/gaussian_sigma` | `1.0` | Sigma used for the 3×3 smoothing stage before IGM derivatives. |

`photo.yaml` contains conservative defaults and sets `enable: false`. The runner in `tools/offline/run.py` independently exposes `--photo`, `--projection`, `--measurement`, `--photo-selector`, `--photo-gate-threshold`, and `--idw-off`, and explicitly sets the corresponding ROS parameters. For the commands recorded in `artifacts/p3/commands.md`, the Shield4 `cube_igm_all` command uses Cubemap + IGM + all selector + IDW enabled; `cube_igm_weakest` changes only the selector; `cube_raw_c1_all` selects raw intensity with IDW enabled; and `cube_raw_c0_all` disables IDW. These statements describe recorded command arguments and configuration precedence, not newly repeated executions.

## Shield4 LiDAR / IMU parameters

`tools/cube_lio/config/shield4.yaml` specifies:

| Parameter | Value | Review relevance |
|---|---:|---|
| `lio.ros.lidar_topic` | `/livox/lidar` | Livox CustomMsg input. |
| `lio.ros.imu_topic` | `/imu/data` | IMU stream selected by this run config. |
| `lio.sensor.lidar_type` | `1` | `LID_TYPE::LIVOX` in `src/super_lio/include/common/ds.h`. Ouster is `7`. |
| `lio.sensor.blind` | `2.0` | Near-range rejection in meters. |
| `lio.sensor.maxrange` | `150.0` | Far-range rejection in meters. |
| `lio.sensor.filter_rate` | `3.0` | Livox geometry handler uses a stride of three input points. The dense photo side channel iterates all input points. |
| `lio.sensor.enable_downsample` | `true` | Enables geometry-side downsampling path. |
| `lio.sensor.voxel_fliter_size` | `0.5` | Geometry voxel-filter size in meters. |
| `lio.sensor.gravity_norm` | `9.7946` | Configured gravity magnitude. |
| `lio.sensor.imu_type` | `0` | IMU model selector. |
| `lio.sensor.imu_na`, `imu_ng` | `0.1`, `0.1` | Accelerometer and gyroscope noise values. |
| `lio.sensor.imu_nba`, `imu_nbg` | `0.0001`, `0.0001` | Accelerometer and gyroscope bias-noise values. |
| `lio.extrinsic.lidar_imu` | `[0.049258, -0.0125, 0.026946, 0.99962, -0.027517, -0.001746, 0.027463, 0.999299, -0.025456, 0.002445, 0.025398, 0.999674]` | Translation and rotation values consumed as the configured LiDAR/IMU extrinsic. See the full YAML snapshot for ordering and surrounding loader context. |

## Sensor-selection branches visible at f306

- `LID_TYPE` assigns `LIVOX = 1` and `OUSTER = 7`.
- `ROSWrapper` dispatches `LIVOX` messages to `livoxHandler`; Ouster and other point-cloud inputs use the `PointCloud2` handler with type-specific branches.
- `ROSWrapper::sync_measure` has an Ouster-specific IMU-window routine; other sensors, including Livox, use the historical non-Ouster branch.
- `SuperLIO::Propagation_Undistort` uses Ouster history retention only when `g_lidar_type == OUSTER`; Livox follows the non-Ouster history and deskew path.
- `PhotoObservation::prepare` accepts the dense intensity side channel generically. It interpolates motion only when a point timestamp falls within the supplied propagated-state history; otherwise the initialized point coordinate remains uncorrected before conversion back to sensor-centric coordinates.

The `ROSWrapper` source and the non-Ouster deskew code are part of the exported f306 tree, but are not changed by commit f306 itself. The relevant source-level timing path and its evidentiary limit are described in [`P3_CODE_AUDIT_MAP.md`](P3_CODE_AUDIT_MAP.md).
