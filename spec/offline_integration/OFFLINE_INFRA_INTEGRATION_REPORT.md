# Offline 32TBB infrastructure integration report

## Outcome

The offline infrastructure is ready for P3 experiments. CUBE-LIO now has one
bag replay entry point, `tools/offline/run_experiment.sh`, which defaults to 32
TBB threads and accepts `--threads 1` for parity checks. Existing
`tools/p2r/run.py` and `tools/cube_lio/run.py` paths remain compatible and
forward to the unified runner. The estimator equations, COIN residual,
degeneracy gate, and projection were not changed.

## Answers

1. **Super-LIO components reused:** the reference's transport-only rosbag
   iteration, direct sensor-message dispatch, authoritative message-header
   timestamps, synchronous estimator stepping, bounded EOF drain, and TUM
   trajectory convention. CUBE-LIO retains its existing direct replay adapter
   instead of copying Super-LIO's in-process publisher/subscriber bridge. See
   [the source audit](SUPER_LIO_OFFLINE_AUDIT.md).
2. **1T and 32T bitwise parity:** yes. On eee_01, trajectory bytes,
   timestamps, Ouster timing diagnostics, evaluator metrics, and stable run
   counts all match exactly. The trajectory SHA-256 is
   `50c981fef0e3e635786fbb86049acccdc01cdda0a0e5d9caecb076fa3fbb5f7e` in
   both runs; all parity checks are recorded as `PASS` in
   `artifacts/offline_infra/parity.json`.
3. **Speedup:** the one-sample full-bag measurement was 39.50 s at 1T and 7.87
   s at 32T, a `5.02x` speedup. CPU consumption was 39.50 CPU-seconds at 1T
   and 113.79 CPU-seconds at 32T. Against the host's 32 logical CPUs, average
   utilization was 3.13% and 45.19%, respectively. Details are in
   `artifacts/offline_infra/benchmark.json`.
4. **Ready for P3:** yes. The unified shell entry defaults to the requested
   offline 32TBB mode, and the parity validator can gate outputs before an
   experiment is accepted. The benchmark is one measurement per mode; use
   repeated runs if P3 needs a statistically stable performance comparison.

## Existing G1 reference at 32T

To check a COIN-enabled production case, the saved P2-T G1 reference
(`runtime/p2t_g1_repeat3`, TBB 4T) was replayed once through the unified 32T
offline entry using the same TunnelD bag, configs, G1 selector, and frozen
thresholds. All archived P2-T estimator-source hashes match the current
estimator sources; the offline executable hash differs because the integration
changed its scheduler default and run-metric instrumentation. The raw
trajectory SHA remains `d71c1ab0b0e3cc542aed184fa9d1e9489b8a7880598a7403a2eb8c3df847bf63`.
`coin_observation.csv`, `time_audit.csv`, `evaluation.json`, and stable run
counts also match byte-for-byte (with bag duration normalized to the legacy
six-significant-digit precision). The ATE remains `3.5462094452 m`. Detailed
evidence is in `artifacts/offline_infra/g1_cross_thread_parity.json`.

The saved G1 record is a production COIN-injection replay at 4T. It was
captured by the prior bag runner; this comparison establishes the cross-thread
reproduction of that production G1 result, while the separate P0 source audit
records the earlier online/offline 30-second comparison.

## Validation

`catkin_make --pkg super_lio --make-args cube_offline_node -j2` succeeded.
Python compilation and both compatibility entry points' `--help` checks
passed. The two full eee_01 runs processed 3,987 LiDAR messages, 153,347 IMU
messages, and wrote 3,981 poses each. ATE RMSE was `0.1174382051 m` in both
runs. Wall and CPU measurements are intentionally kept in the benchmark file;
these runtime measurements are expected to differ by thread count.
