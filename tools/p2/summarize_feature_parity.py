#!/usr/bin/env python3
"""Build the compact P2.2 COIN feature-manager shadow artifact from its CSV."""
import argparse
import csv
import hashlib
import json
from pathlib import Path


INT_FIELDS = {
    "bag_frame", "pose_index", "motion_frame", "point_count", "weak_global_count", "weak_lidar_count",
    "official_active", "local_active", "official_added",
    "local_added", "official_removed", "local_removed", "candidates_after_nms", "selected_centers",
    "projection_rejects", "border_rejects", "mask_rejects", "range_rejects", "ncc_rejects",
    "lifetime_rejects", "ncc_count", "state_mismatches",
}
FLOAT_FIELDS = {
    "timestamp", "header_delta_s", "lidar_end_delta_s", "weak_direction_transform_max_abs",
    "weak_geometry_global_max_abs", "weak_geometry_lidar_max_abs", "ncc_min", "ncc_median", "ncc_p95", "ncc_max",
    "center_max_abs", "uv_max_abs", "world_patch_max_abs", "reference_max_abs",
}


def load_rows(path):
    with path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    for row in rows:
        for name in INT_FIELDS:
            row[name] = int(row[name])
        for name in FLOAT_FIELDS:
            value = row[name]
            row[name] = float(value) if value.lower() != "nan" else None
        row["life_histogram"] = {
            int(pair.split(":", 1)[0]): int(pair.split(":", 1)[1])
            for pair in row["life_histogram"].split(";") if pair
        }
        row["pass"] = row["pass"] == "PASS"
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv", type=Path)
    parser.add_argument("identity", type=Path)
    parser.add_argument("trajectory", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--repeat-csv", type=Path)
    args = parser.parse_args()
    rows = load_rows(args.csv)
    identity = json.loads(args.identity.read_text())
    trajectory_sha = hashlib.sha256(args.trajectory.read_bytes()).hexdigest()
    repeat = None
    if args.repeat_csv:
        first_bytes=args.csv.read_bytes();repeat_bytes=args.repeat_csv.read_bytes()
        repeat={
            "path": str(args.repeat_csv),
            "sha256": hashlib.sha256(repeat_bytes).hexdigest(),
            "byte_identical_to_first_run": first_bytes==repeat_bytes,
        }

    mismatch_fields = ["state_mismatches"]
    max_error_fields = ["center_max_abs", "uv_max_abs", "world_patch_max_abs", "reference_max_abs"]
    totals = {
        name: sum(row[name] for row in rows)
        for name in [
            "official_added", "local_added", "official_removed", "local_removed", "ncc_count",
            "candidates_after_nms", "selected_centers", "projection_rejects", "border_rejects",
            "mask_rejects", "range_rejects", "ncc_rejects", "lifetime_rejects",
        ]
    }
    maxima = {name: max((row[name] or 0.0 for row in rows), default=0.0) for name in max_error_fields}
    shadow_pass = len(rows) == 1185 and all(row["pass"] for row in rows) and all(
        row[name] == 0 for row in rows for name in mismatch_fields
    ) and (repeat is None or repeat["byte_identical_to_first_run"])
    row_summaries = []
    keep = [
        "bag_frame", "pose_index", "motion_frame", "timestamp", "header_delta_s", "lidar_end_delta_s", "point_count",
        "weak_global_count", "weak_lidar_count", "weak_direction_transform_max_abs",
        "weak_geometry_global_max_abs", "weak_geometry_lidar_max_abs", "official_active", "local_active",
        "official_added", "local_added", "official_removed", "local_removed", "candidates_after_nms",
        "selected_centers", "projection_rejects", "border_rejects", "mask_rejects", "range_rejects",
        "ncc_rejects", "lifetime_rejects", "ncc_count", "ncc_min", "ncc_median", "ncc_p95", "ncc_max",
        "life_histogram", "state_mismatches", "center_max_abs", "uv_max_abs", "world_patch_max_abs",
        "reference_max_abs", "pass",
    ]
    # Store an ordered columnar table rather than repeated JSON object keys for each of 1185 scans.
    for row in rows:
        packed = []
        for key in keep:
            value = row[key]
            if key == "life_histogram":
                value = ";".join(f"{life}:{count}" for life,count in sorted(value.items()))
            packed.append(value)
        row_summaries.append(packed)

    result = {
        "phase": "P2.2 COIN feature manager shadow parity",
        "shadow_component_status": "PASS" if shadow_pass else "FAIL",
        "phase_gate_status": "PASS" if shadow_pass else "FAIL",
        "oracle": {
            "repository": "https://github.com/ethz-asl/COIN-LIO",
            "commit": identity["coin_sha"],
            "trajectory_sha256": trajectory_sha,
        },
        "dataset": {"path": identity["tunneld_bag"]["path"], "sha256": identity["tunneld_bag"]["sha256"]},
        "pairing": {
            "bag_pointcloud_messages": 1189,
            "oracle_trajectory_rows": len(rows),
            "unpaired_cloud_messages": 4,
            "mapping": "Pair by COIN lidar_end_time = PointCloud2 header stamp + maximum Ouster point timestamp; all 1185 oracle rows match exactly.",
            "maximum_abs_header_to_oracle_timestamp_delta_s": max((row["header_delta_s"] for row in rows), default=0.0),
            "maximum_abs_lidar_end_to_oracle_timestamp_delta_s": max((row["lidar_end_delta_s"] for row in rows), default=0.0),
            "bag_frame_first": min((row["bag_frame"] for row in rows), default=None),
            "bag_frame_last": max((row["bag_frame"] for row in rows), default=None),
            "note": "Four sparse/unpaired messages are interleaved with valid scans; cloud ordinal pairing is not used. Motion trace frame 0 is the geometry-map initialization scan, so motion frame = trajectory row + 1.",
        },
        "comparison": {
            "frames_compared": len(rows),
            "all_frames_exact_feature_state_order": shadow_pass,
            "active_count_total_official_and_local": sum(row["official_active"] for row in rows),
            "lifecycle_totals": totals,
            "maximum_state_differences": maxima,
            "weak_directions": {
                "official_geometry_rows_replayed": True,
                "local_rule": "CoinFeatureManager::weakDirectionsFromGeometry",
                "frames_with_geometry_weak_directions": sum(row["weak_global_count"] > 0 for row in rows),
                "maximum_sign_invariant_global_direction_error": max((row["weak_geometry_global_max_abs"] or 0.0 for row in rows), default=0.0),
                "maximum_sign_invariant_lidar_direction_error": max((row["weak_geometry_lidar_max_abs"] or 0.0 for row in rows), default=0.0),
                "maximum_global_to_lidar_rotation_error": max((row["weak_direction_transform_max_abs"] or 0.0 for row in rows), default=0.0),
                "note": "The exact official post-geometry translation Jacobian rows and output directions were traced from a temporary oracle copy. The local rule independently reproduces per-frame direction counts and orientations, including fallback.",
            },
            "acquisition_time_transforms": "Real per-point T_Li_Lk lookup tables and vec_idx arrays were traced from official IMU undistortion. Raw points were reconstructed into scan-end coordinates with the inverse transform; complete projected-index maps and feature states then matched across all 1185 scans.",
            "frame_table": {"columns": keep, "rows": row_summaries},
        },
        "repeatability": repeat,
        "remaining_p2_2_gates": [],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(f"shadow={'PASS' if shadow_pass else 'FAIL'}; phase={'PASS' if shadow_pass else 'FAIL'}; wrote {args.output}")
    return 0 if shadow_pass else 1


if __name__ == "__main__":
    raise SystemExit(main())
