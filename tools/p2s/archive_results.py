#!/usr/bin/env python3
"""Archive P2-S run identities, ablation results, and determinism checks."""
import csv
import hashlib
import json
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "artifacts/p2s"


def load(path):
    return json.loads(Path(path).read_text())


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(name, data):
    (OUT / name).write_text(json.dumps(data, indent=2, allow_nan=False) + "\n")


def run_record(name):
    root = ROOT / "runtime" / name
    rows = list(csv.DictReader(open(root / "coin_observation.csv", newline="")))
    used = [row for row in rows if row["status"] == "USED"]
    skipped = [row for row in rows if row["status"] == "SKIPPED"]
    result = load(root / "result.json")
    evaluation = load(root / "evaluation.json")
    run = load(root / "run.json")
    identity = load(root / "identity.json")
    return {
        "name": name, "result": result, "evaluation": evaluation, "run": run,
        "identity": identity, "trajectory_sha256": sha(root / "trajectory.tum"),
        "diagnostics_sha256": sha(root / "coin_observation.csv"),
        "frames": len(rows), "accepted_photo_frames": len(used),
        "skipped_photo_frames": len(skipped),
        "skip_reasons": {reason: sum(row["skip_reason"] == reason for row in skipped)
                         for reason in sorted({row["skip_reason"] for row in skipped})},
        "accepted_motion_fallback_points": sum(int(row["motion_fallback_points"]) for row in used),
        "selector_modes": sorted({row["selector_mode"] for row in rows}),
        "max_active_feature_count": max(int(row["active_after"]) for row in rows),
        "scan_end_delta_s_min": min(float(row["coin_minus_super_scan_end_s"]) for row in used),
        "scan_end_delta_s_median": float(np.median([float(row["coin_minus_super_scan_end_s"]) for row in used])),
        "scan_end_delta_s_max": max(float(row["coin_minus_super_scan_end_s"]) for row in used),
        "fusion_audit": load(root / "fusion_equivalence.json") if (root / "fusion_equivalence.json").exists() else None,
    }


def compare(records):
    base = np.loadtxt(ROOT / "runtime" / records[0]["name"] / "trajectory.tum")
    result = []
    for record in records[1:]:
        current = np.loadtxt(ROOT / "runtime" / record["name"] / "trajectory.tum")
        result.append({
            "run": record["name"],
            "timestamp_vector_equal": bool(np.array_equal(base[:, 0], current[:, 0])),
            "frame_count_equal": base.shape[0] == current.shape[0],
            "trajectory_sha256_equal": record["trajectory_sha256"] == records[0]["trajectory_sha256"],
            "max_translation_difference_m": float(np.linalg.norm(base[:, 1:4] - current[:, 1:4], axis=1).max()),
            "max_quaternion_chord_difference": float(np.linalg.norm(base[:, 4:8] - current[:, 4:8], axis=1).max()),
            "ATE_difference_m": abs(record["evaluation"]["ate_rmse_m"] - records[0]["evaluation"]["ate_rmse_m"]),
        })
    return result


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    g1 = load(ROOT / "artifacts/p2r/g1_geometry.json")["g1_corrected"]
    c1_runs = load(ROOT / "artifacts/p2r/c1_coin_runs.json")["validation_runs"]
    ref_p2r = load(ROOT / "artifacts/p2r/reference_identity.json")
    geometry = load(OUT / "geometry_signal_summary.json")
    official_frequency = {"frames": geometry["official"]["frames"],
                          "weak_frames": geometry["official"]["weak_frames"],
                          "weak_fraction": geometry["official"]["weak_frames"] / geometry["official"]["frames"]}
    super_frequency = {"frames": geometry["super"]["frames"],
                       "weak_frames": geometry["super"]["weak_frames"],
                       "weak_fraction": geometry["super"]["weak_frames"] / geometry["super"]["frames"]}
    raw = {
        "S1_gradient": [run_record("p2s_production_gradient"), run_record("p2s_production_gradient_2"),
                        run_record("p2s_production_gradient_3")],
        "S2_weakest": [run_record("p2s_production_weakest"), run_record("p2s_production_weakest_2"),
                       run_record("p2s_production_weakest_3")],
        "S3_normalized": [run_record("p2s_production_normalized")],
    }
    baseline_c1 = {"name": "P2-R C1/S0 baseline", "ate_rmse_m": c1_runs[0]["evaluation"]["ate_rmse_m"],
                   "trajectory_sha256": c1_runs[0]["trajectory_sha256"], "runs": 3,
                   "accepted_photo_frames": c1_runs[0]["accepted_photo_frames"],
                   "skipped_photo_frames": c1_runs[0]["skipped_photo_frames"],
                   "accepted_motion_fallback_points": c1_runs[0]["accepted_motion_fallback_points"]}
    material_cutoff = baseline_c1["ate_rmse_m"] * 0.90
    ablations = {}
    determinism = {"baseline_C1_S0": {"runs": 3, "trajectory_hashes": [x["trajectory_sha256"] for x in c1_runs],
                                       "repeat_validation_performed": True,
                                       "bitwise_identical_across_repeats": len({x["trajectory_sha256"] for x in c1_runs}) == 1,
                                       "bitwise_diagnostics_identical_across_repeats": None}}
    for mode, records in raw.items():
        ate = records[0]["evaluation"]["ate_rmse_m"]
        comparisons = compare(records) if len(records) > 1 else []
        improvement = (baseline_c1["ate_rmse_m"] - ate) / baseline_c1["ate_rmse_m"]
        ablations[mode] = {
            "selector": {"S1_gradient": "pure deterministic image gradient",
                         "S2_weakest": "always smallest eigenvector of Super H_t^T H_t",
                         "S3_normalized": "COIN contribution/N < official-derived 0.01609598"}[mode],
            "runs": records, "first_run_ate_rmse_m": ate,
            "improvement_vs_C1_fraction": improvement,
            "repeats_triggered_by_predeclared_gate": ate <= material_cutoff,
            "material_repeat_gate_ate_m": material_cutoff,
            "determinism_comparisons": comparisons,
        }
        determinism[mode] = {
            "runs": len(records), "trajectory_sha256": [x["trajectory_sha256"] for x in records],
            "diagnostics_sha256": [x["diagnostics_sha256"] for x in records],
            "comparisons_to_first": comparisons,
            "repeat_validation_performed": len(records) > 1,
            "bitwise_identical_across_repeats": (len({x["trajectory_sha256"] for x in records}) == 1
                                                   if len(records) > 1 else None),
            "bitwise_diagnostics_identical_across_repeats": (len({x["diagnostics_sha256"] for x in records}) == 1
                                                               if len(records) > 1 else None),
        }

    c1 = baseline_c1["ate_rmse_m"]
    weakest_ate = raw["S2_weakest"][0]["evaluation"]["ate_rmse_m"]
    write("production_ablation.json", {
        "scope": "only COIN complementary feature direction policy varied; Super geometry, ESKF, timing, image, NCC, patch/lifetime, residual, Jacobian, scale and variance frozen",
        "GT_usage": "evaluator reports ATE after selection/run; no GT value or trajectory was used to select a mode or threshold",
        "production_repeat_gate": {"rule": "repeat only if first run improves C1 by at least 10% or reaches <=10 m",
                                   "C1_S0_ate_rmse_m": c1, "10_percent_improvement_cutoff_m": material_cutoff},
        "baselines": {"G0_historical_geometry_ate_rmse_m": ref_p2r["g0_historical_geometry_ate_rmse_m"],
                      "G1_corrected_geometry_ate_rmse_m": g1["ate_rmse_m"], "C1_S0": baseline_c1,
                      "official_COiN_oracle_ate_rmse_m": ref_p2r["official_coin_oracle_ate_rmse_m"]},
        "selector_ablation": ablations,
        "summary": {"S1_gradient_ate_rmse_m": raw["S1_gradient"][0]["evaluation"]["ate_rmse_m"],
                    "S2_weakest_ate_rmse_m": weakest_ate,
                    "S3_normalized_ate_rmse_m": raw["S3_normalized"][0]["evaluation"]["ate_rmse_m"],
                    "S2_improvement_vs_C1_m": c1 - weakest_ate,
                    "S2_ate_classification": "MECHANISM_REPRODUCED" if weakest_ate <= 10 else "PARTIAL_RESCUE"},
        "fusion_gate": {"S1_first_run": raw["S1_gradient"][0]["fusion_audit"]["status"],
                        "other_modes": "same CoinObservation production H/r accumulator; inherited P2-R equivalence test, direction-policy change only"},
        "timing_gate": {"accepted_fallback_points_all_runs_zero": all(r["accepted_motion_fallback_points"] == 0
                                                                         for records in raw.values() for r in records),
                         "all_runs_skip_four_genuine_imu_gaps": all(r["skipped_photo_frames"] == 4 and
                                                                      r["skip_reasons"] == {"imu_bracket_gap_or_order_invalid": 4}
                                                                      for records in raw.values() for r in records)},
        "feature_cap_gate": {"cap": 60,
                             "all_production_runs_respect_cap": all(r["max_active_feature_count"] <= 60
                                                                     for records in raw.values() for r in records),
                             "maximum_active_features_by_run": {r["name"]: r["max_active_feature_count"]
                                                                for records in raw.values() for r in records}},
        "weak_direction_frequency": {"official": official_frequency, "Super_G1_shadow_S0": super_frequency,
                                      "Super_C1_S0": {"frames": 1176, "weak_frames": 60, "fallback_xyz_frames": 1116}},
    })
    write("determinism.json", {
        "status": "PASS" if all(v.get("bitwise_identical_across_repeats", True) in (True, None)
                                  for v in determinism.values()) else "FAIL",
        "production_repeatability_measured_for": [mode for mode, value in determinism.items()
                                                   if value.get("repeat_validation_performed")],
        "selector_production_repeat_gate": "S1 and S2 each triggered three-run validation; S3 remained one run because it improved C1 by less than 10% and stayed above 10 m",
        "runs": determinism,
        "fixed_G1_shadow_trajectory_sha256": sha(ROOT / "runtime/p2r_g1_rebased_history/trajectory.tum"),
        "all_shadow_modes_preserve_G1": all(sha(ROOT / "runtime" / name / "trajectory.tum") ==
                                             sha(ROOT / "runtime/p2r_g1_rebased_history/trajectory.tum")
                                             for name in ("p2s_shadow_s0_final", "p2s_shadow_gradient_final",
                                                          "p2s_shadow_weakest_final", "p2s_shadow_normalized_final")),
    })
    old = ROOT / "runtime/p2r_g1_corrected_geometry/trajectory.tum"
    new = ROOT / "runtime/p2r_g1_rebased_history/trajectory.tum"
    a = np.loadtxt(old); b = np.loadtxt(new)
    old_id = load(ROOT / "runtime/p2r_g1_corrected_geometry/identity.json")
    new_id = load(ROOT / "runtime/p2r_g1_rebased_history/identity.json")
    write("history_rebase_audit.json", {
        "classification": "AUDIT_ONLY_UNRESOLVED",
        "H0": {"description": "P2-R current retained and rigidly rebased overlapping Super propagation history",
               "ATE_rmse_m": load(ROOT / "runtime/p2r_g1_rebased_history/evaluation.json")["ate_rmse_m"],
               "path_length_m": float(np.linalg.norm(np.diff(b[:, 1:4], axis=0), axis=1).sum()),
               "trajectory_sha256": sha(new), "binary_sha256": new_id["binary_sha256"]},
        "H1_audit_proxy": {"description": "earlier corrected-end implementation cleared history at corrected start; unsupported early acquisition points used the historical raw-point deskew path; this is not the requested corrected-IMU reconstruction",
                           "ATE_rmse_m": load(ROOT / "runtime/p2r_g1_corrected_geometry/evaluation.json")["ate_rmse_m"],
                           "path_length_m": float(np.linalg.norm(np.diff(a[:, 1:4], axis=0), axis=1).sum()),
                           "trajectory_sha256": sha(old), "binary_sha256": old_id["binary_sha256"]},
        "timestamp_vectors_equal": bool(np.array_equal(a[:, 0], b[:, 0])),
        "frames": len(a), "max_translation_difference_m": float(np.linalg.norm(a[:, 1:4] - b[:, 1:4], axis=1).max()),
        "median_translation_difference_m": float(np.median(np.linalg.norm(a[:, 1:4] - b[:, 1:4], axis=1))),
        "p95_translation_difference_m": float(np.quantile(np.linalg.norm(a[:, 1:4] - b[:, 1:4], axis=1), .95)),
        "max_quaternion_chord_difference": float(np.linalg.norm(a[:, 4:8] - b[:, 4:8], axis=1).max()),
        "ate_difference_m": load(ROOT / "runtime/p2r_g1_rebased_history/evaluation.json")["ate_rmse_m"] -
                           load(ROOT / "runtime/p2r_g1_corrected_geometry/evaluation.json")["ate_rmse_m"],
        "interpretation": "The history policy is associated with a large trajectory change (+13.092 m ATE and up to 38.301 m translation difference), but the comparison does not isolate a scientifically valid reconstructed H1. Source/binary hashes differ and the old path lacked acquisition support. A safe Super-native raw-IMU reconstruction from a corrected anchor was not implemented; do not assign the G1 degradation solely to the rebase.",
    })
    write("reference_identity.json", {
        "phase": "P2-S geometry degeneracy interface audit",
        "start_branch": "p2r-super-coin-time-consistency",
        "start_sha": "9e03a7c48cd7631a7c5f367d36d012b6730b038c",
        "official_coin_source_sha": "76729cc4feb3649cbd79d28f82d9f62a2c82889b",
        "tunneld_bag_path": ref_p2r["tunneld_bag"]["path"],
        "tunneld_bag_sha256": ref_p2r["tunneld_bag"]["sha256"],
        "gt_sha256": ref_p2r["gt_sha256"], "evaluator_sha256": ref_p2r["evaluator_sha256"],
        "official_geometry_trace": {"path": "runtime/p2/coin_geometry_trace.bin", "sha256": sha(ROOT / "runtime/p2/coin_geometry_trace.bin")},
        "official_feature_frame_map": {"path": "runtime/p2/feature_frames_real.csv", "sha256": sha(ROOT / "runtime/p2/feature_frames_real.csv")},
        "super_geometry_trace": {"path": "runtime/p2s_shadow_s0_final/geometry_rows.bin", "sha256": sha(ROOT / "runtime/p2s_shadow_s0_final/geometry_rows.bin")},
        "super_g1_trajectory_sha256": sha(ROOT / "runtime/p2r_g1_rebased_history/trajectory.tum"),
        "official_weak_direction_frequency": official_frequency,
        "super_g1_shadow_weak_direction_frequency": super_frequency,
        "normalized_threshold_predeclared_before_super_selector_runs": geometry["normalized_threshold_derivation"],
    })
    binary = ROOT / "devel/lib/super_lio/cube_offline_node"
    files = ["src/super_lio/CMakeLists.txt", "src/super_lio/include/lio/super_lio.h",
             "src/super_lio/include/intensity/coin/coin_feature_manager.hpp",
             "src/super_lio/include/intensity/coin/coin_observation.hpp",
             "src/super_lio/src/intensity/coin/coin_feature_manager.cpp",
             "src/super_lio/src/intensity/coin/coin_observation.cpp",
             "src/super_lio/src/lio/super_lio.cpp", "tools/p2r/run.py",
             "tools/p2s/analyze_geometry.py", "tools/p2s/analyze_selectors.py",
             "tools/p2s/archive_results.py", "src/super_lio/test/test_coin_feature_math.cpp"]
    write("build_identity.json", {
        "branch": "p2s-geometry-degeneracy-interface-audit",
        "start_sha": "9e03a7c48cd7631a7c5f367d36d012b6730b038c",
        "run_source_head": "9e03a7c48cd7631a7c5f367d36d012b6730b038c",
        "note": "The source files were dirty during P2-S runs; exact hashes below identify the validated production tree. After production, only the unit-test fixture was extended for the explicit 60-feature cap and rebuilt; cube_offline_node SHA remained unchanged from every production run.",
        "source_sha256": {x: sha(ROOT / x) for x in files},
        "binary_path": "devel/lib/super_lio/cube_offline_node",
        "binary_sha256": sha(binary),
        "production_run_binary_sha256": {mode: [x["identity"]["binary_sha256"] for x in records]
                                          for mode, records in raw.items()},
        "config_sha256": raw["S1_gradient"][0]["identity"]["config_sha256"],
        "bag_sha256": ref_p2r["tunneld_bag"]["sha256"], "gt_sha256": ref_p2r["gt_sha256"],
        "evaluator_sha256": ref_p2r["evaluator_sha256"],
    })
    print(json.dumps({"ablation_ates": {k: v["first_run_ate_rmse_m"] for k, v in ablations.items()},
                      "S2_material_cutoff_m": material_cutoff,
                      "determinism": {k: v.get("bitwise_identical_across_repeats") for k, v in determinism.items()},
                      "history": "AUDIT_ONLY_UNRESOLVED"}))


if __name__ == "__main__":
    main()
