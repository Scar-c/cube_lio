# CUBE-LIO / Super-LIO P3-A Prompt
## TunnelD Cubemap Validation Before Livox Generalization

## Mission

Do NOT continue Shield4 optimization.

Before making any claim about Livox/non-repetitive LiDAR generalization, establish that Cubemap itself provides a valid improvement on a controlled Ouster degeneracy case.

Primary goal:

> Demonstrate that CUBE-style intensity representation can achieve reliable
> TunnelD localization, preferably within 10 m ATE.

This phase is about validating representation, not solving Livox.

---

# Workspace

Repository:

```
/home/lc/cube_lio
```

Start from latest P3 branch after:

- offline 32T infrastructure
- P3 Cubemap implementation
- P3-R audit

Use:

```
offline 32T
```

for every experiment.

Do not use online ROS replay.

---

# Frozen components

Keep unchanged:

- Super-LIO estimator
- ESKF
- geometry map
- IMU propagation
- Super-native degeneracy gate
- evaluator
- offline runner

Only change:

```
IntensityRepresentation
```

and related representation-specific code.

Do NOT:

- tune photo_scale
- tune R
- increase feature count
- change EKF weighting
- modify geometry residual

---

# Research question

Answer:

> Compared with COIN raw intensity projection, does CUBE-style intensity
> representation improve localization under geometric degeneracy?

TunnelD is the primary benchmark because:

- Ouster timing is controllable
- intensity observation is continuous
- geometry degeneracy is strong
- Livox FOV/time issues are removed

---

# Dataset

Primary:

```
/home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag
```

GT:

same as previous experiments.

---

# Phase 0
## Reproduce current baseline

Run:

```
Super + COIN + Super-native degeneracy gate
```

Record:

- ATE
- trajectory SHA
- runtime
- diagnostics

Artifact:

```
artifacts/p3a/baseline_coin.json
```

---

# Phase 1
## Representation ablation

Implement/run:

## R0

COIN raw projection

Reference:

current 3.5m baseline.

---

## R1

Cubemap only

No IDW.

Purpose:

measure projection effect.

---

## R2

Cubemap + IDW

Purpose:

measure interpolation effect.

---

## R3

Cubemap + IDW + IGM

Purpose:

full CUBE-style representation.

---

For every representation:

Keep:

- same selector
- same feature limit
- same fusion

Record:

- ATE
- trajectory SHA
- runtime
- feature count
- residual statistics

---

# Phase 2
## Selector dependence test

For the best representation, compare:

A:

Super-native weakest-direction selector

B:

all-use / no weak-direction dependency

Purpose:

Answer:

> Does Cubemap reduce dependence on geometry degeneracy detection?

Do not tune.

---

# Phase 3
## Determinism

For every candidate with:

ATE < 10m

run:

3 times.

Require:

- trajectory SHA identical
- metrics identical

---

# Acceptance criteria

Primary:

PASS:

```
Cubemap representation achieves ATE < 10m on TunnelD
```

Strong PASS:

```
Cubemap + IGM <= COIN baseline
and/or
Cubemap works with all-use selector
```

Partial:

```
Cubemap works but requires weakest-direction selector
```

NO-GO:

```
Cubemap cannot reach 10m
```

---

# Required analysis

Create:

```
spec/p3a/P3A_TUNNELD_CUBEMAP_VALIDATION_REPORT.md
```

Answer:

1. Does Cubemap improve over COIN on TunnelD?
2. Does IDW matter?
3. Does IGM matter?
4. Does Cubemap reduce dependence on weakest-direction selection?
5. Is the representation ready for Livox validation?

---

# Artifacts

Create:

```
artifacts/p3a/
baseline_coin.json
cubemap_results.json
selector_ablation.json
determinism.json
commands.md
```

---

# Git

Create branch:

```
p3a-tunneld-cubemap-validation
```

At completion:

- commit
- push
- verify SHA
- clean worktree
- STOP

Do not return to Shield4 until TunnelD Cubemap validation is complete.
