#!/usr/bin/env python3
"""Summarize the completed single-use P2A matrix without selecting new runs."""
import csv
import hashlib
import json
from pathlib import Path
import subprocess

import numpy as np
import pandas as pd

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    frozen = json.loads((ROOT/'artifacts/p1/frozen_identity.json').read_text())
    for name, digest in frozen.items():
        assert sha(ROOT/name) == digest, name
    for name in ['src/super_lio/src/intensity/cube_image.cpp',
                 'src/super_lio/test/test_cube_photo.cpp']:
        if (ROOT/name).exists():
            assert (ROOT/name).read_bytes() == subprocess.check_output(['git', 'show', '8db5c84281c35e848f2212fc482b60bf4e2a694b:'+name], cwd=ROOT)
    plan = json.loads((ROOT/'artifacts/p2a/matrix_plan.json').read_text())
    determinism = json.loads((ROOT/'artifacts/p2a/determinism.json').read_text())
    assert determinism['status'] == 'PASS'
    assert len(plan['runs']) == 19
    rows = []
    detail = {'runs': {}, 'frozen_hashes_unchanged': frozen,
              'shadow_gate_sha256': sha(ROOT/'artifacts/p2a/shadow_gate.json'),
              'matrix_plan_sha256': sha(ROOT/'artifacts/p2a/matrix_plan.json')}
    for run in plan['runs']:
        folder = ROOT/'runtime'/run['name']
        m = json.loads((folder/'evaluation.json').read_text())
        m.update(json.loads((folder/'run.json').read_text()))
        identity = json.loads((folder/'identity.json').read_text())
        m['identity'] = identity
        assert identity['config_sha256'] == frozen['tools/cube_lio/config/'+run['dataset']+'.yaml']
        assert m['evaluator_sha256'] == frozen['eval/evaluate.py']
        assert not identity['information_audit']
        baseline = json.loads((ROOT/'runtime'/('p0_serial_'+run['dataset'])/'evaluation.json').read_text())
        p1 = json.loads((ROOT/'runtime'/('p1_final_photo_'+run['dataset'])/'evaluation.json').read_text())
        trajectory = np.loadtxt(folder/'trajectory.tum')
        reference_times = np.loadtxt(ROOT/'runtime'/('p0_serial_'+run['dataset'])/'trajectory.tum')[:, 0]
        assert np.array_equal(trajectory[:, 0], reference_times)
        m['trajectory_sha256'] = sha(folder/'trajectory.tum')
        m['path_length_m'] = float(np.linalg.norm(np.diff(trajectory[:, 1:4], axis=0), axis=1).sum())
        m['translation_extent_m'] = float(np.linalg.norm(np.ptp(trajectory[:, 1:4], axis=0)))
        row = {k: run[k] for k in ['dataset', 'policy', 'repeat', 'name']}
        fields = ['ate_rmse_m', 'ate_max_m', 'matched', 'frames', 'duplicate_estimate_timestamps',
                  'processed_duration_s', 'first_timestamp', 'last_timestamp', 'wall_processing_s',
                  'peak_rss_kb', 'path_length_m', 'translation_extent_m', 'trajectory_sha256']
        row.update({k: m[k] for k in fields})
        row['delta_p0_percent'] = 100*(m['ate_rmse_m']/baseline['ate_rmse_m']-1)
        row['delta_p1_percent'] = 100*(m['ate_rmse_m']/p1['ate_rmse_m']-1)
        if run['policy'] != 'GEO':
            photo = pd.read_csv(folder/'photo.csv')
            assert len(photo) == m['frames']
            assert np.isfinite(photo.to_numpy()).all()
            assert (photo.trace_Ap >= 0).all()
            weak = photo.weak_translation_eigenvalue <= photo.weak_translation_eigenvalue.quantile(.1)
            ratios = photo.photo_in_weak_translation/np.maximum(photo.weak_translation_eigenvalue, 1e-9)
            diag = {'valid_mean': float(photo.valid.mean()),
                    'active_mean': float(photo.active_before.mean()),
                    'positive_information_fraction': float((photo.trace_Ap > 0).mean()),
                    'weak_decile_ratio0_median': float(ratios[weak].median()),
                    'weak_decile_ratio0_p90': float(ratios[weak].quantile(.9)),
                    'weak_decile_ratio0_p1_floor1_median': float((photo.photo_in_weak_translation/np.maximum(photo.weak_translation_eigenvalue, 1.))[weak].median()),
                    'all_active_ratio0_median': float(ratios[photo.trace_Ap > 0].median()),
                    'sigma': float(photo.sigma[photo.frozen == 1].iloc[0]),
                    'unsupported_deskew_frames': int((photo.deskew_supported == 0).sum()),
                    'timing_mean_ms': {k: float(photo[k].mean()) for k in ['deskew_ms', 'cubemap_ms', 'idw_ms', 'igm_ms', 'photo_jacobian_ms', 'update_ms', 'replenish_ms']}}
            m['photo_diagnostics'] = diag
            row.update({k: diag[k] for k in ['valid_mean', 'positive_information_fraction', 'weak_decile_ratio0_median', 'all_active_ratio0_median']})
        else:
            for k in ['valid_mean', 'positive_information_fraction', 'weak_decile_ratio0_median', 'all_active_ratio0_median']:
                row[k] = 0.
        if run['dataset'] == 'eee_01':
            row['class'] = 'PASS' if row['delta_p0_percent'] <= 5 else 'REGRESSION'
        elif run['dataset'] == 'tunnel_d':
            ate = m['ate_rmse_m']
            row['class'] = ('REFERENCE_LEVEL' if ate <= 1 else 'STRONG_RESCUE' if ate <= 2
                            else 'CLEAR_RESCUE' if ate <= 10 else 'BOUNDED_IMPROVEMENT'
                            if ate < min(baseline['ate_rmse_m'], p1['ate_rmse_m']) else 'FAIL')
        else:
            row['class'] = 'CATASTROPHIC_ERROR_REMAINS'
        detail['runs'][run['name']] = m
        rows.append(row)
    with (ROOT/'artifacts/p2a/ablation_summary.csv').open('w') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]), lineterminator='\n')
        writer.writeheader(); writer.writerows(rows)
    (ROOT/'artifacts/p2a/metrics.json').write_text(json.dumps(detail, indent=2)+'\n')
    print(pd.DataFrame(rows)[['dataset', 'policy', 'repeat', 'ate_rmse_m', 'ate_max_m', 'class']].to_string(index=False))


if __name__ == '__main__':
    main()
