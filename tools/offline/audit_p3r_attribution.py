#!/usr/bin/env python3
"""Recompute evaluator quantities and archive the fixed Shield4 attribution audit."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "artifacts/p3r"
GT = Path("/home/lc/algorithm_versa/bag/GEODE/Shield_tunnel4.txt")
BASE = "f3060768abee4d2ee2343b07edf62346c7cd0f87"
ORIGINAL_A = ROOT / "runtime/p3_shield4_cube_raw_c1_all32"
ORIGINAL_B = ROOT / "runtime/p3_shield4_geometry32"
RUNS = {
    "A": "p3r_shield4_a_current32",
    "B": "p3r_shield4_b_geometry32",
    "C": "p3r_shield4_c_supported32",
}


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path):
    return json.loads(Path(path).read_text())


def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(value, indent=2, sort_keys=True, allow_nan=False) + "\n")


def length(positions):
    return float(np.linalg.norm(np.diff(positions, axis=0), axis=1).sum())


def quaternion_rotation(q):
    x, y, z, w = q / np.linalg.norm(q)
    return np.array([
        [1 - 2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
        [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
        [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)],
    ])


def manual_evaluation(folder):
    """Independent association and no-scale Kabsch implementation, same protocol."""
    poses = np.loadtxt(folder / "trajectory.tum")
    gt = np.loadtxt(GT)
    calibration_q = np.array([-.00492765, .00575961, .0117651, .999901])
    # The frozen evaluator does not normalize this calibration quaternion.
    x, y, z, w = calibration_q
    calibration_r = np.array([
        [1-2*y*y-2*z*z, 2*x*y-2*z*w, 2*x*z+2*y*w],
        [2*x*y+2*z*w, 1-2*x*x-2*z*z, 2*y*z-2*x*w],
        [2*x*z-2*y*w, 2*y*z+2*x*w, 1-2*x*x-2*y*y],
    ])
    lever = -np.linalg.inv(calibration_r) @ np.array([.00947221, -.308202, -.365733])
    prism = poses[:, 1:4] + np.array([quaternion_rotation(q) @ lever for q in poses[:, 4:8]])
    first, second = (gt[:, 0], poses[:, 0]) if len(gt) < len(poses) else (poses[:, 0], gt[:, 0])
    ids_first, ids_second = [], []
    for i, stamp in enumerate(first):
        distances = np.abs(second - stamp)
        j = int(np.argmin(distances))
        if distances[j] <= .1:
            ids_first.append(i)
            ids_second.append(j)
    ids_gt, ids_est = (ids_first, ids_second) if len(gt) < len(poses) else (ids_second, ids_first)
    estimated, matched_gt = prism[ids_est], gt[ids_gt, 1:4]
    x0, y0 = estimated - estimated.mean(axis=0), matched_gt - matched_gt.mean(axis=0)
    u, _, vt = np.linalg.svd(x0.T @ y0)
    correction = np.eye(3)
    correction[2, 2] = np.linalg.det(vt.T @ u.T)
    rotation = vt.T @ correction @ u.T
    translation = matched_gt.mean(axis=0) - rotation @ estimated.mean(axis=0)
    aligned = estimated @ rotation.T + translation
    errors = np.linalg.norm(aligned - matched_gt, axis=1)
    archived = load_json(folder / "evaluation.json")
    rmse = float(np.sqrt(np.mean(errors**2)))
    gt_length = length(gt[:, 1:4])
    assert abs(rmse - archived["ate_rmse_m"]) < 1e-6, (rmse, archived["ate_rmse_m"])
    assert len(errors) == archived["matched"]
    return {
        "run": folder.name, "gt_rows": len(gt), "gt_duration_s": float(gt[-1, 0]-gt[0, 0]),
        "gt_path_length_m": gt_length, "matched_gt_path_length_m": length(matched_gt),
        "estimate_frames": len(poses), "estimate_raw_full_path_length_m": length(poses[:, 1:4]),
        "estimate_prism_full_path_length_m": length(prism),
        "estimate_prism_matched_path_length_m": length(estimated),
        "estimate_aligned_matched_path_length_m": length(aligned),
        "alignment_rotation": rotation.tolist(), "alignment_translation_m": translation.tolist(),
        "alignment_scale": 1., "rotation_determinant": float(np.linalg.det(rotation)),
        "lever_arm_m": lever.tolist(), "matched": len(errors),
        "unique_matched_estimates": len(set(ids_est)), "unique_matched_gt": len(set(ids_gt)),
        "manual_ate_rmse_m": rmse, "archived_ate_rmse_m": archived["ate_rmse_m"],
        "rmse_difference_m": rmse-archived["ate_rmse_m"],
        "limit_20pct_gt_full_m_exclusive": .2*gt_length,
        "pass_20pct_gt_full": rmse < .2*gt_length,
        "ate_percent_gt_full": 100.*rmse/gt_length,
        "limit_20pct_matched_gt_m_exclusive": .2*length(matched_gt),
        "pass_20pct_matched_gt_sensitivity": rmse < .2*length(matched_gt),
        "matched_first_timestamp": float(gt[ids_gt[0], 0]),
        "matched_last_timestamp": float(gt[ids_gt[-1], 0]),
        "estimate_tail_after_gt_s": float(poses[-1, 0]-gt[-1, 0]),
        "max_association_time_difference_s": float(np.max(np.abs(gt[ids_gt, 0]-poses[ids_est, 0]))),
        "gt_sha256": sha(GT), "trajectory_sha256": sha(folder / "trajectory.tum"),
    }


def evaluator_audit():
    manual = manual_evaluation(ORIGINAL_A)
    stored_length = load_json(ROOT / "artifacts/p3/shield4_results.json")["ground_truth_path_length_m"]
    assert abs(stored_length - manual["gt_path_length_m"]) < 1e-9
    result = {
        "audited_commit": BASE,
        "evaluate_py_calculates_path_length": False,
        "threshold_location": "tools/offline/archive_p3_results.py:139; literal GT path length",
        "length_definition": "sum of consecutive 3D GT position distances over all 911 GT rows",
        "length_source": "full unaligned GT, not estimate; SE3 alignment preserves a fixed path length",
        "archived_literal_gt_length_m": stored_length,
        "association": "nearest timestamps: iterate shorter array, max_diff=0.1s inclusive, offset=0; no interpolation for Shield4",
        "alignment": "one best-fit proper SE3 on all associated prism-position pairs; no scale",
        "shared_20pct_rule_in_evaluate_py": False,
        "cross_dataset_comparability": "Not a common benchmark in this repository: NTU uses author interpolation/duplicate removal; stress datasets use nearest matches; GT coverage and sampling differ; 20% is the Shield4 prompt acceptance rule.",
        "manual_shield4": manual,
        "source_sha256": {path: sha(ROOT / path) for path in (
            "eval/evaluate.py", "eval/ntu_author.py", "tools/offline/archive_p3_results.py")},
    }
    write("evaluator_analysis.json", result)
    print(json.dumps({"gt_length_m": manual["gt_path_length_m"], "threshold_m": manual["limit_20pct_gt_full_m_exclusive"],
                      "manual_ate_m": manual["manual_ate_rmse_m"], "matched": manual["matched"],
                      "pass": manual["pass_20pct_gt_full"], "matched_gt_length_m": manual["matched_gt_path_length_m"]}))


def quantiles(values):
    values = np.asarray(values, dtype=float)
    return {name: float(value) for name, value in zip(("min", "p50", "p90", "p99", "max"),
                                                     np.quantile(values, (0, .5, .9, .99, 1)))}


def time_summary(arm, folder):
    source = folder / "photo_time_audit.csv"
    destination = OUT / ("shield4_time_" + arm.lower() + ".csv")
    shutil.copyfile(source, destination)
    with source.open(newline="") as stream:
        rows = [{k: float(v) for k, v in row.items()} for row in csv.DictReader(stream)]
    assert len(rows) == load_json(folder / "evaluation.json")["frames"]
    integer_columns = [key for key in rows[0] if key.endswith("points") or key.startswith("photo_points_")]
    sums = {key: int(sum(row[key] for row in rows)) for key in integer_columns}
    total, retained = sums["photo_points_total"], sums["photo_points_retained"]
    supported = [row for row in rows if row["frame_supported"]]
    return {
        "scans": len(rows), "supported_frames": len(supported), "skipped_history_frames": len(rows)-len(supported),
        "counts": sums,
        "unsupported_point_percentage_raw": 100.*sums["photo_points_outside_history"]/total,
        "actual_fallback_percentage_retained": 100.*sums["fallback_deskew_points"]/retained if retained else 0.,
        "scans_with_outside_history_points": sum(row["photo_points_outside_history"] > 0 for row in rows),
        "scans_with_actual_fallback": sum(row["fallback_deskew_points"] > 0 for row in rows),
        "fallback_percentage_per_scan": quantiles([row["fallback_percentage"] for row in rows]),
        "photo_end_minus_geometry_end_ms": quantiles([1000.*(row["max_photo_timestamp"]-row["scan_end_geometry"]) for row in rows]),
        "geometry_end_minus_history_last_ms": quantiles([1000.*(row["scan_end_geometry"]-row["history_last"]) for row in supported]),
        "photo_end_minus_history_last_ms": quantiles([1000.*(row["max_photo_timestamp"]-row["history_last"]) for row in supported]),
        "max_photo_offset_s_per_scan": quantiles([row["max_photo_offset_s"] for row in rows]),
        "per_scan_csv": str(destination.relative_to(ROOT)), "per_scan_csv_sha256": sha(destination),
        "worst_fallback_scans": sorted(rows, key=lambda row: row["fallback_percentage"], reverse=True)[:5],
    }


def archive():
    OUT.mkdir(parents=True, exist_ok=True)
    evaluation = load_json(OUT / "evaluator_analysis.json")
    threshold = evaluation["manual_shield4"]["limit_20pct_gt_full_m_exclusive"]
    arms, diagnostics = {}, {}
    for arm, name in RUNS.items():
        folder = ROOT / "runtime" / name
        identity = load_json(folder / "identity.json")
        metrics = load_json(folder / "evaluation.json")
        status = load_json(folder / "result.json")
        assert status["status"] == "SUCCESS" and identity["threads"] == 32
        assert identity["dataset"] == "shield4" and not identity["coin"] and not identity["shadow"]
        if arm in ("A", "C"):
            assert identity["photo"] and identity["photo_time_audit"]
            assert identity["projection"] == "cubemap" and identity["measurement"] == "raw"
            assert identity["idw_enable"] and identity["photo_selector"] == "all"
            assert identity["photo_policy"] == "C0" and identity["photo_gate_threshold"] == 0.31913064578672057
        else:
            assert not identity["photo"] and not identity["photo_history_supported_only"]
        assert metrics["evaluator_sha256"] == evaluation["source_sha256"]["eval/evaluate.py"]
        assert metrics["gt_sha256"] == evaluation["manual_shield4"]["gt_sha256"]
        record = {"run": name, "identity": identity, "evaluation": metrics,
                  "resources": load_json(folder / "run.json"), "status": status,
                  "trajectory_sha256": sha(folder / "trajectory.tum"),
                  "passed": metrics["ate_rmse_m"] < threshold,
                  "manual_evaluation": manual_evaluation(folder)}
        if (folder / "photo.csv").exists():
            with (folder / "photo.csv").open(newline="") as stream:
                photo_rows = list(csv.DictReader(stream))
            record["photo_summary"] = {
                "mean_valid_rows": float(np.mean([int(row["valid"]) for row in photo_rows])),
                "sigma_final": float(photo_rows[-1]["sigma"]),
                "sigma_frozen_frames": sum(int(row["frozen"]) for row in photo_rows),
            }
        if arm in ("A", "B"):
            original = ORIGINAL_A if arm == "A" else ORIGINAL_B
            record["original_p3_trajectory_sha256"] = sha(original / "trajectory.tum")
            record["exact_original_trajectory_match"] = record["trajectory_sha256"] == record["original_p3_trajectory_sha256"]
            assert record["exact_original_trajectory_match"], "Instrumentation/build changed arm " + arm
        else:
            assert identity["photo_history_supported_only"]
        if arm in ("A", "C"):
            diagnostics[arm] = time_summary(arm, folder)
        arms[arm] = record
    a, c = arms["A"]["evaluation"]["ate_rmse_m"], arms["C"]["evaluation"]["ate_rmse_m"]
    for field in ("bag_sha256", "config_sha256", "binary_sha256", "lio_library_sha256", "p3_source_sha256"):
        assert all(record["identity"][field] == arms["A"]["identity"][field] for record in arms.values()), field
    assert diagnostics["C"]["counts"]["fallback_deskew_points"] == 0
    assert diagnostics["C"]["counts"]["photo_points_retained"] == diagnostics["C"]["counts"]["photo_points_inside_history"]
    with (OUT / "shield4_time_a.csv").open(newline="") as stream:
        a_rows = list(csv.DictReader(stream))
    with (OUT / "shield4_time_c.csv").open(newline="") as stream:
        c_rows = list(csv.DictReader(stream))
    timing_fields = ("frame", "scan_start", "scan_end_geometry", "history_first", "history_last", "history_states",
                     "frame_supported", "geometry_points", "photo_points_total", "photo_points_inside_history",
                     "photo_points_outside_history", "min_photo_offset_s", "max_photo_offset_s", "max_photo_timestamp")
    identical_support = len(a_rows) == len(c_rows) and all(
        all(a_row[key] == c_row[key] for key in timing_fields) for a_row, c_row in zip(a_rows, c_rows))
    c_filter_valid = all(int(row["photo_points_retained"]) == int(row["photo_points_inside_history"])
                         and int(row["photo_points_dropped"]) == int(row["photo_points_outside_history"])
                         and int(row["fallback_deskew_points"]) == 0 for row in c_rows)
    assert c_filter_valid
    write("shield4_time_diagnostics.json", {
        "definition": "All raw accepted photo points are counted before C filtering; inside is inclusive propagated-history bounds with a positive history span. Actual fallback counts retained points whose original deskew interpolation did not run. Frames without a usable history are skipped, not fallback-rasterized.",
        "runs": diagnostics,
    })
    write("shield4_ablation.json", {"base_commit": BASE, "audit_package_commit": "71822c76fb3b2a4d4ddad69c536862ede24388a3",
        "protocol": "Shield4 complete bag, offline 32T, sequential A/B/C, no tuning",
        "threshold_m_exclusive": threshold, "threshold_source": "20% of full GT sampled path length",
        "arms": arms, "a_to_c_ate_change_m": c-a, "a_to_c_ate_relative_change": (c-a)/a,
        "a_to_c_pass_changed": arms["A"]["passed"] != arms["C"]["passed"],
        "interpretation_limit": "C changes which photo inputs enter raster/IDW/features; it isolates sensitivity to unsupported timestamps, not the causal contribution of cubemap vs IDW vs intensity individually."})
    locked = ["src/super_lio/include/intensity/cube_projector.hpp", "src/super_lio/include/intensity/cube_image.hpp",
              "src/super_lio/src/intensity/cube_image.cpp", "src/super_lio/src/intensity/spherical_image.cpp",
              "src/super_lio/include/intensity/spherical_image.hpp", "src/super_lio/include/intensity/information_budget.hpp",
              "src/super_lio/include/intensity/intensity_representation.hpp",
              "eval/evaluate.py", "eval/ntu_author.py", "tools/cube_lio/config/photo.yaml", "tools/cube_lio/config/shield4.yaml"]
    locked += subprocess.check_output(["git", "ls-tree", "-r", "--name-only", BASE, "--",
        "src/super_lio/src/lio", "src/super_lio/include/lio", "src/super_lio/src/ros", "src/super_lio/include/ros",
        "src/super_lio/src/intensity/coin", "src/super_lio/include/intensity/coin"], cwd=ROOT, text=True).splitlines()
    invariants = {}
    for path in locked:
        original = subprocess.check_output(["git", "show", BASE+":"+path], cwd=ROOT)
        invariants[path] = {"base_sha256": hashlib.sha256(original).hexdigest(), "current_sha256": sha(ROOT/path),
                            "unchanged": original == (ROOT/path).read_bytes()}
    assert all(record["unchanged"] for record in invariants.values())
    historical_sources = {}
    for label, folder in (("A", ORIGINAL_A), ("B", ORIGINAL_B)):
        original_identity = load_json(folder / "identity.json")
        comparisons = {}
        for path, stored_sha in original_identity["p3_source_sha256"].items():
            base_bytes = subprocess.check_output(["git", "show", BASE+":"+path], cwd=ROOT)
            comparisons[path] = stored_sha == hashlib.sha256(base_bytes).hexdigest()
        assert all(comparisons.values()), "Archived source hashes do not match f306 arm " + label
        historical_sources[label] = {"recorded_head": original_identity["source_head"],
                                     "recorded_source_hashes_match_f306": comparisons}
    write("behavior_invariance.json", {"locked_sources": invariants,
          "historical_source_verification": historical_sources,
          "original_binary_sha256": sha(ROOT / "runtime/p3r_original_build/cube_offline_node"),
          "original_lio_library_sha256": sha(ROOT / "runtime/p3r_original_build/liblio.so"),
          "audit_source_sha256": {path: sha(ROOT/path) for path in (
              "src/super_lio/include/intensity/photo_observation.hpp",
              "src/super_lio/src/intensity/photo_observation.cpp", "tools/offline/run.py",
              "tools/offline/audit_p3r_attribution.py")},
          "A_trajectory_sha_identical_to_p3": arms["A"]["exact_original_trajectory_match"],
          "B_trajectory_sha_identical_to_p3": arms["B"]["exact_original_trajectory_match"],
          "A_C_time_support_metadata_identical_per_scan": identical_support,
          "time_support_fields_compared": list(timing_fields),
          "C_filter_valid_on_every_scan": c_filter_valid,
          "C_actual_fallback_points": diagnostics["C"]["counts"]["fallback_deskew_points"]})
    print(json.dumps({arm: {"ate_rmse_m": row["evaluation"]["ate_rmse_m"], "pass": row["passed"],
                           "sha": row["trajectory_sha256"]} for arm, row in arms.items()}, indent=2))
    print(json.dumps({arm: {"unsupported_pct": row["unsupported_point_percentage_raw"],
                           "fallback_points": row["counts"]["fallback_deskew_points"]} for arm, row in diagnostics.items()}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", choices=("evaluator", "archive"))
    arguments = parser.parse_args()
    evaluator_audit() if arguments.phase == "evaluator" else archive()
