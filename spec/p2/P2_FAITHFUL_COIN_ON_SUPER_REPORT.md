# P2 Faithful COIN-LIO on Super-LIO report

**Verdict: `P2 BLOCKED — PARITY OR DETERMINISM FAILURE`**

P2.1 frontend parity and P2.2 full-sequence feature shadow parity passed. P2.3 reproduced the pinned official COIN correction-row formula to roundoff, but the required finite-difference check of the actual TunnelD residual failed. The failure is attributable to approximations in the pinned COIN projector/image-gradient chain, not to a mismatch between the local row and the official source formula. The P2.3 hard gate blocks formal P2.4/C2 acceptance. One later user-authorized exploratory run aborted before producing a complete trajectory, so this report has no Super+COIN ATE.

## Frozen references and controls

The target is ENWIDE TunnelD, `/home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag`, SHA256 `afa448cd2ee32921cd514bb7d4c2e139f642bb164f66b1d556cf48c0c798406e`. The pinned COIN-LIO oracle is `76729cc4feb3649cbd79d28f82d9f62a2c82889b`; its checkout remained clean. The frozen GT SHA256 is `25c7a20513b3c41e7a5f517119ff41bcf07329b6d87f3aeb8f5ed7725f5c922e`, and the evaluator SHA256 is `6fb3c28d376dd073b517204c249e092b63b305700e8a005b210087250c9b76bf`.

| Run | TunnelD ATE RMSE | Frames / matched | Trajectory SHA256 | Status |
|---|---:|---:|---|---|
| Official COIN-LIO, unmodified | 0.5001613629532796 m | 1185 / 1184 | `c3fbc96f5f6abd42824229be78a9a324bf5a43b444f775d446db18da14dd3dcc` | Successful pinned oracle |
| Super-LIO geometry baseline | 113.91929554770142 m | 1179 / 1179 | `a07250898a5bd75fe247554edaaad786f5e6aa8ccf77d24767ae5068bc6d5b24` | Frozen P0/P2A control |
| Super-LIO + COIN fusion | **No complete result / ATE** | 193 partial trajectory records | `cab4c1c7a9fc4b67bdd09f1df932880693dc32b4d9c5b9ab4748ca9afd27d124` | One user-authorized exploratory run aborted; formal P2.4/C2 remains blocked |

The GT association and evaluator were not changed. The six-frame difference between the official oracle and Super geometry is retained as a known baseline difference; it was not tuned in P2.

## Stage gates

**P2.0 — PASS.** Recorded the dataset, GT, evaluator, official config and source identities and checked the existing P0/P1/P2A controls. The identity record is `artifacts/p2/reference_identity.json`.

**P2.1 — PASS.** On ten TunnelD frames distributed from the first through last scan, the local COIN Ouster frontend matched the pinned official frontend's 128×1024 images, point ownership, range, masks, projected-index map, processed intensities, display image, and image derivatives. Integer/index arrays had zero mismatches; float image and gradient arrays had zero exact mismatches; sampled UV maximum error was `1.4210854715202004e-14` pixels. This isolated current-frame frontend gate used identity distortion transforms, as recorded in `frontend_parity.json`; it does not by itself establish acquisition-time tracking parity.

**P2.2 — PASS in shadow.** The full 1185-row official TunnelD trajectory was paired to bag scans by the exact COIN `lidar_end_time`; all paired scan-end timestamps had zero delta. The comparison replayed official per-point `T_Li_Lk` and `vec_idx` transforms and final geometry translation rows. Feature state/order and add/remove totals matched on every frame; local weak-direction counts and orientations matched the traced official outputs. Two sequential full feature-shadow CSVs were byte-identical, SHA256 `80583c838b78c73d406e2bfdec1160cf05da9b226d63602722fe3cee3571855c`. This is a shadow implementation, not production ROS/ESKF wiring.

**P2.3 — BLOCKED.** The local source-formula correction row matches the pinned COIN expression on 1494 scalar pose-DoF samples with maximum absolute difference `1.8189894035458565e-12`. The actual fixed-correspondence residual finite difference fails: relative error P50 `0.29533596811444723`, P95 `1.1922045930114016`, and 129 sign disagreements among 1494 components (sign agreement `0.9136546184738956`). The projector's horizontal derivative passes (P95 `4.697054834437805e-10`), while its vertical derivative fails (P95 `0.22421892766728582`). The image-sampling gradient also fails (combined P95 `1.2230676357207384`).

The source audit isolated two pinned-COIN approximations. Its analytic vertical projection derivative uses one global scale from the first/last beam elevations, while its actual projector interpolates each calibrated beam interval. Its image gradient uses a one-pixel central difference of bilinear samples, which is not the local derivative of the bilinear residual. These differences remain on samples away from integer-pixel boundaries. Replacing them with exact derivatives would change the pinned official COIN correction row, so this round does not silently substitute those derivatives or waive the required FD gate. The sample policy and full values are in `artifacts/p2/jacobian_tests.json`; the input CSV SHA256 is `d8e068e451166efe73a7edce2faee7be20ddcdf4fe8de1a0aa72c2d2bd1e3e3c`.

**P2.4 formal gate and C2 acceptance — BLOCKED by P2.3.** After the original closure, the user explicitly authorized one exploratory TunnelD fusion attempt despite the failed finite-difference gate. This single diagnostic run wired COIN rows into Super's pose normal equations using the pinned `photo_scale=0.00095` and `measurement_variance=0.001`, then aborted with SIGABRT after 183 complete COIN observation scans. The node threw `COIN mode has insufficient propagated IMU history`. It had emitted 193 trajectory records, but the trajectory is incomplete; the evaluator did not run and no ATE is available. The final COIN CSV record (frame 183) was truncated by the abort. The complete run record, file hashes, source identity, and partial diagnostics are in `artifacts/p2/c2_single_run.json`.

Across the 183 complete diagnostic rows, the COIN-vs-Super scan-end delta had median `0.00416479 s` (range `0.00272843–0.00538935 s`), and the per-scan motion-history fallback count had median `9605` points (range `4216–16469`). The median was 50 valid patches and 1250 photo rows per scan. These are partial-run diagnostics only; the timing and fallback figures mean acquisition-transform parity remains unestablished. The run did not verify accumulated-normal-equation equivalence or production-vs-diagnostic scale equivalence. Do not interpret the partial trajectory, official oracle ATE, or Super geometry ATE as a fused result.

## Regression and determinism evidence

The frozen P2A photo-OFF trajectories for TunnelD, NTU `eee_01`, and Shield1 are byte-identical to their P0 references. P2A repeated TunnelD controls, including C0, are also byte-identical. P2 shadow feature output repeated twice is byte-identical as above. The required three-run production C2 determinism check remains unrun because the P2.3 gate blocks formal C2. The one authorized exploratory attempt is not a determinism check. The existing P1/P2A math and information-budget tests and the COIN feature-math tests pass; the real TunnelD P2.3 diagnostic exits with status 3 to signal the failed gate, after writing its audit CSV.

## Scope limits and next action

The COIN frontend/feature implementation and ROS/ESKF observation wiring were built for this isolated diagnostic, but only a partial run was obtained. The normal-equation addition used Super's `[right-local rotation, global position]` order and the pinned scale conversion; independent equivalence, acquisition-transform parity, C1 production shadow, complete C2, and runtime/resource measurements remain unverified. These remain gated behind review of the P2.3 blocker; no parameter sweep or GT tuning was performed.

P2 closes as blocked. Do not start projection ablation/P3 from this result. Stop for external review of whether the pinned official COIN approximations are acceptable for a source-faithful integration or whether the requested strict residual-derivative gate requires a different scoped experiment.
