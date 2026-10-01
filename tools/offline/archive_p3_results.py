#!/usr/bin/env python3
"""Archive the completed P3 offline runs as compact, provenance-bearing JSON."""
import csv
import hashlib
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT / "runtime"
OUT = ROOT / "artifacts/p3"


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def record(name, role):
    folder = RUNTIME / name
    evaluation = json.loads((folder / "evaluation.json").read_text())
    run = json.loads((folder / "run.json").read_text())
    identity = json.loads((folder / "identity.json").read_text())
    trajectory = folder / "trajectory.tum"
    poses = np.loadtxt(trajectory)
    result = {
        "run": name,
        "role": role,
        "status": json.loads((folder / "result.json").read_text())["status"],
        "protocol": "offline ROS bag replay, TBB threads=32",
        "threads": identity["threads"],
        "photo_enabled": identity.get("photo", False),
        "projection": identity.get("projection"),
        "measurement": identity.get("measurement"),
        "idw_enabled": identity.get("idw_enable"),
        "photo_selector": identity.get("photo_selector"),
        "photo_gate_threshold": identity.get("photo_gate_threshold"),
        "coin_enabled": identity.get("coin", False),
        "coin_selector": identity.get("selector"),
        "coin_g1_threshold": identity.get("gate_g1_threshold"),
        "coin_g2_threshold": identity.get("gate_g2_threshold"),
        "ate_rmse_m": evaluation["ate_rmse_m"],
        "ate_mean_m": evaluation["ate_mean_m"],
        "ate_median_m": evaluation["ate_median_m"],
        "ate_max_m": evaluation["ate_max_m"],
        "frames": evaluation["frames"],
        "matched": evaluation["matched"],
        "duration_s": evaluation["processed_duration_s"],
        "estimated_path_length_m": float(np.linalg.norm(np.diff(poses[:, 1:4], axis=0), axis=1).sum()),
        "wall_processing_s": run["wall_processing_s"],
        "cpu_user_s": run["cpu_user_s"],
        "peak_rss_kb": run["peak_rss_kb"],
        "trajectory_sha256": sha(trajectory),
        "bag_sha256": identity["bag_sha256"],
        "gt_sha256": evaluation["gt_sha256"],
        "evaluator_sha256": evaluation["evaluator_sha256"],
        "binary_sha256": identity["binary_sha256"],
        "source_head": identity["source_head"],
        "config_sha256": identity["config_sha256"],
        "p3_source_sha256": identity.get("p3_source_sha256", {}),
    }
    photo_csv = folder / "photo.csv"
    if photo_csv.exists():
        with photo_csv.open(newline="") as stream:
            rows = list(csv.DictReader(stream))
        if rows:
            result["photo_diagnostics"] = {
                "rows": len(rows),
                "mean_valid": float(np.mean([int(row["valid"]) for row in rows])),
                "mean_active_before": float(np.mean([int(row["active_before"]) for row in rows])),
                "mean_active_after": float(np.mean([int(row["active_after"]) for row in rows])),
                "weakest_gate_active_frames": sum(int(row["weakest_gate_active"]) for row in rows),
                "deskew_supported_frames": sum(int(row["deskew_supported"]) for row in rows),
            }
    coin_csv = folder / "coin_observation.csv"
    if coin_csv.exists():
        with coin_csv.open(newline="") as stream:
            rows = list(csv.DictReader(stream))
        if rows:
            result["coin_diagnostics"] = {
                "rows": len(rows),
                "used_frames": sum(row["status"] == "USED" for row in rows),
                "mean_valid_patches": float(np.mean([int(row["valid_patches"]) for row in rows])),
                "mean_photo_rows": float(np.mean([int(row["photo_rows"]) for row in rows])),
                "geometry_gate_active_frames": sum(int(row["gate_active"] or 0) for row in rows),
            }
    return result


def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


TUNNEL = {
    "geometry": ("p3_tunneld_geometry32", "P3 photo-off geometry control"),
    "coin_g1": ("offline_infra_tunneld_g1_threads32", "frozen COIN + Super-native G1 reference"),
    "coin_weakest": ("p3_tunneld_coin_weakest32", "COIN native weakest selector"),
    "coin_all_use": ("p3_tunneld_coin_alluse32", "COIN gradient selector without the G1 gate"),
    "cube_raw_c0": ("p3_tunneld_cube_raw_c0_all32", "cubemap raw, IDW off, all-use"),
    "cube_raw_c1": ("p3_tunneld_cube_raw_c1_all32", "cubemap raw, IDW on, all-use"),
    "sphere_raw_c1": ("p3_tunneld_sphere_raw_c1_all32", "equirectangular raw, IDW on, all-use"),
    "cube_igm_all": ("p3_tunneld_cube_igm_all32_det1", "cubemap IGM, IDW on, all-use"),
    "cube_igm_weakest": ("p3_tunneld_cube_igm_weakest32", "cubemap IGM, IDW on, Super-native weakest selector"),
    "sphere_igm_c2": ("p3_tunneld_sphere_igm_c2_all32", "equirectangular IGM, IDW on, all-use"),
}
NTU = {
    "geometry": ("p3_ntu_geometry32", "P3 photo-off geometry control"),
    "cube_raw_c0": ("p3_ntu_cube_raw_c0_all32", "cubemap raw, IDW off, all-use"),
    "cube_raw_c1": ("p3_ntu_cube_raw_c1_all32", "cubemap raw, IDW on, all-use"),
    "sphere_raw_c1": ("p3_ntu_sphere_raw_c1_all32", "equirectangular raw, IDW on, all-use"),
    "cube_igm_all": ("p3_ntu_cube_igm_all32", "cubemap IGM, IDW on, all-use"),
    "cube_igm_weakest": ("p3_ntu_cube_igm_weakest32", "cubemap IGM, IDW on, Super-native weakest selector"),
}
SHIELD = {
    "geometry": ("p3_shield4_geometry32", "P3 photo-off geometry control"),
    "geometry_repeat": ("p3_shield4_geometry32_repeat", "repeat of the photo-off geometry control"),
    "cube_igm_all": ("p3_shield4_cube_igm_all32", "cubemap IGM, IDW on, all-use"),
    "cube_igm_weakest": ("p3_shield4_cube_igm_weakest32", "cubemap IGM, IDW on, Super-native weakest selector"),
    "cube_raw_c0": ("p3_shield4_cube_raw_c0_all32", "cubemap raw, IDW off, all-use"),
    "cube_raw_c1": ("p3_shield4_cube_raw_c1_all32", "cubemap raw, IDW on, all-use"),
    "cube_raw_c1_repeat": ("p3_shield4_cube_raw_c1_all32_repeat", "repeat of cubemap raw C1 all-use"),
    "cube_raw_c1_weakest": ("p3_shield4_cube_raw_c1_weakest32", "cubemap raw, IDW on, Super-native weakest selector"),
    "sphere_raw_c1": ("p3_shield4_sphere_raw_c1_all32", "equirectangular raw, IDW on, all-use"),
    "sphere_igm_c2": ("p3_shield4_sphere_igm_c2_all32", "equirectangular IGM, IDW on, all-use"),
}


def group(spec):
    return {key: record(run, role) for key, (run, role) in spec.items()}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    tunnel, ntu, shield = group(TUNNEL), group(NTU), group(SHIELD)
    threshold = 0.2 * 501.43951893190064
    parity_path = ROOT / "artifacts/offline_infra/g1_cross_thread_parity.json"
    parity = json.loads(parity_path.read_text())
    write("baseline.json", {
        "protocol": "offline replay with TBB=32 for P3 runs",
        "COIN_baseline": {
            "dataset": "TunnelD",
            "description": "COIN intensity measurements with frozen Super-native G1 weakest-direction gate; archived offline 32T reference.",
            "run": tunnel["coin_g1"],
        },
        "photo_off_geometry_controls": {
            "TunnelD": tunnel["geometry"], "NTU_eee_01": ntu["geometry"],
            "Shield4": {"first": shield["geometry"], "repeat": shield["geometry_repeat"]},
        },
        "online_offline_g1_sha_check": {
            "source_artifact": "artifacts/offline_infra/g1_cross_thread_parity.json",
            "reference_run": parity["reference_run"], "reference_threads": parity["thread_counts"][0],
            "offline_run": parity["candidate_run"], "offline_threads": parity["thread_counts"][1],
            "same_shared_inputs": parity["shared_inputs_identical"],
            "same_estimator_source_hashes": parity["p2t_estimator_source_hashes_match"],
            "trajectory_sha256": parity["checks"]["trajectory.tum"]["reference_sha256"],
            "trajectory_byte_identical": parity["checks"]["trajectory.tum"]["byte_identical"],
            "coin_diagnostics_byte_identical": parity["checks"]["coin_observation.csv"]["byte_identical"],
            "time_audit_byte_identical": parity["checks"]["time_audit.csv"]["byte_identical"],
            "status": parity["status"],
        },
        "shield4_acceptance": {
            "gt_path_length_m": 501.43951893190064,
            "limit_fraction": 0.2,
            "ate_rmse_limit_m_exclusive": threshold,
        },
    })
    write("tunneld_results.json", {"dataset": "TunnelD", "runs": tunnel})
    write("ntu_results.json", {"dataset": "NTU eee_01", "runs": ntu})
    write("shield4_results.json", {
        "dataset": "Shield4",
        "ground_truth_path_length_m": 501.43951893190064,
        "acceptance": "ATE RMSE < 20% of GT path length",
        "ate_rmse_limit_m_exclusive": threshold,
        "passed": shield["cube_raw_c1"]["ate_rmse_m"] < threshold and shield["cube_raw_c1_repeat"]["ate_rmse_m"] < threshold,
        "passing_configuration": "cubemap + raw intensity + depth-gated IDW + all-use selector",
        "runs": shield,
        "interpretation": "Only cubemap raw C1 all-use passes, twice with identical trajectory SHA. The same representation with the Super-native weakest selector fails; IGM and equirectangular controls fail. The result meets the prompt's RMSE criterion, with a small margin and an estimated traveled distance longer than the reference path.",
    })
    write("projection_results.json", {
        "controlled_dimensions": {
            "C0_to_C1": "cubemap, raw channel, all-use; IDW disabled then enabled",
            "cubemap_to_equirectangular": "raw channel, IDW enabled, all-use",
            "raw_to_IGM": "cubemap, IDW enabled, all-use",
        },
        "TunnelD": {key: tunnel[key] for key in ("cube_raw_c0", "cube_raw_c1", "sphere_raw_c1", "cube_igm_all", "sphere_igm_c2")},
        "NTU_eee_01": {key: ntu[key] for key in ("cube_raw_c0", "cube_raw_c1", "sphere_raw_c1", "cube_igm_all")},
        "Shield4": {key: shield[key] for key in ("cube_raw_c0", "cube_raw_c1", "sphere_raw_c1", "cube_igm_all", "sphere_igm_c2")},
    })
    write("selector_ablation.json", {
        "selector_definitions": {
            "all": "P3 accepts every valid photo response under the existing cap; no Super-native weak-direction gate is applied.",
            "weakest": "P3 applies the frozen Super-native G1 weakest-translation gate (threshold 0.31913064578672057) to the photo rows.",
            "coin_g1": "COIN's existing selector with the frozen Super-native G1 confidence gate.",
            "coin_gradient": "COIN gradient-based ranking with no G1 geometry gate; it is not an unfiltered raw-pixel mode.",
            "coin_native_weakest": "COIN's own weakest-direction mode; listed separately from the frozen G1 comparison.",
        },
        "COIN_TunnelD": {key: tunnel[key] for key in ("coin_g1", "coin_all_use", "coin_weakest")},
        "Cubemap_IGM": {
            "TunnelD": {key: tunnel[key] for key in ("cube_igm_all", "cube_igm_weakest")},
            "NTU_eee_01": {key: ntu[key] for key in ("cube_igm_all", "cube_igm_weakest")},
            "Shield4": {key: shield[key] for key in ("cube_igm_all", "cube_igm_weakest")},
        },
        "Shield4_Cubemap_Raw_C1": {key: shield[key] for key in ("cube_raw_c1", "cube_raw_c1_weakest")},
    })
    tunnel_igm_hashes = [
        sha(RUNTIME / "p3_tunneld_cube_igm_all32_det1/trajectory.tum"),
        sha(RUNTIME / "p3_tunneld_cube_igm_all32_det2/trajectory.tum"),
    ]
    shield_c1_hashes = [shield["cube_raw_c1"]["trajectory_sha256"], shield["cube_raw_c1_repeat"]["trajectory_sha256"]]
    write("determinism.json", {
        "historical_online_offline_G1": {
            "source_artifact": "artifacts/offline_infra/g1_cross_thread_parity.json",
            "reference_run": parity["reference_run"], "offline_run": parity["candidate_run"],
            "threads": parity["thread_counts"], "trajectory_sha256": [
                parity["checks"]["trajectory.tum"]["reference_sha256"],
                parity["checks"]["trajectory.tum"]["candidate_sha256"],
            ], "byte_identical": parity["checks"]["trajectory.tum"]["byte_identical"],
            "all_recorded_diagnostics_identical": all(item["byte_identical"] for item in parity["checks"].values() if isinstance(item, dict) and "byte_identical" in item),
        },
        "TunnelD_Cubemap_IGM_all_use": {
            "runs": ["p3_tunneld_cube_igm_all32_det1", "p3_tunneld_cube_igm_all32_det2"],
            "trajectory_sha256": tunnel_igm_hashes,
            "bitwise_trajectory_identical": len(set(tunnel_igm_hashes)) == 1,
        },
        "Shield4_Cubemap_Raw_C1_all_use": {
            "runs": ["p3_shield4_cube_raw_c1_all32", "p3_shield4_cube_raw_c1_all32_repeat"],
            "trajectory_sha256": shield_c1_hashes,
            "ate_rmse_m": [shield["cube_raw_c1"]["ate_rmse_m"], shield["cube_raw_c1_repeat"]["ate_rmse_m"]],
            "bitwise_trajectory_identical": len(set(shield_c1_hashes)) == 1,
        },
        "Shield4_photo_off_geometry_control": {
            "runs": ["p3_shield4_geometry32", "p3_shield4_geometry32_repeat"],
            "trajectory_sha256": [shield["geometry"]["trajectory_sha256"], shield["geometry_repeat"]["trajectory_sha256"]],
            "ate_rmse_m": [shield["geometry"]["ate_rmse_m"], shield["geometry_repeat"]["ate_rmse_m"]],
            "bitwise_trajectory_identical": shield["geometry"]["trajectory_sha256"] == shield["geometry_repeat"]["trajectory_sha256"],
            "max_frame_translation_step_m": 94.3713950220214,
            "first_step_over_2m_at_frame": 2047,
        },
        "p3_fixed_chunk_reduction": "PhotoObservation reduces fixed 32-feature chunks in ascending order; the two full TunnelD CUBE+IGM all-use trajectories are byte-identical.",
    })


if __name__ == "__main__":
    main()
