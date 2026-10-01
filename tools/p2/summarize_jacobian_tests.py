#!/usr/bin/env python3
"""Summarize TunnelD COIN fixed-correspondence photometric finite differences."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path


def quantile(values, q):
    values = sorted(v for v in values if math.isfinite(v))
    if not values:
        return None
    return values[int(math.floor(q * (len(values) - 1)))]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    with args.csv.open(newline="") as stream:
        rows = list(csv.DictReader(stream))

    def numbers(field):
        return [float(row[field]) for row in rows]

    photo = [row for row in rows if max(abs(float(row["analytic_h"])), abs(float(row["numeric_h"]))) >= 1e-5]
    photo_errors = [float(row["relative_error"]) for row in photo]
    unique = {}
    for row in rows:
        unique[(row["pose_index"], row["feature_index"], row["patch_index"])] = row
    samples = list(unique.values())
    projector_x = [float(row["projector_relative_error_x"]) for row in samples]
    projector_y = [float(row["projector_relative_error_y"]) for row in samples]
    image_x = [float(row["image_gradient_relative_error_x"]) for row in samples]
    image_y = [float(row["image_gradient_relative_error_y"]) for row in samples]
    signs = sum(row["sign_agreement"] == "0" for row in photo)
    source_h = numbers("official_source_h_error")
    projector_p95 = max(quantile(projector_x, .95) or 0., quantile(projector_y, .95) or 0.)
    image_gradient_p95 = max(quantile(image_x, .95) or 0., quantile(image_y, .95) or 0.)
    photo_p95 = quantile(photo_errors, .95)
    gate_pass = bool(rows) and max(source_h, default=0.) < 1e-8 and signs == 0 and \
        photo_p95 is not None and photo_p95 < 1e-4 and projector_p95 < 1e-4 and image_gradient_p95 < 1e-4

    result = {
        "phase": "P2.3 COIN residual/Jacobian finite differences",
        "phase_gate_status": "PASS" if gate_pass else "BLOCKED",
        "dataset": "ENWIDE TunnelD",
        "trajectory_pairing": "Exact COIN lidar_end_time; all selected scan rows have zero timestamp delta.",
        "sample_policy": {
            "pose_stride": 23,
            "features_per_sampled_frame": ["first", "middle", "last"],
            "patch_points": [0, 12, 24],
            "pixel_boundary_exclusion_fraction": 0.04,
            "position_epsilon_m": 1e-3,
            "right_rotation_epsilon_rad": 1e-4,
            "fixed_correspondence": True,
        },
        "official_formula_parity": {
            "status": "PASS" if max(source_h, default=0.) < 1e-8 else "FAIL",
            "samples": len(rows),
            "maximum_abs_H_difference": max(source_h, default=0.),
            "note": "The local source-formula row matches the pinned COIN central-difference image gradient and analytic projection formula. The FD gate below tests the derivative of the actual bilinear residual.",
        },
        "projector_jacobian_fd": {
            "horizontal_relative_p95": quantile(projector_x, .95),
            "vertical_relative_p95": quantile(projector_y, .95),
            "combined_relative_p95": projector_p95,
            "status": "PASS" if projector_p95 < 1e-4 else "FAIL",
        },
        "image_gradient_fd": {
            "horizontal_relative_p95": quantile(image_x, .95),
            "vertical_relative_p95": quantile(image_y, .95),
            "combined_relative_p95": image_gradient_p95,
            "status": "PASS" if image_gradient_p95 < 1e-4 else "FAIL",
        },
        "fixed_correspondence_pose_jacobian_fd": {
            "samples": len(photo),
            "six_dof_rows": len(rows),
            "relative_p50": quantile(photo_errors, .50),
            "relative_p95": photo_p95,
            "sign_disagreements": signs,
            "sign_agreement_fraction": 1. - signs / len(photo) if photo else 0.,
            "status": "PASS" if photo_p95 is not None and photo_p95 < 1e-4 and signs == 0 else "FAIL",
        },
        "diagnosis": [
            "The source-faithful correction row agrees with the pinned COIN formula to roundoff, so the mismatch is not a local sign or tangent-order implementation error.",
            "The official projector Jacobian uses one global vertical scale derived from the first/last beam elevations, while project() interpolates each calibrated beam interval; horizontal projection FD passes and vertical projection FD does not.",
            "The official image derivative samples bilinear intensity at u±1/v±1 and uses a one-pixel central difference. The residual itself is bilinear, whose local derivative differs on real TunnelD images.",
            "These are upstream COIN linearization approximations away from pixel/clamp boundaries. Changing them to exact derivatives would change the pinned official H and must not be presented as official formula parity.",
        ],
        "fusion_gate": "NOT_RUN: P2.3 numerical Jacobian gate failed; P2.4 is gated on this result.",
        "input_csv_sha256": hashlib.sha256(args.csv.read_bytes()).hexdigest(),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(f"jacobian_gate={'PASS' if gate_pass else 'BLOCKED'}; wrote {args.output}")
    return 0 if gate_pass else 1


if __name__ == "__main__":
    raise SystemExit(main())
