#!/usr/bin/env python3
"""Turn the P2.1 official-vs-Super CSV into the compact review artifact."""
import argparse
import csv
import json
from pathlib import Path


FLOAT_FIELDS = {
    "timestamp", "raw_mean", "raw_std", "raw_min", "raw_max", "raw_max_abs",
    "filtered_mean", "filtered_std", "filtered_min", "filtered_max", "filtered_max_abs",
    "dx_mean", "dx_std", "dx_min", "dx_max", "dx_max_abs",
    "dy_mean", "dy_std", "dy_min", "dy_max", "dy_max_abs", "project_max_uv_error",
}
INT_FIELDS = {
    "frame", "point_count", "rows", "cols", "valid_intensity_pixels", "valid_range_pixels",
    "valid_mask_pixels", "raw_exact_mismatches", "filtered_exact_mismatches",
    "dx_exact_mismatches", "dy_exact_mismatches", "mask_mismatches", "range_mismatches",
    "owner_mismatches", "photo_u8_mismatches", "proj_index_mismatches", "projected_samples",
    "project_valid_mismatches",
}
ERROR_FIELDS = ["raw_max_abs", "filtered_max_abs", "dx_max_abs", "dy_max_abs", "project_max_uv_error"]
MISMATCH_FIELDS = [
    "raw_exact_mismatches", "filtered_exact_mismatches", "dx_exact_mismatches", "dy_exact_mismatches",
    "mask_mismatches", "range_mismatches", "owner_mismatches", "photo_u8_mismatches", "proj_index_mismatches",
    "project_valid_mismatches",
]


def read_frames(path: Path):
    with path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    for row in rows:
        for key in FLOAT_FIELDS:
            row[key] = float(row[key])
        for key in INT_FIELDS:
            row[key] = int(row[key])
        row["pass"] = row["pass"] == "PASS"
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv", type=Path)
    parser.add_argument("identity", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    frames = read_frames(args.csv)
    identity = json.loads(args.identity.read_text())
    maximum_errors = {key: max((frame[key] for frame in frames), default=0.0) for key in ERROR_FIELDS}
    exact_mismatches = {key: sum(frame[key] for frame in frames) for key in MISMATCH_FIELDS}
    height_dimensions = sorted({frame["rows"] for frame in frames})
    width_dimensions = sorted({frame["cols"] for frame in frames})
    status = "PASS" if (
        len(frames) == 10
        and height_dimensions == [128]
        and width_dimensions == [1024]
        and all(frame["pass"] for frame in frames)
        and all(value == 0 for value in exact_mismatches.values())
        and maximum_errors["raw_max_abs"] <= 1e-6
        and maximum_errors["filtered_max_abs"] <= 1e-6
        and maximum_errors["dx_max_abs"] <= 1e-6
        and maximum_errors["dy_max_abs"] <= 1e-6
        and maximum_errors["project_max_uv_error"] <= 1e-12
    ) else "FAIL"

    result = {
        "phase": "P2.1 current-frame Ouster image frontend parity",
        "status": status,
        "oracle": {
            "repository": "https://github.com/ethz-asl/COIN-LIO",
            "commit": identity["coin_sha"],
            "config_sha256": {
                "params.yaml": identity["sha256"]["coin.params"],
                "line_removal.yaml": identity["sha256"]["coin.line_removal"],
                "os_enwide.json": identity["sha256"]["coin.os_enwide"],
            },
        },
        "dataset": {
            "path": identity["tunneld_bag"]["path"],
            "sha256": identity["tunneld_bag"]["sha256"],
        },
        "comparison": {
            "frames_requested": 10,
            "frames_processed": len(frames),
            "selected_frame_indices": [frame["frame"] for frame in frames],
            "dimensions": [128, 1024],
            "input_processing": "Both frontends received the identical PointCloudXYZI produced by the pinned official COIN Preprocess; distortion transforms were identity to isolate current-frame frontend semantics.",
            "tolerances": {
                "float_image_and_gradient_max_abs": 1e-6,
                "projected_uv_max_abs": 1e-12,
                "integer_maps_masks_owners_and_u8": "exact equality",
            },
            "maximum_errors": maximum_errors,
            "total_exact_mismatches": exact_mismatches,
            "frames": frames,
        },
        "coverage_limit": "This gate establishes current-frame calibrated projection and image construction only. It does not test feature lifecycle, weak-direction selection, scan-time distortion-aware landmark reprojection, residual/Jacobian, or estimator fusion.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(f"{status}: wrote {args.output}")
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
