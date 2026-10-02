# Shield4 attribution audit command ledger

Repository: `/home/lc/cube_lio`; branch: `p3r-shield4-attribution-audit`; base production commit: `f3060768abee4d2ee2343b07edf62346c7cd0f87`; base review package: `71822c76fb3b2a4d4ddad69c536862ede24388a3`.

## Evaluator audit and build

```bash
python3 tools/offline/audit_p3r_attribution.py evaluator
source /opt/ros/noetic/setup.bash
source devel/setup.bash
catkin_make --pkg super_lio --make-args cube_offline_node -j2
python3 -m py_compile tools/offline/run.py tools/offline/audit_p3r_attribution.py
git diff --check
```

The original executable and `liblio.so` are retained locally in ignored `runtime/p3r_original_build/`. The audit build log is ignored `runtime/p3r_attribution_build.log`. Only the offline executable was built; no frontend tuning or unit-test-driven estimator changes were made.

## Complete-bag comparisons

Run sequentially, with 32 TBB threads and the same Shield4 bag, YAML files and evaluator. A is also the Phase 3 instrumented run; it is not duplicated. A/C retain Cubemap + raw intensity + IDW + all-use, and the unchanged C0 information policy.

```bash
python3 tools/offline/run.py shield4 --name p3r_shield4_a_current32 --threads 32 --photo --projection cubemap --measurement raw --photo-selector all --photo-time-audit
python3 tools/offline/run.py shield4 --name p3r_shield4_b_geometry32 --threads 32
python3 tools/offline/run.py shield4 --name p3r_shield4_c_supported32 --threads 32 --photo --projection cubemap --measurement raw --photo-selector all --photo-time-audit --photo-history-supported-only
python3 tools/offline/audit_p3r_attribution.py archive
```

The runner uses local ROS services, with bag/node/evaluator logs under each ignored runtime directory. No network dataset download is performed. Each identity records command controls, input/config/source hashes, executable SHA and shared `liblio.so` SHA. Runs occur on the base Git HEAD with uncommitted audit sources; the content hashes, rather than HEAD alone, identify the actual build. Those exact sources are committed at closure.

## Feedback and hypotheses

`audit_p3r_attribution.py evaluator` was run before adding instrumentation. It independently reproduced the baseline ATE and GT path denominator. The complete A/B/C replay is the attribution feedback loop: a short prefix cannot reproduce the accumulated estimator trajectory and the final acceptance outcome. Full-bag comparison replaces bug minimization and repair because this prompt explicitly requests attribution without a fix.

Predictions stated before the new bag comparisons:

1. Unsupported photo timestamps are influential: measured fallback points occur and C changes ATE/PASS materially.
2. Dense-versus-stride sampling significantly truncates scan end: the maximum photo time exceeds geometry end by a substantial amount relative to the history-end gap. The recorded gaps distinguish this from IMU sampling/synchronization lag.
3. The observed photo benefit is insensitive to unsupported points: A/B reproduce the archived trajectory SHAs and C preserves a similar passing result.

At archive time, assertions check successful 32T runs, unchanged evaluator/GT identities, independent ATE agreement, exact A/B trajectory matches to P3, and unchanged locked source/config files. Complete per-scan CSVs are retained in this artifact directory; no environment-variable or credential dumps are archived.
