#!/usr/bin/env python3
"""Summarize exact official COIN and Super geometry translation-row traces."""
import argparse
import bisect
import csv
import hashlib
import json
import math
import struct
from pathlib import Path

import numpy as np


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_official(path, frame_csv):
    timestamps = {}
    with open(frame_csv, newline="") as stream:
        for row in csv.DictReader(stream):
            timestamps[int(row["pose_index"])] = float(row["timestamp"])
    frames = []
    with open(path, "rb") as stream:
        while True:
            head = stream.read(12)
            if not head:
                break
            if len(head) != 12:
                raise ValueError("truncated official geometry trace header")
            frame, count, cols = struct.unpack("<III", head)
            if cols not in (0, 3) or (count and cols != 3):
                raise ValueError("official geometry trace must contain three columns")
            values = np.frombuffer(stream.read(count * cols * 8), dtype="<f8")
            if values.size != count * cols:
                raise ValueError("truncated official geometry row payload")
            if frame != len(frames):
                raise ValueError("non-sequential official frame IDs")
            rows = values.reshape((count, cols)).copy() if count else np.zeros((0, 3))
            frames.append((frame, timestamps.get(frame), count, rows, None))
    if len(frames) != len(timestamps):
        raise ValueError("official geometry trace and timestamp frame counts differ")
    return frames


def read_super(path):
    frames = []
    with open(path, "rb") as stream:
        while True:
            head = stream.read(20)
            if not head:
                break
            if len(head) != 20:
                raise ValueError("truncated Super geometry trace header")
            frame, stamp, effective, count = struct.unpack("<IdII", head)
            payload = np.frombuffer(stream.read(count * 4 * 8), dtype="<f8")
            if payload.size != count * 4:
                raise ValueError("truncated Super geometry row payload")
            if frame != len(frames):
                raise ValueError("non-sequential Super frame IDs")
            matrix = payload.reshape((count, 4)).copy() if count else np.zeros((0, 4))
            frames.append((frame, stamp, effective, matrix[:, :3], matrix[:, 3]))
    return frames


def summarize(frame, threshold=25.0):
    frame_id, stamp, effective, rows, residuals = frame
    rows = np.asarray(rows, dtype=float).reshape((-1, 3))
    finite = np.isfinite(rows).all(axis=1)
    norms = np.linalg.norm(rows, axis=1)
    nonzero = finite & (norms > 1e-12)
    valid = rows[nonzero]
    valid_norms = norms[nonzero]
    if len(valid):
        unit = valid / valid_norms[:, None]
        eigenvalues, eigenvectors = np.linalg.eigh(valid.T @ valid)
        dots = np.abs(unit @ eigenvectors).astype(np.float32)
        above = dots > 0.5
        contributions = np.sum(np.where(above, dots, 0.0), axis=0, dtype=np.float64)
        counts = above.sum(axis=0)
        abs_means = np.abs(unit).mean(axis=0)
        abs_medians = np.median(np.abs(unit), axis=0)
        # Plane-normal signs are equivalent; canonicalize before angular binning.
        canonical = unit.copy()
        for axis in range(3):
            pending = np.abs(canonical[:, :axis]).max(axis=1) < 1e-12 if axis else np.ones(len(unit), bool)
            flip = pending & (canonical[:, axis] < 0)
            canonical[flip] *= -1
        theta = np.arccos(np.clip(canonical[:, 2], -1., 1.))
        phi = (np.arctan2(canonical[:, 1], canonical[:, 0]) + 2 * np.pi) % (2 * np.pi)
        bins = np.stack((np.floor(theta / np.deg2rad(10)).astype(int),
                         np.floor(phi / np.deg2rad(10)).astype(int)), axis=1)
        unique, bin_counts = np.unique(bins, axis=0, return_counts=True)
        probabilities = bin_counts / bin_counts.sum()
        entropy = float(-np.sum(probabilities * np.log(probabilities)))
        ranked = sorted(zip(bin_counts.tolist(), unique.tolist()), reverse=True)[:5]
        dominant = ";".join(f"{b[1][0]}:{b[1][1]}={b[0]}" for b in ranked)
        eig = np.maximum(eigenvalues, 0.)
        e1, e2, e3 = map(float, eig)
        ratios = [e1 / e2 if e2 > 0 else None,
                  e1 / e3 if e3 > 0 else None,
                  e2 / e3 if e3 > 0 else None,
                  e3 / max(e1, np.finfo(float).eps)]
    else:
        contributions = np.zeros(3); counts = np.zeros(3, dtype=int)
        abs_means = np.full(3, np.nan); abs_medians = np.full(3, np.nan)
        e1 = e2 = e3 = 0.; ratios = [None, None, None, None]
        unique = np.zeros((0, 2), dtype=int); entropy = 0.; dominant = ""
    weak = contributions < threshold
    residual_valid = np.asarray([] if residuals is None else residuals, dtype=float)
    residual_valid = residual_valid[np.isfinite(residual_valid)]
    raw_norms = norms[finite]

    def nstats(values):
        if len(values) == 0:
            return (math.nan, math.nan, math.nan, math.nan, math.nan)
        q = np.quantile(values, [0, .5, .95, 1])
        return (float(q[0]), float(np.mean(values)), float(q[1]), float(q[2]), float(q[3]))

    norm_min, norm_mean, norm_median, norm_p95, norm_max = nstats(raw_norms)
    return {
        "frame": frame_id, "timestamp": stamp, "effective_point_count": effective,
        "N_geo_rows": len(rows), "finite_rows": int(finite.sum()), "nonzero_finite_rows": len(valid),
        "row_norm_min": norm_min, "row_norm_mean": norm_mean, "row_norm_median": norm_median,
        "row_norm_p95": norm_p95, "row_norm_max": norm_max,
        "lambda1": e1, "lambda2": e2, "lambda3": e3,
        "lambda1_over_lambda2": ratios[0], "lambda1_over_lambda3": ratios[1],
        "lambda2_over_lambda3": ratios[2], "condition_proxy": ratios[3],
        "contribution1": float(contributions[0]), "contribution2": float(contributions[1]),
        "contribution3": float(contributions[2]),
        "contribution_per_row1": float(contributions[0] / len(rows)) if len(rows) else math.nan,
        "contribution_per_row2": float(contributions[1] / len(rows)) if len(rows) else math.nan,
        "contribution_per_row3": float(contributions[2] / len(rows)) if len(rows) else math.nan,
        "above_half_count1": int(counts[0]), "above_half_count2": int(counts[1]),
        "above_half_count3": int(counts[2]),
        "above_half_fraction1": float(counts[0] / len(valid)) if len(valid) else math.nan,
        "above_half_fraction2": float(counts[1] / len(valid)) if len(valid) else math.nan,
        "above_half_fraction3": float(counts[2] / len(valid)) if len(valid) else math.nan,
        "weak_original_count": int(weak.sum()), "fallback_original_xyz": bool(not weak.any()),
        "weakest_eigenvector_x": float(np.linalg.eigh(valid.T @ valid)[1][0, 0]) if len(valid) else math.nan,
        "weakest_eigenvector_y": float(np.linalg.eigh(valid.T @ valid)[1][1, 0]) if len(valid) else math.nan,
        "weakest_eigenvector_z": float(np.linalg.eigh(valid.T @ valid)[1][2, 0]) if len(valid) else math.nan,
        "abs_nx_mean": float(abs_means[0]), "abs_ny_mean": float(abs_means[1]),
        "abs_nz_mean": float(abs_means[2]), "abs_nx_median": float(abs_medians[0]),
        "abs_ny_median": float(abs_medians[1]), "abs_nz_median": float(abs_medians[2]),
        "occupied_10deg_bins": int(len(unique)), "angular_entropy_nats": entropy,
        "angular_effective_bins": float(math.exp(entropy)), "dominant_angular_bins": dominant,
        "residual_rms": float(np.sqrt(np.mean(residual_valid ** 2))) if len(residual_valid) else math.nan,
        "residual_median_abs": float(np.median(np.abs(residual_valid))) if len(residual_valid) else math.nan,
        "rejected_or_unrepresented_effective_points":
            int(effective - len(rows)) if effective is not None else None,
    }


def finite_stats(rows, key):
    values = np.array([r[key] for r in rows if r.get(key) is not None and np.isfinite(r[key])])
    if not len(values):
        return None
    return {"count": int(len(values)), "min": float(values.min()), "p10": float(np.quantile(values, .1)),
            "median": float(np.median(values)), "p90": float(np.quantile(values, .9)),
            "max": float(values.max()), "mean": float(values.mean())}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--official-rows", type=Path, required=True)
    parser.add_argument("--official-frames", type=Path, required=True)
    parser.add_argument("--super-rows", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    official = [summarize(x) for x in read_official(args.official_rows, args.official_frames)]
    super_frames = [summarize(x) for x in read_super(args.super_rows)]
    fields = list(official[0])
    for name, rows in (("official_geometry_signal.csv", official), ("super_geometry_signal.csv", super_frames)):
        with open(args.output_dir / name, "w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
            writer.writeheader()
            writer.writerows(rows)
    weak_norm = [r[f"contribution_per_row{d}"] for r in official
                 for d in range(1, 4) if r[f"contribution{d}"] < 25 and r["N_geo_rows"]]
    normalized_threshold = float(np.median(weak_norm))
    official_times = sorted((r["timestamp"], r) for r in official if r["timestamp"] is not None)
    times = [x[0] for x in official_times]
    pairs = []
    for row in super_frames:
        if row["timestamp"] is None or not times:
            continue
        idx = bisect.bisect_left(times, row["timestamp"])
        candidates = [j for j in (idx - 1, idx) if 0 <= j < len(times)]
        j = min(candidates, key=lambda k: abs(times[k] - row["timestamp"]))
        ref = official_times[j][1]
        pairs.append({"super_frame": row["frame"], "official_frame": ref["frame"],
                      "timestamp_delta_s": row["timestamp"] - ref["timestamp"],
                      "super_N_geo_rows": row["N_geo_rows"], "official_N_geo_rows": ref["N_geo_rows"],
                      "super_contribution_per_row1": row["contribution_per_row1"],
                      "official_contribution_per_row1": ref["contribution_per_row1"],
                      "super_weak_count": row["weak_original_count"],
                      "official_weak_count": ref["weak_original_count"]})
    close_pairs = [p for p in pairs if abs(p["timestamp_delta_s"]) <= .05]
    summary = {
        "inputs": {"official_geometry_trace_sha256": sha(args.official_rows),
                   "official_frame_csv_sha256": sha(args.official_frames),
                   "super_geometry_trace_sha256": sha(args.super_rows)},
        "official": {"frames": len(official), "weak_frames": sum(r["weak_original_count"] > 0 for r in official),
                     "fallback_frames": sum(r["fallback_original_xyz"] for r in official),
                     "rows": {k: finite_stats(official, k) for k in
                              ("N_geo_rows", "row_norm_median", "lambda1_over_lambda2", "lambda1_over_lambda3",
                               "lambda2_over_lambda3", "contribution1", "contribution2", "contribution3",
                               "contribution_per_row1", "contribution_per_row2", "contribution_per_row3",
                               "angular_entropy_nats", "residual_rms")}},
        "super": {"frames": len(super_frames), "weak_frames": sum(r["weak_original_count"] > 0 for r in super_frames),
                  "fallback_frames": sum(r["fallback_original_xyz"] for r in super_frames),
                  "rows": {k: finite_stats(super_frames, k) for k in
                           ("N_geo_rows", "effective_point_count", "row_norm_median", "lambda1_over_lambda2",
                            "lambda1_over_lambda3", "lambda2_over_lambda3", "contribution1", "contribution2",
                            "contribution3", "contribution_per_row1", "contribution_per_row2",
                            "contribution_per_row3", "angular_entropy_nats", "residual_rms")}},
        "pairing": {"nearest_timestamp_pairs": len(pairs), "within_50ms": len(close_pairs),
                    "close_pair_delta_s": finite_stats(close_pairs, "timestamp_delta_s")},
        "normalized_threshold_derivation": {
            "rule": "median official contribution/N over every direction marked weak by exact c<25 rule",
            "official_weak_direction_samples": len(weak_norm), "threshold": normalized_threshold,
            "frozen_candidate_threshold": 0.01609598,
            "threshold_matches_frozen_rounding": abs(normalized_threshold - 0.01609598) < 1e-7,
            "use_for_selector_S3": abs(normalized_threshold - 0.01609598) < 1e-7},
        "official_rule_parity": {"expected_weak_frames_from_p2": 1014,
                                 "recomputed_weak_frames": sum(r["weak_original_count"] > 0 for r in official),
                                 "pass": sum(r["weak_original_count"] > 0 for r in official) == 1014},
    }
    (args.output_dir / "geometry_signal_summary.json").write_text(json.dumps(summary, indent=2, allow_nan=False) + "\n")
    with open(args.output_dir / "paired_geometry_signal.csv", "w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(pairs[0]) if pairs else [], lineterminator="\n")
        writer.writeheader(); writer.writerows(close_pairs)
    print(json.dumps({"official_weak_frames": summary["official"]["weak_frames"],
                      "super_weak_frames": summary["super"]["weak_frames"],
                      "official_row_median": summary["official"]["rows"]["N_geo_rows"]["median"],
                      "super_row_median": summary["super"]["rows"]["N_geo_rows"]["median"],
                      "normalized_threshold": normalized_threshold,
                      "paired_within_50ms": len(close_pairs)}))


if __name__ == "__main__":
    main()
