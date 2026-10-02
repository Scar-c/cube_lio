# CUBE-LIO / Super-LIO P3-R Prompt
## Shield4 Livox Attribution Audit + Photo/Geometry Time Consistency Verification

## Objective

Do NOT optimize ATE.

Do NOT modify Cubemap, IDW, IGM, selector, or estimator.

Current P3 result:

- Cubemap + raw intensity + IDW + all-use
- Shield4 ATE reported:
  90.394 m
- Current conclusion:
  observed PASS under the 20% trajectory-length criterion

However, before claiming Cubemap improves Livox/non-repetitive LiDAR robustness, audit whether the result is caused by:

1. true intensity representation improvement
2. Livox timing/deskew behavior
3. evaluator/path-length interpretation
4. geometry/photo point inconsistency

This stage is an attribution audit only.

---

# Workspace

Repository:

```
/home/lc/cube_lio
```

Current relevant commit:

```
f3060768abee4d2ee2343b07edf62346c7cd0f87
```

Audit commit:

```
71822c76fb3b2a4d4ddad69c536862ede24388a3
```

Use offline 32T runner.

Do not change estimator behavior.

---

# Phase 1
## Audit evaluator

Inspect:

```
eval/evaluate.py
```

Answer:

1. How is trajectory length calculated?
2. Is it based on:
   - estimated trajectory?
   - GT trajectory?
   - aligned trajectory?
3. How is ATE alignment performed?
4. Is the 20% threshold comparable across datasets?

Create:

```
spec/p3r/EVALUATOR_AUDIT.md
```

Include a manual calculation for Shield4.

Required artifacts:

```
artifacts/p3r/evaluator_analysis.json
```

---

# Phase 2
## Audit Livox timestamp chain

Inspect:

```
ROSWrapper.cpp
ROSWrapper.h
super_lio.cpp
photo_observation.cpp
```

Trace:

Raw Livox point:

```
offset_time
    |
    v
photo cloud
    |
    v
cubemap
    |
    v
residual
```

and:

Geometry point:

```
filter_rate sampling
    |
    v
geometry cloud
    |
    v
scan end time
    |
    v
IMU propagation
```

Answer:

1. Are geometry and photo clouds sampled from identical points?
2. If not, is the difference intentional?
3. What timestamp defines scan end?
4. Can photo points exist outside IMU history?

Create:

```
spec/p3r/LIVOX_TIME_CHAIN_AUDIT.md
```

---

# Phase 3
## Instrument only, do not fix

Add optional diagnostics.

Do not modify estimation equations.

Record per scan:

- total photo points
- photo points inside IMU history
- photo points outside IMU history
- percentage of fallback deskew points
- geometry point count
- photo point count
- scan end from geometry
- maximum photo offset time

Dataset:

Shield4 only.

Run:

```
cubemap + IDW + all-use
```

Use offline 32T.

Create:

```
artifacts/p3r/shield4_time_diagnostics.json
```

---

# Phase 4
## Controlled comparison

No tuning.

Run:

Shield4:

A:
current implementation

B:
photo disabled geometry only

C:
photo enabled but only use points within IMU history

Purpose:

Determine whether the PASS depends on unsupported photo timestamps.

Do not claim B/C improvement.

Only diagnose.

Record:

```
artifacts/p3r/shield4_ablation.json
```

---

# Phase 5
## Confirm CUBE attribution

Based on evidence classify:

## Case A

Few/no photo points outside IMU history.

Conclusion:

Cubemap result likely represents intensity improvement.

## Case B

Large number of fallback photo points.

Conclusion:

Livox timing adaptation remains a confounder.

## Case C

ATE changes strongly when removing unsupported photo points.

Conclusion:

Need future Livox timing repair before claiming generalization.

---

# Forbidden

Do NOT:

- tune thresholds
- change photo scale
- change selector
- change projection
- modify Livox parser
- improve ATE intentionally

This is an audit.

---

# Required report

Create:

```
spec/p3r/P3R_SHIELD4_ATTRIBUTION_REPORT.md
```

Must answer:

1. Is Shield4 PASS caused by Cubemap or mixed with timing effects?
2. Is evaluator threshold valid?
3. How many photo points lack IMU support?
4. Is Livox adaptation required before future claims?

---

# Closure

At completion:

- commit
- push
- verify SHA
- clean worktree
- STOP

Do not start P4 automatically.
