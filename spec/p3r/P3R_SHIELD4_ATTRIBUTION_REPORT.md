# P3-R Shield4 attribution report

## Result and answers

**The original numerical Shield4 PASS is verified, but exclusive Cubemap/intensity attribution is not established.** The instrumented baseline reproduces the original P3 trajectory byte for byte. It includes 3,991,596 photo points without a propagated-history bracket. Removing only those inputs changes ATE from 90.394225 m to 7037.537418 m and changes PASS to FAIL. This satisfies the prompt's **Case B and Case C** conditions: fallback inputs are a material confounder, and the outcome is highly sensitive to their inclusion.

1. **Is PASS caused by Cubemap or mixed with timing effects?** The complete Cubemap/raw/IDW/all-use system passes with the inherited timing behavior. Its PASS depends on the photo input set that includes unsupported timestamps, so it cannot be assigned solely to intensity representation. A/B show that the enabled photo path changes the outcome substantially; they do not isolate Cubemap from IDW or from temporal/input-coverage effects.
2. **Is the evaluator threshold valid?** Yes for this prompt: independent GT-length and ATE calculations reproduce `L_GT=501.439518932 m`, limit `100.287903786 m`, and ATE `90.394224597 m`. The denominator is GT, not the drifting estimate. A matched-GT denominator yields the same PASS. The final 13.447 s of estimate after GT coverage are unscored, and the 20% rule is not a common cross-dataset benchmark.
3. **How many photo points lack IMU support?** 3,991,596 of 80,732,252 accepted photo inputs, **4.944239633%**, across all 3394 processed scans. All are after the final propagated history time. C retains 76,740,656 bracketed points and has zero actual fallback points.
4. **Is Livox adaptation required before future generalization claims?** Yes: the consumed/propagated-history endpoint and dense photo acquisition interval require a future time-consistency repair and renewed controlled verification before claiming Livox/non-repetitive robustness. This audit implements no such repair and does not predict its resulting ATE.

## Fixed comparison

Base production tree: `f3060768abee4d2ee2343b07edf62346c7cd0f87`; external source package: `71822c76fb3b2a4d4ddad69c536862ede24388a3`. All three new runs used the complete Shield4 gamma bag, selected `/livox/lidar` and `/imu/data`, offline replay with a 32-thread TBB limit, the same configurations, and the frozen no-scale SE3 evaluator. Runs were sequential, with no threshold, weight, projection, IDW, selector or estimator tuning. A also serves as the instrumented Phase 3 run.

| Arm | Photo inputs / behavior | ATE RMSE (m) | `< 100.287904 m` | Trajectory SHA256 |
|---|---|---:|:---:|---|
| A: current | Cubemap + raw intensity + IDW + all-use; all accepted photo points, including inherited fallback | 90.394224597 | PASS | `aa42f52e6fce824ed65e0380da9aa2f06fdf647ee07b1b8b00eb0bd7a1f43f32` |
| B: photo disabled | Geometry only | 9739.859138552 | FAIL | `ad6ee5037d42bc6490f90e1552e9d90e6c790ddba28fd337cbb55ccbb46e6fb8` |
| C: history-supported photo only | Same photo configuration; drop inputs outside propagated history before the image build | 7037.537417924 | FAIL | `977d01fce7b4370f5e685fdc563ef0f6f3394a42a1fdfdf6d247b45d48c20318` |

Each run reads 3404 LiDAR messages and 34041 selected IMU messages and outputs 3394 poses, with 898 evaluation pairs and identical output time bounds. The 10 messages not represented in the photo audit are outside the regular initialized photo-preparation outputs; they are not counted as fallback points. All 3394 audited A/C frames have a usable positive-span history.

A and B have exact full-trajectory SHA matches to their archived P3 counterparts. Thus the rebuilt default behavior and optional counting did not perturb these outputs. The C result is one complete diagnostic run; no C repeat, parameter search, repaired timing run or P4 work was performed. A/C ATE changes by 6947.143193 m, a factor of about 77.85 in RMSE. This is a sensitivity result, not an improvement claim for B or C.

## Time-support evidence

| Count | A | C |
|---|---:|---:|
| Geometry cloud points before downstream geometry processing | 26,907,641 | 26,907,641 |
| Accepted raw photo points before C filtering | 80,732,252 | 80,732,252 |
| Photo points inside history | 76,740,656 | 76,740,656 |
| Photo points outside history, all after history end | 3,991,596 | 3,991,596 |
| Retained photo points | 80,732,252 | 76,740,656 |
| Actual fallback-deskew points retained in image inputs | 3,991,596 | 0 |

The raw count and time-support metadata are identical between A and C on every scan. C's retained/dropped counts match the inclusive history filter on every scan. A's fallback fraction is 4.94424% weighted by all photo inputs; per-scan median is 2.74754%, p90 9.64745%, p99 10.53977%, and maximum 17.02414%. Every processed A scan has fallback inputs. There are no before-history points, non-finite point times, or unusable-history frames in either run.

| Gap | Median | p99 | Maximum |
|---|---:|---:|---:|
| Max accepted photo time − geometry scan end | 0.008345 ms | 0.008345 ms | 0.158310 ms |
| Geometry scan end − propagated history end | 2.718210 ms | 10.450325 ms | 15.755653 ms |
| Max accepted photo time − propagated history end | 2.725244 ms | 10.461364 ms | 15.763998 ms |

Maximum accepted photo offsets have median 0.099836275 s and largest observed value 0.099973283 s. The geometry-stride endpoint difference is small relative to the consumed IMU history lag. Therefore, a future review must cover the non-Ouster IMU consumption/history endpoint, not just replace the geometry-derived scan end with a dense-cloud maximum. This audit changes neither mechanism.

## Attribution limits

Removing unsupported-timestamp inputs changes raster occupancy, IDW support, feature references and the estimator's subsequent trajectory. It is not equivalent to correctly motion-compensating those same inputs with adequate IMU brackets. The result proves dependence on inclusion of that time-linked input set; it does not prove that its temporal error alone caused the passing attractor, nor that deleting inputs is an appropriate production repair.

The normalization rule and its configuration are unchanged. Its data-derived final sigma is 4.378560054 in A and 4.563723647 in C; mean valid photo residual rows are 170.64349 and 91.64378 respectively. These differences are downstream consequences of the C input intervention and subsequent trajectory, not hand-set photo-scale changes. The audit cannot separate those mediating effects with the specified three arms.

The result is specifically for Cubemap **raw intensity**, not a new validation of the paper's IGM pipeline. No broad sensor-generalization or exact CUBE-paper reproduction claim follows. The evaluator arithmetic is not the explanation for the PASS; timing/input consistency remains the unresolved attribution issue.

## Source and artifact verification

The optional controls default off. Only `PhotoObservation` instrumentation/input selection and offline audit tooling are changed. All 36 locked source/config files match f306, including Cubemap/projection/IDW/IGM, representation interfaces, COIN selection/math, ROSWrapper parser, geometry/ESKF, and evaluator sources. Both executable and shared `liblio.so` hashes are recorded for the new runs; A/B trajectory equality provides the behavioral check.

The original P3 run identities record Git HEAD `12d5cdfa...`, since those experiments preceded the final P3 commit. Every recorded P3 core source-content hash was independently compared with f306 and matches it. New runs similarly record base HEAD 71822c76 plus `source_dirty=true` and precise content hashes; these identify the uncommitted audit build that is committed at closure. HEAD alone is not used as a binary/source identity.

- [`EVALUATOR_AUDIT.md`](EVALUATOR_AUDIT.md): length, association, alignment and manual calculation.
- [`LIVOX_TIME_CHAIN_AUDIT.md`](LIVOX_TIME_CHAIN_AUDIT.md): parser → history → photo residual trace and measured gaps.
- [`evaluator_analysis.json`](../../artifacts/p3r/evaluator_analysis.json): independent baseline calculation and evaluator hashes.
- [`shield4_time_diagnostics.json`](../../artifacts/p3r/shield4_time_diagnostics.json): counts/quantiles and per-scan file identities.
- Complete per-scan [`A CSV`](../../artifacts/p3r/shield4_time_a.csv) and [`C CSV`](../../artifacts/p3r/shield4_time_c.csv).
- [`shield4_ablation.json`](../../artifacts/p3r/shield4_ablation.json): all run identities, full trajectory SHAs, manual A/B/C ATE checks and resources.
- [`behavior_invariance.json`](../../artifacts/p3r/behavior_invariance.json): locked-source comparisons, historical content verification, exact A/B SHA checks and per-scan C-filter checks.
- [`commands.md`](../../artifacts/p3r/commands.md): executed build/audit/replay commands and pre-run hypotheses.

All required runs and attribution work are complete. The closure commit/push is limited to this audit; the branch is `p3r-shield4-attribution-audit`. Further timing repair or P4 work requires a subsequent task.
