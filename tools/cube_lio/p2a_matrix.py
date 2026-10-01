#!/usr/bin/env python3
"""Execute the predeclared P2A matrix once and stop on repeatability failure."""
import hashlib
import json
from pathlib import Path
import subprocess

import numpy as np

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compare(a, b):
    x, y = np.loadtxt(a), np.loadtxt(b)
    same_frames = x.shape == y.shape
    same_times = same_frames and np.array_equal(x[:, 0], y[:, 0])
    translation = float(np.linalg.norm(x[:, 1:4]-y[:, 1:4], axis=1).max()) if same_frames else None
    # q and -q encode the same orientation. Use chord distance to avoid acos rounding.
    qdelta = float(np.minimum(np.linalg.norm(x[:, 4:8]-y[:, 4:8], axis=1),
                             np.linalg.norm(x[:, 4:8]+y[:, 4:8], axis=1)).max()) if same_frames else None
    return {'sha256_a': sha(a), 'sha256_b': sha(b), 'byte_identical': a.read_bytes() == b.read_bytes(),
            'frames_a': len(x), 'frames_b': len(y), 'timestamps_identical': bool(same_times),
            'max_translation_difference_m': translation, 'max_quaternion_chord_difference': qdelta,
            'pass': bool(same_times and translation <= 1e-9 and qdelta <= 1e-9)}


def main():
    gate = json.loads((ROOT/'artifacts/p2a/shadow_gate.json').read_text())
    policies = ['C0']+gate['approved_policies']
    expected = ['C0', 'C60', 'C100', 'K100']
    assert policies == expected, 'The fixed authorized matrix requires all declared policies to pass.'
    plan = {'dataset_order': ['eee_01', 'tunnel_d', 'shield1'], 'policy_order': policies,
            'ntu_shield_runs_per_policy': 1, 'tunnel_runs_per_policy': 2,
            'audit_enabled': False, 'photo_off_regression_per_dataset': 1,
            'repeat_pose_tolerance': 1e-9, 'gate_sha256': sha(ROOT/'artifacts/p2a/shadow_gate.json'),
            'runs': []}
    path = ROOT/'artifacts/p2a/matrix_plan.json'
    assert not path.exists(), 'Matrix is single-use: do not rerun after observing ATE.'
    path.write_text(json.dumps(plan, indent=2)+'\n')
    evidence = {'tunnel_repeats': {}, 'photo_off_p0_parity': {}, 'c0_p1_parity': {}, 'status': 'RUNNING'}
    evidence_path = ROOT/'artifacts/p2a/determinism.json'
    def save():
        path.write_text(json.dumps(plan, indent=2)+'\n')
        evidence_path.write_text(json.dumps(evidence, indent=2)+'\n')
    def run(dataset, policy, repeat=1, photo=True):
        name = f'p2a_actual_{dataset}_{policy}_{repeat}' if photo else f'p2a_geo_regression_{dataset}'
        cmd = ['python3', str(ROOT/'tools/cube_lio/run.py'), dataset, '--name', name]
        if photo:
            cmd += ['--photo', '--policy', policy]
        print('RUN '+name, flush=True)
        subprocess.run(cmd, cwd=ROOT, check=True)
        plan['runs'].append({'dataset': dataset, 'policy': policy if photo else 'GEO',
                             'repeat': repeat, 'name': name, 'argv': cmd})
        save()
        return ROOT/'runtime'/name/'trajectory.tum'
    for dataset in plan['dataset_order']:
        geo = run(dataset, 'GEO', photo=False)
        check = compare(geo, ROOT/'runtime'/('p0_serial_'+dataset)/'trajectory.tum')
        evidence['photo_off_p0_parity'][dataset] = check
        save()
        assert check['byte_identical'], 'Photo-off frozen baseline regression'
        for policy in policies:
            first = run(dataset, policy)
            if policy == 'C0':
                check = compare(first, ROOT/'runtime'/('p1_final_photo_'+dataset)/'trajectory.tum')
                evidence['c0_p1_parity'][dataset] = check
                save()
                assert check['byte_identical'], 'C0 frozen P1 regression'
            if dataset == 'tunnel_d':
                second = run(dataset, policy, repeat=2)
                check = compare(first, second)
                evidence['tunnel_repeats'][policy] = check
                if not check['pass']:
                    evidence['status'] = 'NONDETERMINISM_BLOCKER'
                    save()
                    raise RuntimeError('NONDETERMINISM_BLOCKER: stopping without averaging or reruns')
                save()
    evidence['status'] = 'PASS'
    save()


if __name__ == '__main__':
    main()
