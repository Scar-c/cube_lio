#!/usr/bin/env python3
"""Gate frozen-trajectory policies using information only; never reads ATE."""
import csv
import hashlib
import json
from pathlib import Path

import numpy as np
import pandas as pd

ROOT = Path(__file__).resolve().parents[2]
DATASETS = ['eee_01', 'tunnel_d', 'shield1']
POLICIES = ['C0', 'C60', 'C100', 'K100']


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    gate = {'selection_uses_gt_or_ate': False,
            'substantial_reduction_definition': 'at least 50% reduction of last-iteration weakest-geometry-decile median qp/qg on frozen TunnelD',
            'relative_psd_tolerance': 1e-10,
            'raw_sampler_relative_tolerance': 1e-12,
            'serial_parallel_fixture': json.loads((ROOT/'artifacts/p2a/policy_tests.json').read_text()),
            'datasets': {}, 'approved_policies': []}
    fixture = gate['serial_parallel_fixture']
    assert fixture['max_relative_A_error'] <= 1e-14
    assert fixture['max_relative_b_error'] <= 1e-14
    assert fixture['selected_ids_exact']
    summaries = []
    for dataset in DATASETS:
        folder = ROOT/'runtime'/('p2a_shadow_'+dataset)
        trajectory = folder/'trajectory.tum'
        assert trajectory.read_bytes() == (ROOT/'runtime'/('p1_final_photo_'+dataset)/'trajectory.tum').read_bytes()
        x = pd.read_csv(folder/'information.csv')
        assert np.isfinite(x.select_dtypes(include='number').to_numpy()).all()
        assert (x.valid_count_error == 0).all()
        assert x.raw_A_relative_error.max() < 1e-12
        assert x.raw_b_relative_error.max() < 1e-12
        c0 = x[x.policy == 'C0'].reset_index(drop=True)
        last0 = c0.groupby('frame', sort=False).tail(1)
        weak_frames = set(last0[last0.qg0 <= last0.qg0.quantile(.1)].frame)
        assert len(last0) == len(np.loadtxt(trajectory))
        ds = {'trajectory_sha256': sha(trajectory), 'p1_byte_parity': True,
              'information_csv_sha256': sha(folder/'information.csv'),
              'frames': len(last0), 'observations': len(c0),
              'raw_A_max_relative_error': float(x.raw_A_relative_error.max()),
              'raw_b_max_relative_error': float(x.raw_b_relative_error.max()),
              'policies': {}}
        for policy in POLICIES:
            y = x[x.policy == policy].reset_index(drop=True)
            assert np.array_equal(y[['frame', 'iteration']], c0[['frame', 'iteration']])
            scale = np.maximum(1., c0.trace_Ap)
            assert (y.authority_difference_min_eigenvalue >= -1e-10*scale).all()
            assert (y.psd_min >= -1e-10*np.maximum(1., y.trace_Ap)).all()
            assert (y.trace_Ap <= c0.trace_Ap + 1e-10*scale).all()
            for k in range(3):
                assert (y['qp'+str(k)] <= c0['qp'+str(k)] + 1e-10*scale).all()
            assert (y.alpha <= 1).all() and (y.alpha > 0).all()
            assert (y.n_used <= y.n_valid).all()
            if policy in ['C60', 'C100']:
                budget = int(policy[1:])
                expected = np.minimum(1., budget/np.maximum(1., y.n_valid))
                assert np.allclose(y.alpha, expected, rtol=1e-14, atol=0)
                assert np.array_equal(y.n_used, y.n_valid)
            if policy == 'K100':
                assert y.n_used.max() <= 100
            last = y.groupby('frame', sort=False).tail(1)
            weak_median = float(last[last.frame.isin(weak_frames)].ratio0.median())
            ds['policies'][policy] = {'all_iterations_no_authority_increase': True,
                'finite_psd': True, 'weakest_decile_median_ratio': weak_median,
                'min_authority_difference_eigenvalue': float(y.authority_difference_min_eigenvalue.min())}
            for scope, z in [('all_iterations', y), ('last_iteration', last)]:
                active = z[z.trace_Ap > 0]
                row = {'dataset': dataset, 'policy': policy, 'scope': scope,
                       'observations': len(z), 'positive_information_fraction': float((z.trace_Ap > 0).mean()),
                       'weak_decile_ratio0_median': float(z[z.frame.isin(weak_frames)].ratio0.median())}
                fields = ['n_valid', 'n_used', 'alpha', 'ratio0', 'ratio1', 'ratio2', 'trace_ratio',
                          'd_photo_translation', 'd_photo_rotation', 'occupied_faces', 'occupied_cells',
                          'residuals_per_cell', 'top10_cell_trace_fraction', 'residual_median',
                          'residual_mad', 'residual_rms']
                for field in fields:
                    values = active[field] if field in ['occupied_faces', 'occupied_cells', 'residuals_per_cell', 'top10_cell_trace_fraction'] else z[field]
                    if values.empty:
                        values = pd.Series([0.])
                    row[field+'_mean'] = float(values.mean())
                    for q, label in [(.5, 'p50'), (.9, 'p90'), (.99, 'p99')]:
                        row[field+'_'+label] = float(values.quantile(q))
                    row[field+'_max'] = float(values.max())
                summaries.append(row)
        gate['datasets'][dataset] = ds
    tunnel = gate['datasets']['tunnel_d']['policies']
    for policy in POLICIES[1:]:
        reduction = 1.-tunnel[policy]['weakest_decile_median_ratio']/tunnel['C0']['weakest_decile_median_ratio']
        tunnel[policy]['weak_decile_reduction_fraction'] = reduction
        if reduction >= .5:
            gate['approved_policies'].append(policy)
    with (ROOT/'artifacts/p2a/shadow_summary.csv').open('w') as f:
        writer = csv.DictWriter(f, fieldnames=list(summaries[0]), lineterminator='\n')
        writer.writeheader(); writer.writerows(summaries)
    (ROOT/'artifacts/p2a/shadow_gate.json').write_text(json.dumps(gate, indent=2)+'\n')
    print(json.dumps({'approved_policies': gate['approved_policies'], 'tunnel': tunnel}, indent=2))


if __name__ == '__main__':
    main()
