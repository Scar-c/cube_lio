# Super geometry signal audit

The Super trace exports the exact final-iteration translation rows passed to `CoinObservation::finish`, after the same `compute_error` acceptance checks that feed the geometry update. Each row is paired with its final point-to-plane residual. It also records the number of downsampled effective points. This audit does not change Super's geometry, map, correspondences, or ESKF.

Across 1180 corrected-timing G1 frames, the median is **971 accepted rows** from **1166 downsampled points**. Translation-row norms are 1.0. The median eigenvalue ratios are `λ1/λ2=0.152`, `λ1/λ3=0.138`, and `λ2/λ3=0.853`; the weakest-axis contribution has median 56.64 absolute and 0.0580 per row. Median point-to-plane residual RMS is 0.0316 m. Normal directions occupy a median 50 (effective entropy 49.3) deterministic 10° bins, with entropy 3.90 nats.

The official trace contains only the final accepted `h_geo_last` rows. It does not carry pre-gate neighbor/correspondence counts or residual values, so candidate rejection counts and official residual quality cannot be reconstructed from that binary trace. The accepted geometry row count is the comparable effective-row count.

Official and Super frames are paired by nearest corrected LiDAR-end timestamp; 1177 Super frames have a pair within 50 ms. The CSV preserves the actual timestamp delta and per-frame metrics. It does not enforce row, eigenvector, or trajectory equality between different estimators.

The Super signal is anisotropic, but less strongly concentrated in its weakest eigenaxis than the official COIN oracle. Always passing that weakest Super eigenvector to COIN changes feature selection and produces the strongest controlled production result in this round. This is evidence that the feature-selection interface matters; it does not prove Super's geometry is physically identical to official COIN geometry.
