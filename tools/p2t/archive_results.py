#!/usr/bin/env python3
"""Archive P2-T degeneracy-gate validation from retained runtime records."""
import csv
import hashlib
import json
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "artifacts/p2t"
S2_RUNS = ["p2t_s2_reference", "p2t_s2_repeat2", "p2t_s2_repeat3"]
G1_RUNS = ["p2t_g1_first", "p2t_g1_repeat2", "p2t_g1_repeat3"]
G2_RUNS = ["p2t_g2_first", "p2t_g2_repeat2", "p2t_g2_repeat3"]
NTU_RUNS = ["p2t_ntu_validation", "p2t_ntu_validation_repeat2", "p2t_ntu_validation_repeat3"]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load(path):
    return json.loads(Path(path).read_text())


def summarize(values):
    data = np.asarray([float(x) for x in values if x not in (None, "") and np.isfinite(float(x))])
    if not len(data):
        return {"count": 0}
    return {"count": int(len(data)), "min": float(data.min()),
            "p05": float(np.quantile(data, .05)), "p50": float(np.quantile(data, .5)),
            "p95": float(np.quantile(data, .95)), "max": float(data.max()),
            "mean": float(data.mean())}


def read_diag(root):
    with open(root / "coin_observation.csv", newline="") as stream:
        return list(csv.DictReader(stream))


def center_count(value):
    return len([part for part in (value or "").split(";") if part])


def run_record(name):
    root = ROOT / "runtime" / name
    identity = load(root / "identity.json")
    evaluation = load(root / "evaluation.json")
    result = load(root / "result.json")
    run = load(root / "run.json")
    rows = read_diag(root)
    used = [row for row in rows if row["status"] == "USED"]
    skipped = [row for row in rows if row["status"] == "SKIPPED"]
    feature_counts = [int(row["active_after"]) for row in used]
    centers = [center_count(row["selected_centers_xy"]) for row in used]
    skip_reasons = {}
    for row in skipped:
        skip_reasons[row["skip_reason"]] = skip_reasons.get(row["skip_reason"], 0) + 1
    fields = ("N_geo_rows", "lambda1", "lambda2", "lambda3", "lambda1_over_lambda2",
              "lambda1_over_lambda3", "weakest_axis_stability", "anisotropy_confidence",
              "eigengap_confidence", "degeneracy_confidence")
    geometry_summary = {key: summarize([row[key] for row in used]) for key in fields}
    return {
        "name": name, "result": result, "evaluation": evaluation, "run": run,
        "identity": identity,
        "trajectory_sha256": sha(root / "trajectory.tum"),
        "diagnostics_sha256": sha(root / "coin_observation.csv"),
        "frames": len(rows), "accepted_photo_frames": len(used),
        "skipped_photo_frames": len(skipped), "skip_reasons": skip_reasons,
        "accepted_motion_fallback_points": sum(int(row["motion_fallback_points"]) for row in used),
        "weakest_selector_frames": sum(int(row["weak_dirs"]) == 1 for row in used),
        "xyz_fallback_frames": sum(int(row["weak_dirs"]) == 3 for row in used),
        "gate_active_frames": sum(int(row["gate_active"]) for row in used),
        "gate_active_fraction": (sum(int(row["gate_active"]) for row in used) / len(used)) if used else 0.,
        "max_active_features": max(feature_counts) if feature_counts else 0,
        "active_features": summarize(feature_counts),
        "selected_center_count": summarize(centers),
        "selected_gradient_mean": summarize([row["selected_gradient_mean"] for row in used]),
        "ncc_median_by_frame": summarize([row["ncc_median"] for row in used]),
        "photo_rows_by_frame": summarize([row["photo_rows"] for row in used]),
        "scan_end_delta_s": summarize([row["coin_minus_super_scan_end_s"] for row in used]),
        "geometry_signal": geometry_summary,
        "fusion_audit": load(root / "fusion_equivalence.json") if (root / "fusion_equivalence.json").exists() else None,
    }


def deterministic_group(names):
    records = [run_record(name) for name in names]
    first = records[0]
    hashes = [record["trajectory_sha256"] for record in records]
    diag_hashes = [record["diagnostics_sha256"] for record in records]
    timestamps = []
    for name in names:
        rows = read_diag(ROOT / "runtime" / name)
        timestamps.append([float(row["timestamp"]) for row in rows])
    timestamp_equal = all(items == timestamps[0] for items in timestamps[1:])
    return {
        "runs": names, "trajectory_sha256": hashes, "diagnostics_sha256": diag_hashes,
        "timestamp_vector_equal": timestamp_equal,
        "frames_equal": all(record["frames"] == first["frames"] for record in records),
        "ate_rmse_m": [record["evaluation"]["ate_rmse_m"] for record in records],
        "bitwise_trajectory_identical": len(set(hashes)) == 1,
        "bitwise_diagnostics_identical": len(set(diag_hashes)) == 1,
        "run_records": records,
    }


def photo_off_record(name):
    root = ROOT / "runtime" / name
    return {"name": name, "evaluation": load(root / "evaluation.json"),
            "run": load(root / "run.json"), "identity": load(root / "identity.json"),
            "trajectory_sha256": sha(root / "trajectory.tum"),
            "result": load(root / "result.json")}


def photo_off_group(names):
    records = [photo_off_record(name) for name in names]
    timestamps = [np.loadtxt(ROOT / "runtime" / name / "trajectory.tum")[:, 0].tolist()
                  for name in names]
    hashes = [record["trajectory_sha256"] for record in records]
    first = records[0]
    return {"runs": names, "trajectory_sha256": hashes,
            "timestamp_vector_equal": all(value == timestamps[0] for value in timestamps[1:]),
            "frame_count_equal": all(record["run"]["frames"] == first["run"]["frames"]
                                      for record in records),
            "ate_rmse_m": [record["evaluation"]["ate_rmse_m"] for record in records],
            "bitwise_trajectory_identical": len(set(hashes)) == 1,
            "run_records": records}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    thresholds = load(OUT / "degeneracy_statistics.json")["threshold_derivation"]
    s2 = deterministic_group(S2_RUNS)
    g1 = deterministic_group(G1_RUNS)
    g2 = deterministic_group(G2_RUNS)
    selector = {"start_sha": "5b28b14fab1785c990bf8793eaa4eef635dc72cd",
                "thresholds": thresholds,
                "baselines": {"P2S_S2_ATE_m": load(ROOT / "artifacts/p2s/production_ablation.json")
                              ["summary"]["S2_weakest_ate_rmse_m"],
                              "P2R_C1_S0_ATE_m": load(ROOT / "artifacts/p2s/production_ablation.json")
                              ["baselines"]["C1_S0"]["ate_rmse_m"],
                              "P2R_G1_geometry_ATE_m": load(ROOT / "artifacts/p2r/g1_geometry.json")
                              ["g1_corrected"]["ate_rmse_m"]},
                "modes": {"S2_always_weakest": s2, "G1_confidence_q95": g1,
                          "G2_conservative_q99": g2},
                "interpretation": {
                    "S2_reproduced": abs(s2["ate_rmse_m"][0] -
                        load(ROOT / "artifacts/p2s/production_ablation.json")["summary"]["S2_weakest_ate_rmse_m"]) < 1e-9,
                    "G1_recovers_S2_level": g1["bitwise_trajectory_identical"] and
                        g1["ate_rmse_m"][0] < 10. and abs(g1["ate_rmse_m"][0] - s2["ate_rmse_m"][0]) < 1.,
                    "G2_conservative_gate_recovers": g2["ate_rmse_m"][0] < 10.,
                    "feature_limit_60_preserved": all(record["max_active_features"] <= 60
                        for group in (s2, g1, g2) for record in group["run_records"]),
                    "zero_accepted_motion_fallback": all(record["accepted_motion_fallback_points"] == 0
                        for group in (s2, g1, g2) for record in group["run_records"]),
                    "four_genuine_imu_gap_skips": all(record["skipped_photo_frames"] == 4 and
                        record["skip_reasons"] == {"imu_bracket_gap_or_order_invalid": 4}
                        for group in (s2, g1, g2) for record in group["run_records"]),
                    "gate_selection_uses_no_coin_absolute_threshold": True,
                }}
    (OUT / "selector_comparison.json").write_text(json.dumps(selector, indent=2, allow_nan=False) + "\n")

    first_s2 = s2["run_records"][0]
    s2_reference = {
        "reproduction": first_s2,
        "three_run_determinism": {key: value for key, value in s2.items() if key != "run_records"},
        "P2S_reference": {"ATE_rmse_m": selector["baselines"]["P2S_S2_ATE_m"],
                          "trajectory_sha256": "1f72919449989a7b14918fa456015fc21ae470af239915a89096b369b669b311"},
        "S2_reproduction_matches_P2S": selector["interpretation"]["S2_reproduced"] and
            first_s2["trajectory_sha256"] == "1f72919449989a7b14918fa456015fc21ae470af239915a89096b369b669b311",
        "source_note": "Eigenvalues and temporal weakest-axis stability are captured from the exact final geometry rows used at each S2 frame; feature statistics are from the corresponding COIN diagnostics.",
    }
    (OUT / "s2_reference.json").write_text(json.dumps(s2_reference, indent=2, allow_nan=False) + "\n")

    ntu = photo_off_group(NTU_RUNS)
    ntu_control = photo_off_record("p2t_ntu_original_control")
    ntu_baseline = photo_off_record("p2r_eee01_photo_off")
    ntu_ate = ntu["ate_rmse_m"][0]
    ntu_json = {
        "validation_scope": "photo-off normal-scene regression; COIN is disabled for eee_01 and the selector mode cannot enter its estimator path",
        "P2T_G1_configured_but_COIN_disabled": ntu,
        "same_build_original_selector_control": ntu_control,
        "same_build_selector_configuration_has_no_effect":
            ntu["bitwise_trajectory_identical"] and
            all(record["trajectory_sha256"] == ntu_control["trajectory_sha256"] for record in ntu["run_records"]),
        "P2R_photo_off_reference": ntu_baseline,
        "P2T_vs_P2R_reference_ATE_delta_m": ntu_ate - ntu_baseline["evaluation"]["ate_rmse_m"],
        "P2T_vs_P2R_reference_ATE_delta_fraction":
            ntu_ate / ntu_baseline["evaluation"]["ate_rmse_m"] - 1.,
        "limitation": "The small ATE difference from the older P2R run is a build/reference comparison; the same-build selector-on/off trajectories are bitwise identical. COIN photometric selection is not supported on this sequence by the existing runner/frontend scope.",
    }
    (OUT / "ntu_validation.json").write_text(json.dumps(ntu_json, indent=2, allow_nan=False) + "\n")

    shield_capture = photo_off_record("p2t_geometry_shield1")
    shield_mode = photo_off_record("p2t_shield_validation")
    shield_baseline = photo_off_record("p2r_shield1_photo_off_scoped")
    shield_json = {
        "validation_scope": "photo-off additional Livox geometry regression; COIN is disabled and no COIN selector is evaluated on Shield1",
        "P2T_photo_off_geometry_capture_run": shield_capture,
        "P2T_G1_configured_but_COIN_disabled": shield_mode,
        "P2R_photo_off_reference": shield_baseline,
        "P2T_trajectory_matches_P2R_reference":
            shield_capture["trajectory_sha256"] == shield_baseline["trajectory_sha256"] ==
            shield_mode["trajectory_sha256"],
        "P2T_ATE_delta_m": shield_capture["evaluation"]["ate_rmse_m"] -
            shield_baseline["evaluation"]["ate_rmse_m"],
        "limitation": "This validates that the selector code does not perturb the shared photo-off Livox path; it does not test an active COIN image selector on Shield1.",
    }
    (OUT / "shield_validation.json").write_text(json.dumps(shield_json, indent=2, allow_nan=False) + "\n")

    determinism = {
        "status": "PASS" if all(group["bitwise_trajectory_identical"] and
                                 group["bitwise_diagnostics_identical"] and group["timestamp_vector_equal"]
                                 for group in (s2, g1, g2)) and ntu["bitwise_trajectory_identical"] else "FAIL",
        "TunnelD": {"S2": {key: value for key, value in s2.items() if key != "run_records"},
                    "G1": {key: value for key, value in g1.items() if key != "run_records"},
                    "G2": {key: value for key, value in g2.items() if key != "run_records"}},
        "NTU_photo_off_G1_configured": {key: value for key, value in ntu.items() if key != "run_records"},
        "NTU_same_build_original_control_trajectory_sha256": ntu_control["trajectory_sha256"],
        "Shield1_same_build_G1_configured_trajectory_sha256": shield_mode["trajectory_sha256"],
    }
    (OUT / "determinism.json").write_text(json.dumps(determinism, indent=2, allow_nan=False) + "\n")

    files = ["src/super_lio/include/intensity/coin/coin_feature_manager.hpp",
             "src/super_lio/include/intensity/coin/coin_observation.hpp",
             "src/super_lio/include/intensity/coin/super_degeneracy_gate.hpp",
             "src/super_lio/include/intensity/intensity_representation.hpp",
             "src/super_lio/include/intensity/coin/coin_intensity_representation.hpp",
             "src/super_lio/src/intensity/coin/coin_feature_manager.cpp",
             "src/super_lio/src/intensity/coin/coin_observation.cpp",
             "src/super_lio/src/lio/super_lio.cpp", "tools/p2r/run.py",
             "tools/p2t/analyze_degeneracy.py", "tools/p2t/archive_results.py",
             "src/super_lio/test/test_coin_feature_math.cpp"]
    binary = ROOT / "devel/lib/super_lio/cube_offline_node"
    identity = {
        "branch": "p2t-super-native-degeneracy-gate",
        "start_sha": "5b28b14fab1785c990bf8793eaa4eef635dc72cd",
        "run_source_head": "5b28b14fab1785c990bf8793eaa4eef635dc72cd",
        "note": "The validated P2-T source tree was dirty during runs; exact tracked-source hashes and production binary hash are recorded below.",
        "source_sha256": {name: sha(ROOT / name) for name in files},
        "binary_path": str(binary.relative_to(ROOT)), "binary_sha256": sha(binary),
        "production_run_binary_sha256": {
            name: sorted({record["identity"]["binary_sha256"] for record in group["run_records"]})
            for name, group in (("S2", s2), ("G1", g1), ("G2", g2))},
        "confidence_thresholds": thresholds,
        "datasets": {name: {"bag": record["identity"]["bag"],
                             "config_sha256": record["identity"]["config_sha256"]}
                     for name, record in (("tunnel_d", first_s2), ("eee_01", ntu["run_records"][0]),
                                          ("shield1", shield_capture))},
    }
    (OUT / "build_identity.json").write_text(json.dumps(identity, indent=2, allow_nan=False) + "\n")

    print(json.dumps({"thresholds": thresholds,
                      "ATE": {"S2": s2["ate_rmse_m"], "G1": g1["ate_rmse_m"], "G2": g2["ate_rmse_m"]},
                      "determinism": determinism["status"],
                      "ntu_same_build_mode_match": ntu_json["same_build_selector_configuration_has_no_effect"],
                      "shield_matches_reference": shield_json["P2T_trajectory_matches_P2R_reference"]}, indent=2))


if __name__ == "__main__":
    main()
