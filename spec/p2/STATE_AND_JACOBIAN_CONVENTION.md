# COIN / Super-LIO state and Jacobian convention

This note fixes the transform, residual, sign, and tangent-order conventions that the P2 photometric implementation must use. It documents the source-derived convention; it does not claim a production residual or fusion parity gate has passed.

## Frames and transforms

Use `T_AB` to mean a transform that maps coordinates in frame B into frame A. Official COIN's pose state `T_GI` maps IMU coordinates into global coordinates. Its fixed `T_IL` maps LiDAR coordinates into IMU coordinates, so the LiDAR pose is `T_GL = T_GI * T_IL`. A COIN patch stores fixed points `p_G` in global coordinates. `T_LG = inverse(T_GL)` maps them to the predicted end-of-scan LiDAR frame `L_k`.

The Ouster image belongs to the original scan acquisition times. COIN projects `p_Lk` into a rounded end-frame bucket, selects the closest corrected scan point in that bucket (or its same-column fallback bucket), and uses that point's transform index to retrieve `T_Li_Lk`. It then computes `p_Li = T_Li_Lk * p_Lk` and projects `p_Li` into the dense image. The frame therefore needs the corrected cloud, the per-point `vec_idx`, and the transform vector; deskewing the cloud alone does not preserve this reverse projection path.

The Super-LIO ESKF pose is also the IMU body pose. Its six pose increments are stored as `[δθ_right, δp_global]`: `ESKF::Update()` applies `R ← R·Exp(δθ)` and `p ← p + δp` in `src/super_lio/src/lio/ESKF.cpp:112-119`. Official COIN's manifold state declares position before rotation, so its first six columns are `[δp_global, δθ_right]` (`refs/COIN-LIO/include/use_ikfom.h:12-21`; right multiplication is in `mtk/types/SOn.hpp:231-239`).

## Residual and correction Jacobian

For each fixed patch point, official COIN uses the float-image residual

```text
r = I_current(u,v) - I_reference
```

and obtains `(u,v)` by bilinear sampling. The image gradient uses bilinear samples at `u±1` and `v±1`, then central differences. Projection uses the calibrated Ouster `du/dp` at `p_Li`.

The official `h_x` is a correction-direction Jacobian, not `∂r/∂δ`. In official `[δp, δθ]` order, the source's point-to-pose block is

```text
dp_dcorrection = [ R_Li_I · R_IG,  -R_Li_I · hat(p_I) ]
H_coin          = [dI/du, dI/dv] · [du/dp_Li] · dp_dcorrection
```

where `p_I = R_IG (p_G - p_GI)`, and `R_Li_I = R_Li_Lk · R_LI`, with `R_LI = R_ILᵀ` mapping IMU increments into LiDAR coordinates. This is the source's intended correction-direction row for additive global position and a right-local IMU rotation increment. P2.3 reproduced that source row to `1.82e-12` maximum absolute difference, but TunnelD finite differences show it is not the exact derivative of the implemented residual: source image gradients use central samples at ±1 pixel, and source `du/dp` uses one global vertical scale while `project()` interpolates calibrated beam intervals.

On 1494 fixed-correspondence scalar rows, six-DoF relative error P95 is `1.192` and 129 components disagree in sign. Away from integer-pixel boundaries, the horizontal projector derivative passes (P95 `4.70e-10`), while the vertical derivative P95 is `0.224`; the bilinear residual's local image-gradient P95 error is `1.223`. These are verified pinned-source approximations rather than a local state-order mismatch. Replacing them with exact bilinear and per-beam derivatives would change official COIN's `H`, so P2 stops before fusion instead of claiming a passed numerical Jacobian gate.

To express a COIN row in Super's six-dimensional pose order, apply the explicit block permutation

```text
S = [ 0  I ]
    [ I  0 ]
H_super = H_coin · S = [H_coin_rotation, H_coin_position]
```

This permutation assumes both rows refer to the same IMU pose and right-local rotation coordinates. P2.3 must check all six columns numerically, including the LiDAR/IMU extrinsic and acquisition-time transform. No positional copy of the six columns is valid.

## Normal-equation and scale contract

COIN concatenates geometry and photo rows. The ENWIDE `photo_scale=λ=0.00095` multiplies both the photo correction row and its residual before the shared `R=geo_std=0.001` update. Therefore its photo contribution has the form

```text
A_photo = (λ² / R) Σ H_superᵀ H_super
b_photo = (λ² / R) Σ H_superᵀ r
```

Super's `ESKF::UpdateObserve()` consumes preaccumulated `HTVH` and `HTVr` in the first six state slots. Its geometry observer uses `J·1000·Jᵀ` and `-J·1000·error` in `SuperLIO::Observe()` (`src/super_lio/src/lio/super_lio.cpp:454-512`). Before connecting photo terms, verify row orientation and RHS sign against both this observer and an explicit stacked `H/r` calculation. The value `0.00095` applies only to the official raw-intensity domain; it must not be applied to P1 CUBE/IGM residuals.

Current status: source convention audited; COIN projector/image parity passes; 1185-frame feature lifecycle shadow passes with official per-point motion transforms and geometry-derived weak directions. The local weak-direction rule matches official traced geometry rows and output directions frame by frame; two full shadow CSVs are byte-identical. Source-formula Jacobian parity passes, but the required residual finite-difference gate is blocked by the pinned COIN approximations described above. Production acquisition-transform wiring, photo scaling equivalence, and ESKF fusion were not attempted.
