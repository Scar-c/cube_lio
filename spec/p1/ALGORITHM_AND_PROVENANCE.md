# Implemented P1 algorithm and provenance

Authoritative algorithm source is the [CUBE-LIO public slides](https://www.docswell.com/s/scomup/59N8N9-2026-03-22-173102).
Slides 10, 13 and 15 specify cubemap projection/IDW, Gaussian-derivative IGM with
high-response features, and an IGM residual with a projection/transform Jacobian
chain. They do not specify the numerical settings or cache policy used here.
No claim is made about unpublished author code or exact parameter parity.

Super-LIO GPLv3 base is official ros1 `60b57aaac8dc397f80c56364e7ccb008c300cc29`;
ros2 `f89f48dc7aea6cfa262f18e4d03b319e04e0dbd2` was inspected. The Noetic host
justifies ros1. P0 tip is `1b0698efe63fae09f9d498a285c3feb5b1b69cbd`.
The P1 module is original code under GPLv3; upstream headers/notices and LICENSE
are retained. No COIN-LIO C++ source is copied or directly adapted.
COIN-LIO main `76729cc4feb3649cbd79d28f82d9f62a2c82889b` was read at
`FeatureManager::updateFeatures`, `trackFeatures`, `h_share_model_photometric`,
`h_share_combined`, and `Projector::projectionJacobian`. Its relevant photometric
files identify Patrick Pfreundschuh (2024), BSD-3-Clause. This is acknowledged
as design inspiration, with no copied BSD code requiring additional notices.
No localization_zoo prototype was used.

The only SuperLIO integration points are frontend construction in init,
prepare before Observe, addition of photo terms after geometry accumulation,
and feature replenishment after UpdateObserve returns. ESKF.cpp/.h, OctVoxMap,
HKNN, geometry residuals and weights, DownSample, UpdateMap, and upstream
Propagation_Undistort remain unchanged. Existing geometry input sampling stays
unchanged. A separate `pc_intensity` retains all range/tag-valid input points,
with raw Ouster intensity or Livox reflectivity and original point time.
Extra dense deskew follows the upstream interpolation design, independently
returning its IMU-end coordinates to the LiDAR-end frame. Geometry continues
using upstream's own deskewed/downsampled scan.

Cubemap convention (**our conservative implementation choice**): choose the
dominant absolute coordinate; ties prefer X, then Y, then Z. Faces are ordered
+X,-X,+Y,-Y,+Z,-Z. Face basis vectors are:

| Face | d (normal) | a (image u) | b (image v) |
|---|---|---|---|
| +X | (1,0,0) | (0,1,0) | (0,0,1) |
| -X | (-1,0,0) | (0,-1,0) | (0,0,1) |
| +Y | (0,1,0) | (-1,0,0) | (0,0,1) |
| -Y | (0,-1,0) | (1,0,0) | (0,0,1) |
| +Z | (0,0,1) | (1,0,0) | (0,1,0) |
| -Z | (0,0,-1) | (1,0,0) | (0,-1,0) |

For f=(N-1)/2, depth z=d^T p>0:
u=f(1+a^T p/z), v=f(1+b^T p/z).
The projection rows are f(a/z-(a^T p)d/z²)^T and
f(b/z-(b^T p)d/z²)^T. No atan2/asin or beam lookup occurs in the photo path.
Zero/NaN points are invalid. Exact ties and near ties within 1e-8 relative to
the dominant coordinate are explicitly rejected for residual sampling;
one-sided face changes are tested. Bilinear support must stay at least two
pixels inside a face. No interpolation crosses cube seams in V0. The slides
mention crossing faces, but do not supply a complete public implementation;
conservative seam rejection is a known coverage limitation.

Rasterization (**our conservative implementation choice**): nearest integer
pixel with nearest-return z-buffer, deterministic first-input tie. Keep
intensity, Euclidean range, raw mask, filled mask, raw point index, IGM mask,
IGM and its gradients separately. Six current faces only; no global image map.

IDW (**our conservative implementation choice**): for a missing pixel, gather
raw measurements within a circular radius 3, sort by squared pixel distance,
take up to k=6 and require at least 3. Set weights to distance^(-power), power=2.
Intensity and range are weighted averages. The nearest support range anchors
the depth layer. If any chosen support differs by more than 0.3 m+0.02*range,
reject the entire fill. Never recursively fill from filled pixels. All these
values, the threshold, and the enable switch are YAML parameters. Faces and
pixel rows use TBB; the frontend does not smear foreground/background surfaces.

IGM (**our conservative implementation choice** consistent with slide 13):
3×3 separable Gaussian, sigma=1 pixel, followed by centered [-1/2,0,1/2]
derivatives on u and v. This is the derivative of the discrete Gaussian-smoothed
image. M=sqrt(I_u²+I_v²). Gaussian and derivative supports must have valid,
range-consistent measurements, so zero-filled holes never act as intensity
edges. Store diagnostic centered gradients of M, and differentiate the exact
bilinear interpolant of M for the residual Jacobian. The four bilinear ranges
also must be consistent. Final residual uses M, never raw intensity.

Selection (**our conservative implementation choice**): every visible raw-point
candidate whose subpixel M>=5 is eligible. Sort high responses deterministically,
suppress a square radius 2 around retained or newly born features, stop at
1200. This is bounded high-response selection, not COIN complementary ranking.
This numerical threshold/suppression may be restrictive for sparse OS1-16 data;
coverage and actual counts are reported, without GT-based adjustment.

Lifecycle (**COIN-derived engineering choice**): store world-frame 3D landmarks,
fixed birth IGM, birth time/frame/face; age is current frame minus birth frame.
Old landmarks project into each current image, with mask/FOV/range and robust
appearance gates. Expire at age 10, cap at 1200, and replenish only after state
update. Current-frame births never participate in that frame's observation.
Unlike COIN's raw-intensity patches, this scalar IGM observation uses a robust
appearance gate rather than NCC. The absence of patch NCC is an explicit V0
choice. World coordinates and bounded lifetimes avoid a trajectory-length map.

Transform/sign (**derived from the retained Super-LIO state**, not copied from
COIN): R is T_GB rotation, t its global translation, T_BL=(R_BL,t_BL).
p_B=R^T(p_G-t), p_L=R_BL^T(p_B-t_BL).
r=M(Pi_cube(p_L))-m_ref, with fixed image during each iterated update.
Super-LIO applies R_new=R*Exp(delta_theta) and t_new=t+delta_t. Therefore:

dp_L/dxi = [R_BL^T [p_B]_x, -R_BL^T R^T], xi=(theta_x,theta_y,theta_z,t_x,t_y,t_z).

J=dM/du * dPi/dp_L * dp_L/dxi. Birth world point is
p_G=R*(R_BL*p_L+t_BL)+t after the completed update.
Use Ap=sum(w J^T J), bp=-sum(w J^T r). The negative sign matches upstream
geometry's `HTVr -= J*1000*error`. Return Ag+Ap and bg+bp to the unchanged
ESKF::UpdateObserve. TBB thread-local fixed-size 6×6/6 accumulators are reduced;
no stacked H is allocated. Double photo accumulation casts to upstream float
at the callback boundary, matching Super-LIO's geometry accumulation style.

Noise/robustness (**our conservative implementation choice**): collect valid
historical residuals at the propagated pose in the first 20 frames, then freeze
sigma=max(1,1.4826*median(abs(r-median(r)))). Require at least 30 samples; if
unavailable, keep photo information disabled and bound stored samples until
the minimum is reached. During startup photo terms remain zero. Set W to
photo.weight/sigma² times a Huber factor min(1,1.345/abs(r/sigma)). Reject
abs(r)>4.685*sigma. `photo.weight=1` is fixed across sensors and runs; neither
the COIN raw-intensity scale nor ground-truth ATE influences calibration.

IMU gaps (**our conservative implementation choice**): if the production
sync/propagation gives fewer than two usable history states, keep the geometry
scan update and output, set Ap/bp to zero and do not replenish. Log
`deskew_supported=0`. Dense points outside a usable interpolation interval
retain their raw sensor coordinates, matching upstream's static fallback
concept; no unsupported extrapolation is invented. This can reduce accuracy
during fast motion and is documented as a limitation.

Diagnostics: each frame records active before/after, valid residual count,
residual mean/RMS/median, FOV/mask/range/outlier rejection counts, frozen sigma,
trace(Ag/Ap), all six geometry pose eigenvalues, the weakest translational
eigenvalue/vector of Ag_tt and its v^T Ap_tt v contribution. This vector is
global because translation perturbations are global. It is a block-level
diagnostic, not a marginalized Schur-complement observability proof. Timings
separate dense deskew, rasterization, IDW, IGM, photo Jacobians, iterative
update and replenishment. Coverage counters expose masked pixels. No GT enters
feature selection, noise estimation or these information calculations.

Finite differences exercise the actual implementation: 6000 random projected
points across all faces (step 1e-6, tolerance 2e-7); 1120 complete residual
points across all faces and 6 pose axes (step 1e-6, tolerance 2e-6), with nonzero
pose/extrinsics and actual synthesized intensity→Gaussian→IGM→bilinear sampling.
Pixel interpolation kinks and face seams are tested separately. Maximum
absolute errors are 8.8947e-9 and 3.7852e-9 respectively. Invalid masks, same-depth
IDW fill, cross-depth rejection and information-form descent sign also pass.
