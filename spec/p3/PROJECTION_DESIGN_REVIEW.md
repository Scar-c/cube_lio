# Projection and intensity-representation design review

| Representation | Mapping and strengths | Main limits | Role in this experiment |
|---|---|---|---|
| Spherical / equirectangular | Maps azimuth and elevation to a rectangular image. It is simple to inspect and conventional image operators apply directly. | Longitude wraps at a seam; the poles collapse to singularities and high-latitude pixels have large, nonuniform angular footprints. A generic implementation needs trigonometric functions and cannot assume a rotating sensor's beam lookup table. | Generic projection control. Use the same raster size policy, depth-aware filling, measurement channel, feature cap, residual, and selector as the cubemap arm. |
| Generic projection | An interface boundary for a point-to-chart mapping, local image sampling, gradient, residual, and validity rules. It does not prescribe the chart or sensor calibration. | An abstraction alone has no accuracy benefit. If masks, interpolation, resolution, or residual channels vary with the implementation, a comparison no longer isolates projection. | Keep estimator and feature lifecycle outside the representation implementation; switch only the representation object. |
| Cubemap | Assigns a point to one of six cube faces by its dominant signed axis and uses two local coordinates. It avoids spherical poles and spreads distortion among charts; the public CUBE-LIO slides emphasize low-cost arithmetic and compatibility with non-repetitive scans. | Face seams need an explicit policy; pixel footprints still vary over a face; sparse holes need careful completion. | Compare cubemap raw intensity with and without IDW, then enable IGM while holding the rest fixed. |
| Voxel/hash intensity field | Stores reflectance values in spatial cells or a neighborhood structure. It avoids a global angular chart and can sample in 3D. | Requires a spatial interpolation kernel, neighborhood search, memory policy, and map-update design. Sparse and moving returns make support/visibility decisions important; these choices would change more than the requested projection representation. | Out of scope for P3; document as a distinct representation family, not a control to add mid-study. |

## Fair-comparison contract

The equirectangular-vs-cubemap comparison must share the same input points and deskew, depth validation, target measurement channel, residual and Jacobian convention, feature lifetime/cap, and selector. IDW must be either enabled for both or disabled for both. A raw-intensity comparison isolates projection more cleanly; an IGM comparison tests projection under the CUBE measurement channel. Results should report valid residual coverage as well as ATE because the charts can reject different fractions of measurements.

The COIN arm is retained as its existing Ouster-specific raw-intensity feature pipeline. It is a useful production reference but differs in calibrated projection, preprocessing, patch/NCC lifecycle, and selector, so COIN-vs-Cubemap is a system comparison rather than a projection-only comparison. Its residual and estimator fusion remain frozen.

## Facts and inferences

**Confirmed by the CUBE-LIO ICRA abstract and public slides:** six cube-face mapping; the contrast with equirectangular pole distortion and trigonometric operations; semi-dense selection from IGM; direct IGM residual optimization; IDW displayed for sparse scans; tightly coupled geometric and photometric constraints.

**Engineering inference:** the control matrix above can attribute differences to projection/channel only if the shared-comparison contract is enforced. IDW may improve support but can create biased samples at discontinuities; rejection rate and depth-consistency checks are therefore part of the evidence, not just implementation details. A voxel/hash map is not a projection substitute with matched cost unless its spatial support and memory/update rules are held fixed.
