# CUBE-LIO design review

## Sources

- Yang Liu et al., *Cubemap-Based LiDAR-Inertial Odometry with Intensity Assistance*, ICRA 2026 program and abstract: <https://ras.papercept.net/conferences/conferences/ICRA26/program/ICRA26_ContentListWeb_4.html>.
- Yang Liu, *LiDAR intensity-assisted cubemap-based LiDAR-inertial odometry*, Robotics Symposia 2026 slides (slides 6–15): <https://www.docswell.com/s/scomup/59N8N9-2026-03-22-173102>.
- Implementation boundary and adopted choices from [`ALGORITHM_AND_PROVENANCE.md`](../p1/ALGORITHM_AND_PROVENANCE.md).

The conference abstract and public slides describe the published design. They do not disclose every rasterization, interpolation, mask, or tuning detail. The latter remain implementation choices and are not presented here as author-code facts.

## Confirmed from the paper abstract and public slides

### What Cubemap means

CUBE-LIO projects LiDAR return intensity onto six planar images corresponding to the faces of a cube. A 3D point is assigned to its dominant signed Cartesian axis, then mapped to two local face coordinates. The resulting per-scan intensity representation is used to form direct photometric constraints alongside geometric LiDAR constraints in a tightly coupled LIO update. It is a projection representation for measurements, not a six-view camera rig or a persistent global texture map.

### Why use it instead of equirectangular projection

The slides identify polar stretching and distortion in equirectangular images, as well as trigonometric projection cost and assumptions tied to rotating/ring-structured LiDAR. The cube faces avoid latitude pole singularities, distribute angular distortion across six charts, and use dominant-axis selection with arithmetic. The ICRA abstract confirms the intended benefits: no pole singularities, lower overall polar distortion, and avoiding trigonometric mapping cost. Cubemap charts introduce face boundaries; the public design mentions face-crossing treatment, while this repository's P1 implementation conservatively rejects seam samples.

### Role of IDW

The slides show intensity images before and after inverse-distance-weighted interpolation and motivate it for low-density, non-repetitive scans. It supplies nearby pixel values where a sparse scan left holes, making image filters and local residual sampling possible. IDW is a raster completion step: it does not add a LiDAR return or independent geometric evidence. Its validity depends on not averaging across depth layers. The depth-gated, non-recursive IDW used in this repository is an engineering policy; its exact radius, support count, and thresholds are not specified in the public slides.

### Role of IGM

IGM is the intensity-gradient magnitude, computed from spatial derivatives of a smoothed intensity image. The slides propose selecting semi-dense high-response pixels and minimizing a direct residual between the current IGM sample and a stored reference IGM value. The stated motivation is to suppress low-frequency intensity changes associated with range/incidence effects while retaining local edges and texture. Inference from the formula: gradient magnitude is invariant to an additive intensity offset and attenuates sufficiently smooth spatial trends; it is not invariant to multiplicative gain, arbitrary reflectance changes, or occlusion. IGM is a measurement channel and feature signal, not a projection method by itself.

### Does CUBE require geometry-degeneracy detection?

The published presentation describes semi-dense selection by IGM response and joint geometry/photometric optimization; neither the abstract nor the slides make a geometry weak-direction detector a prerequisite for forming or selecting photometric constraints. Thus the representation can be used without such a detector. This does not imply that every photometric feature constrains every degenerate pose axis, or that the geometric degeneracy gate in this repository can be removed without an ablation.

## Repository-specific interpretation

The local P1 code is an independent conservative implementation, not the authors' released implementation. Its fixed landmark lifetime, depth checks, seam rejection, robust scale, and IDW thresholds are local choices. The P3 tests therefore answer controlled questions about this implementation under the frozen Super-LIO update; they cannot establish exact reproduction of unpublished CUBE-LIO source or settings.

The intended separation is:

- **Projection:** equirectangular or cubemap coordinates and their Jacobians.
- **Raster completion:** raw samples only, or depth-gated IDW.
- **Measurement channel:** raw intensity or IGM.
- **Feature selector:** Super-native weakest-translation direction or all-use response ranking.

Changing one axis at a time is necessary to distinguish projection gains from the IGM channel and selector behavior.
