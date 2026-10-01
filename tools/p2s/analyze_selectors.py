#!/usr/bin/env python3
"""Compare selector-only shadows replayed on the identical frozen G1 geometry."""
import csv
import hashlib
import itertools
import json
import statistics
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MODES = {
    "S0_original": "p2s_shadow_s0_final",
    "S1_gradient": "p2s_shadow_gradient_final",
    "S2_weakest": "p2s_shadow_weakest_final",
    "S3_normalized": "p2s_shadow_normalized_final",
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def centers(value):
    if not value:
        return set()
    return {tuple(map(int, item.split(":"))) for item in value.split(";") if item}


def median(values):
    values = sorted(values)
    return statistics.median(values) if values else None


def mean(values):
    return statistics.mean(values) if values else None


def main():
    base = ROOT / "runtime/p2r_g1_rebased_history/trajectory.tum"
    baseline_hash = sha(base)
    summaries = []
    per_mode = {}
    for mode, directory in MODES.items():
        root = ROOT / "runtime" / directory
        rows = list(csv.DictReader(open(root / "coin_observation.csv", newline="")))
        trajectory_hash = sha(root / "trajectory.tum")
        if trajectory_hash != baseline_hash:
            raise RuntimeError(f"{mode} changed frozen G1 trajectory")
        used = [r for r in rows if r["status"] == "USED"]
        skipped = [r for r in rows if r["status"] == "SKIPPED"]
        all_sets = {int(r["frame"]): centers(r["selected_centers_xy"]) for r in rows}
        additions = [len(all_sets[int(r["frame"])]) for r in rows]
        regions = Counter()
        for items in all_sets.values():
            for x, y in items:
                regions[f"r{min(2, 3*y//128)}_c{min(2, 3*x//1024)}"] += 1
        weak_counts = Counter(int(r["weak_dirs"]) for r in used)
        photo_frames = [r for r in used if int(r["photo_rows"]) > 0]
        ncc_frames = [r for r in used if int(r["ncc_count"]) > 0]
        frame_by_id = {int(r["frame"]): r for r in rows}
        survival = []
        for row in used:
            fid = int(row["frame"])
            next_row = frame_by_id.get(fid + 1)
            if next_row and next_row["status"] == "USED" and int(row["active_after"]) > 0:
                previous_count = int(next_row["active_before"])
                rejected = int(next_row["removed"])
                survival.append(max(0, previous_count - rejected) / int(row["active_after"]))

        result = {
            "selector": mode,
            "runtime_dir": str(root.relative_to(ROOT)),
            "trajectory_sha256": trajectory_hash,
            "frozen_G1_trajectory_unchanged": True,
            "run_success": json.load(open(root / "result.json"))["status"] == "SUCCESS",
            "frames": len(rows), "accepted_photo_frames": len(used),
            "genuine_gap_skips": len(skipped),
            "skip_reasons": dict(Counter(r["skip_reason"] for r in skipped)),
            "accepted_motion_fallback_points": sum(int(r["motion_fallback_points"]) for r in used),
            "weak_direction_input_counts": {str(k): weak_counts[k] for k in sorted(weak_counts)},
            "max_active_feature_count": max(int(r["active_after"]) for r in rows),
            "sixty_feature_cap_preserved": max(int(r["active_after"]) for r in rows) <= 60,
            "selected_center_count_total": sum(additions),
            "selected_centers_per_frame_median": median(additions),
            "selected_centers_per_frame_mean": mean(additions),
            "selected_centers_image_region_3x3": dict(sorted(regions.items())),
            "selected_gradient_magnitude_mean": mean([float(r["selected_gradient_mean"]) for r in used
                                                        if r["selected_gradient_mean"]]),
            "selected_directional_score_mean_along_super_eigenvectors": {
                f"eigenvector_{i+1}": mean([float(r[f"selected_score_e{i+1}"]) for r in used
                                              if r[f"selected_score_e{i+1}"]]) for i in range(3)},
            "frame_statistics": {
                "active_before_median": median([int(r["active_before"]) for r in used]),
                "valid_patches_median": median([int(r["valid_patches"]) for r in photo_frames]),
                "photo_rows_median": median([int(r["photo_rows"]) for r in photo_frames]),
                "ncc_median": median([float(r["ncc_median"]) for r in ncc_frames]),
                "ncc_count_median": median([int(r["ncc_count"]) for r in ncc_frames]),
                "residual_rms_median": median([float(r["residual_rms"]) for r in photo_frames]),
                "next_frame_active_patch_survival_fraction_median": median(survival),
                "next_frame_active_patch_survival_fraction_mean": mean(survival),
            },
            "added_centers_by_frame": {str(k): sorted(v) for k, v in all_sets.items()},
        }
        per_mode[mode] = result
        summaries.append({k: v for k, v in result.items() if k != "added_centers_by_frame"})

    comparisons = {}
    for a, b in itertools.combinations(MODES, 2):
        av = per_mode[a]["added_centers_by_frame"]
        bv = per_mode[b]["added_centers_by_frame"]
        scores = []
        exact = 0
        for key in av:
            left, right = set(map(tuple, av[key])), set(map(tuple, bv[key]))
            union = left | right
            if union:
                scores.append(len(left & right) / len(union))
            if left == right:
                exact += 1
        comparisons[f"{a}__{b}"] = {
            "frames_compared": len(av), "mean_jaccard_nonempty_frames": mean(scores),
            "median_jaccard_nonempty_frames": median(scores),
            "fraction_nonempty_frames_with_same_added_centers": exact / len(av),
            "nonempty_frame_pairs": len(scores),
        }
    out = ROOT / "artifacts/p2s"
    out.mkdir(parents=True, exist_ok=True)
    fields = list(summaries[0])
    with open(out / "selector_shadow_summary.csv", "w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows({key: json.dumps(value, sort_keys=True) if isinstance(value, (dict, list)) else value
                          for key, value in row.items()} for row in summaries)
    (out / "selector_overlap.json").write_text(json.dumps({
        "frozen_G1_trajectory_sha256": baseline_hash,
        "all_modes_preserve_trajectory": True,
        "pairwise_added_center_overlap": comparisons,
        "modes": {k: {x: v for x, v in val.items() if x != "added_centers_by_frame"}
                  for k, val in per_mode.items()},
    }, indent=2, allow_nan=False) + "\n")
    print(json.dumps({k: {"selected": v["selected_center_count_total"],
                         "ncc_median": v["frame_statistics"]["ncc_median"],
                         "residual_rms_median": v["frame_statistics"]["residual_rms_median"],
                         "weak_input": v["weak_direction_input_counts"]}
                      for k, v in per_mode.items()}))


if __name__ == "__main__":
    main()
