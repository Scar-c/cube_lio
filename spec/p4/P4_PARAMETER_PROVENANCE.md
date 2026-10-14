# P4 fixed parameter provenance

Frozen before P4 math tests or accuracy evaluation. No GT/ATE tuning.
The complete paper specifies none of the numeric image/filter/gating settings
below. Published constants and equations are distinguished from local defaults.

| Parameter | Value / policy | Provenance |
|---|---|---|
| Faces and local bases | +X,-Y,-X,+Y,+Z,-Z; exact Eq. (5) | paper explicitly specified |
| Projection scale | r/2; inverse direction normalized times Euclidean range | derived from paper equation (3),(7),(8) |
| Seam W / gradient offsets | inverse/reproject out-of-face; offsets ±1, denominator 2 | paper explicitly specified (15),(16) |
| Test/initial face resolution | 64 (synthetic); 96 proposed for first production config | reused from existing repository |
| IDW radius / k / min support / power | 3 / 6 / 3 / 2 | reused from existing repository; not paper settings |
| IDW support | immutable raw raster; no recursively synthesized support | reused from existing repository |
| Range discontinuity gate | 0.3 m + 0.02 times anchor range | reused from existing repository; rejects competing depth layers |
| Range collision | nearest Euclidean range wins | reused from existing repository |
| Raster bin | floor(u), floor(v); exact +boundary allocated to last bin | engineering default for Eq. (3)'s [0,r] coordinates |
| Gaussian sigma | 1 pixel | reused from existing repository |
| Gaussian truncation | radius ceil(3 sigma) | engineering default |
| Gaussian derivative | g(k) proportional exp(-k²/(2sigma²)); d(k)=k*g(k)/sigma², normalized so sum k*d(k)=1 | engineering default discrete correlation convention; first-order Gaussian derivative, not raw central difference |
| Filter border | seam-aware W with bilinear remapping, no artificial zero padding | engineering default based on paper's continuous cross-face sampling |
| Mask handling | full filter support valid and range-consistent | engineering default; missing samples never treated as zero observations |
| IGM threshold | proposed 5 DN/pixel | reused from existing repository, not yet enabled |
| Immediate neighborhood | proposed 8-connected one-pixel expansion | engineering default interpretation of III-D |
| Production cap/lifetime/noise/robust loss | NOT FROZEN / NOT IMPLEMENTED until math gate passes | paper underspecified; cannot claim author values |
| Ouster preprocessing | pinned COIN params/line FIR/metadata at 76729cc; intensity (reflectivity=false) | reused from existing repository and recovered upstream config |
| Controls | unmodified f306 estimator, TunnelD YAML, 32 TBB threads | reused from existing repository |
| Exact projection FD | h=1e-6 m, max error <2e-7 pixels/m | reused from existing repository test standard |
| Inverse / W direction check | angular-vector error <1e-12 | engineering default numerical precision check |
| Transform FD | h=1e-6, max error <2e-8 | engineering default |
| Residual state FD acceptance | component error <=1e-4 + 1e-4*abs(FD), all tested components | engineering default declared before measurement; do not relax after observing failure |
| Residual state FD probes | h=1e-4,1e-5,1e-6,1e-7 in rotation rad / translation m | engineering default convergence check |

Eq. (16) is a finite-difference image derivative. The paper does not specify the
continuous interpolation used in Eq. (10). P4 uses bilinear sampling as a local
implementation choice and explicitly audits its consistency with Eq. (16).
The exact bilinear derivative is used only as a diagnostic reference, never as
a silent replacement for the published gradient.
