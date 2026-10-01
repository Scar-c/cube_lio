# Shield4 Livox adaptation audit

## Frozen input identity

- Bag: `/home/lc/algorithm_versa/bag/GEODE/Shield_tunnel4_gamma.bag`
- Bag SHA-256: `d628847b5db9f304d24415b48edafaa16bc31c52aa8b8d56e0815afdc50d7377`
- Position-only GT: `/home/lc/algorithm_versa/bag/GEODE/Shield_tunnel4.txt`
- GT SHA-256: `89185a4472c3d9407e6619ab5f5f2f9099fb3e3dff83265c1d8e29e4166dbd47`
- Bag duration: 340.45 s; 113,237 messages total.
- Topics: `/livox/lidar` 3,404 `livox_ros_driver/CustomMsg`; `/imu/data` 34,041 `sensor_msgs/Imu`; `/livox/imu` 68,984 IMU messages; stereo compressed images 3,404 per camera.

## Sensor and scan-pattern findings

The supplied `gamma_config.yaml` labels the LiDAR as Livox and declares `VERT_RES: 6`; the bag uses `CustomMsg`, whose points carry Cartesian XYZ, `reflectivity`, `tag`, and per-point `offset_time`. This is the solid-state Livox Avia path used in the repository's existing Shield1 configuration. It is not a spinning Ouster range image and the message does not provide a calibrated ring/column image index. A generic angular raster is therefore the relevant projection test; treating message order as a fixed scanline would be unjustified.

The first LiDAR message has 24,000 points. Its offsets range from 0 to 99,839,365 ns and are nondecreasing. Reflectivity spans 0–255; tag classes are `0x00` and `0x10`, both accepted by the existing parser. These are first-message measurements, not claims about full-bag extrema. Existing `ROSWrapper::livoxHandler` converts offsets from nanoseconds to seconds, uses the message header stamp as scan start, and applies the existing tag/range gate. Geometry keeps every third point under the frozen `filter_rate=3`; the dense intensity side channel keeps every valid point. The LiDAR end time follows the last retained geometry point, while the dense channel preserves individual offsets; timing diagnostics must therefore remain enabled when available.

The configured external IMU is `/imu/data`. `/livox/imu` is present but is not the IMU selected by the frozen gamma configuration. The camera topics are not consumed by this LiDAR-only P3 experiment.

## Projection assumptions and risks

- Equirectangular mapping is computed from XYZ azimuth/elevation and makes no rotating-beam assumption. It has a longitude seam and severe polar sampling distortion.
- Cubemap mapping is also computed from XYZ and supports this non-repetitive pattern. Face boundaries can discard measurements unless cross-face sampling is implemented; the current conservative path rejects seam-crossing residuals.
- Both rasters contain angular holes because the scan is sparse and non-repetitive. The same depth-gated IDW policy must be used in a projection-only comparison. Interpolated pixels are not raw returns and must not bridge foreground/background surfaces.
- The existing dense-side-channel deskew uses the incoming per-point offset and interpolated propagated states. Frames without adequate history contribute no photo term; geometry frames and output timestamps remain controlled by the unchanged Super-LIO path.
- The frozen Livox-to-IMU transform is taken from the supplied GEODE gamma calibration and the prior Shield sequence configuration. The evaluation reuses the project’s recorded gamma-to-Leica lever-arm conversion; this assumption is kept fixed and no calibration/time-offset fit is allowed.

## Acceptance reference

`Shield_tunnel4.txt` has 911 position rows spanning 328.42 s. Its sampled path length, computed as the sum of consecutive 3D position distances, is 501.4395 m. The prompt's 20% limit is therefore **ATE RMSE < 100.2879 m**. The GT is position-only; evaluate with the existing no-scale SE(3) alignment and evo nearest timestamp association (`max_diff=0.1 s`, zero offset), after applying the fixed gamma-to-Leica sensor lever arm to estimated poses. RMSE above the threshold is a failed Shield4 acceptance regardless of relative improvement over the geometry baseline.

## Runtime status

The input audit is complete. The geometry-only 32T trajectory diverges after frame 2047 and reaches a 94.37 m single-frame translation step; an exact repeat has the same ATE and trajectory SHA. The fixed Livox extrinsic and `/imu/data` selection match `gamma_config.yaml`; no calibration or time offset was fitted.

Among controlled P3 runs, cubemap + raw intensity + depth-gated IDW + all-use produces 90.3942 m ATE against the strict 100.2879 m limit, with a byte-identical repeat. It is the only Shield4 configuration below the prompt threshold. The evaluator matched 898 timestamp pairs; the bag's final 13.4 seconds extend beyond the GT. Full results, input/config hashes, and trajectory SHAs are in [`shield4_results.json`](../../artifacts/p3/shield4_results.json).
