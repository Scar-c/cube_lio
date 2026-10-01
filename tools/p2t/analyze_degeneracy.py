#!/usr/bin/env python3
"""Derive geometry-only Super degeneracy confidence and frozen gate thresholds."""
import argparse
import csv
import hashlib
import json
import math
import struct
from pathlib import Path

import numpy as np


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_trace(path):
    frames = []
    with open(path, "rb") as stream:
        while True:
            header = stream.read(20)
            if not header:
                break
            if len(header) != 20:
                raise ValueError("truncated Super geometry trace header")
            frame, timestamp, effective_points, count = struct.unpack("<IdII", header)
            values = np.frombuffer(stream.read(count * 4 * 8), dtype="<f8")
            if values.size != count * 4:
                raise ValueError("truncated Super geometry row payload")
            if frame != len(frames):
                raise ValueError("non-sequential geometry frame IDs")
            matrix = values.reshape((count, 4)).copy() if count else np.zeros((0, 4))
            frames.append({"frame": frame, "timestamp": timestamp,
                           "effective_points": effective_points,
                           "rows": matrix[:, :3], "residuals": matrix[:, 3]})
    if not frames:
        raise ValueError("empty geometry trace: {}".format(path))
    return frames


def describe(values):
    array = np.asarray([x for x in values if x is not None and np.isfinite(x)], dtype=float)
    if not len(array):
        return {"count": 0}
    return {"count": int(len(array)), "min": float(array.min()),
            "p05": float(np.quantile(array, .05)), "p50": float(np.quantile(array, .50)),
            "p90": float(np.quantile(array, .90)), "p95": float(np.quantile(array, .95)),
            "p99": float(np.quantile(array, .99)), "max": float(array.max()),
            "mean": float(array.mean())}


def summarize_frames(frames, dataset):
    result = []
    previous_axis = None
    previous_time = None
    for frame in frames:
        rows = frame["rows"]
        valid = np.isfinite(rows).all(axis=1)
        rows = rows[valid]
        count = len(rows)
        eigenvalues = np.zeros(3)
        eigenvectors = np.eye(3)
        valid_signal = count > 3
        if valid_signal:
            eigenvalues, eigenvectors = np.linalg.eigh(rows.T @ rows)
            eigenvalues = np.maximum(eigenvalues, 0.)
            valid_signal = np.isfinite(eigenvalues).all() and eigenvalues[2] > 1e-12
        if valid_signal:
            lambda1, lambda2, lambda3 = [float(x) for x in eigenvalues]
            ratio12 = lambda1 / lambda2 if lambda2 > 1e-12 else 1.
            ratio13 = lambda1 / lambda3
            anisotropy = min(1., max(0., 1. - ratio13))
            eigengap = min(1., max(0., (lambda2 - lambda1) / lambda3))
            axis = eigenvectors[:, 0]
            dt = frame["timestamp"] - previous_time if previous_time is not None else math.inf
            stability = (abs(float(np.dot(axis, previous_axis)))
                         if previous_axis is not None and 0. < dt <= .25 else 0.)
            confidence = anisotropy * eigengap * stability
            angle_degrees = math.degrees(math.acos(min(1., max(0., stability)))) if stability else None
            previous_axis = axis
            previous_time = frame["timestamp"]
        else:
            lambda1 = lambda2 = lambda3 = ratio12 = ratio13 = 0.
            anisotropy = eigengap = stability = confidence = 0.
            angle_degrees = None
        result.append({
            "dataset": dataset, "frame": frame["frame"], "timestamp": frame["timestamp"],
            "effective_points": frame["effective_points"], "N_geo_rows": count,
            "lambda1": lambda1, "lambda2": lambda2, "lambda3": lambda3,
            "lambda1_over_lambda2": ratio12, "lambda1_over_lambda3": ratio13,
            "anisotropy_confidence": anisotropy, "eigengap_confidence": eigengap,
            "weakest_axis_stability": stability, "weakest_axis_step_degrees": angle_degrees,
            "degeneracy_confidence": confidence, "valid_signal": bool(valid_signal),
        })
    return result


def summarize_dataset(rows, thresholds=None):
    valid = [r for r in rows if r["valid_signal"]]
    summary = {key: describe([r[key] for r in valid]) for key in (
        "N_geo_rows", "lambda1_over_lambda2", "lambda1_over_lambda3",
        "anisotropy_confidence", "eigengap_confidence", "weakest_axis_stability",
        "weakest_axis_step_degrees", "degeneracy_confidence")}
    summary["frames"] = len(rows)
    summary["valid_geometry_frames"] = len(valid)
    if thresholds:
        summary["gate_activation"] = {
            key: {"frames": sum(r["valid_signal"] and r["degeneracy_confidence"] >= threshold for r in rows),
                  "fraction": sum(r["valid_signal"] and r["degeneracy_confidence"] >= threshold for r in rows) / len(rows)}
            for key, threshold in thresholds.items()
        }
    return summary


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--tunnel-geometry", type=Path, required=True)
    parser.add_argument("--ntu-geometry", type=Path, required=True)
    parser.add_argument("--shield-geometry", type=Path, required=True)
    parser.add_argument("--s2-geometry", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=Path("artifacts/p2t"))
    args = parser.parse_args()
    inputs = {"tunnel_d_g1": args.tunnel_geometry, "eee_01": args.ntu_geometry,
              "shield1": args.shield_geometry, "tunnel_d_s2": args.s2_geometry}
    signals = {name: summarize_frames(read_trace(path), name) for name, path in inputs.items()}
    normal_confidence = [row["degeneracy_confidence"] for row in signals["eee_01"]
                         if row["valid_signal"]]
    if not normal_confidence:
        raise ValueError("NTU geometry produced no valid confidence samples")
    # Thresholds are frozen from geometry-only NTU confidence quantiles before
    # any gated TunnelD ATE is inspected. G2 uses a stricter tail quantile.
    g1_threshold = float(np.quantile(normal_confidence, .95))
    g2_threshold = float(np.quantile(normal_confidence, .99))
    thresholds = {"G1": g1_threshold, "G2": g2_threshold}
    names = ("tunnel_d_g1", "eee_01", "shield1", "tunnel_d_s2")
    summary = {
        "confidence_definition": {
            "lambda1_over_lambda2": "lambda1/lambda2, eigenvalues of H_t^T H_t in ascending order",
            "lambda1_over_lambda3": "lambda1/lambda3, eigenvalues of H_t^T H_t in ascending order",
            "anisotropy_confidence": "clamp(1-lambda1/lambda3, 0, 1)",
            "eigengap_confidence": "clamp((lambda2-lambda1)/lambda3, 0, 1)",
            "weakest_axis_stability": "abs(dot(v1[t],v1[t-1])) when timestamp delta is in (0, 0.25] s; zero without temporal support",
            "degeneracy_confidence": "anisotropy_confidence * eigengap_confidence * weakest_axis_stability",
            "sign_invariant": True,
        },
        "threshold_derivation": {
            "calibration_dataset": "NTU eee_01 photo-off Super geometry rows",
            "selection_inputs": "geometry rows, timestamps and eigensignals only; no GT or ATE",
            "G1_rule": "95th percentile of valid per-frame degeneracy_confidence",
            "G2_rule": "99th percentile of valid per-frame degeneracy_confidence",
            "G1_confidence_threshold": g1_threshold,
            "G2_confidence_threshold": g2_threshold,
            "G2_is_at_least_as_conservative_as_G1": g2_threshold >= g1_threshold,
        },
        "datasets": {name: summarize_dataset(signals[name], thresholds) for name in names},
        "inputs": {name: {"path": str(path), "sha256": sha(path)} for name, path in inputs.items()},
        "frame_count": {name: len(signals[name]) for name in names},
    }
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "degeneracy_statistics.json").write_text(
        json.dumps(summary, indent=2, allow_nan=False) + "\n")
    columns = list(signals[names[0]][0])
    with open(args.output_dir / "frame_geometry_signals.csv", "w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns, lineterminator="\n")
        writer.writeheader()
        for name in names:
            writer.writerows(signals[name])
    print(json.dumps({"thresholds": thresholds,
                      "activation": {name: summary["datasets"][name]["gate_activation"]
                                     for name in names}}, indent=2))


if __name__ == "__main__":
    main()
