#!/usr/bin/env python3
"""Compare offline replay artifacts and archive parity/benchmark evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ARTIFACTS = ROOT / 'artifacts/offline_infra'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def byte_check(a, b):
    return {'sha256_threads_1': sha(a), 'sha256_threads_32': sha(b),
            'bytes_threads_1': a.stat().st_size, 'bytes_threads_32': b.stat().st_size,
            'byte_identical': a.read_bytes() == b.read_bytes()}


def timestamp_bytes(path):
    return b''.join(line.split(None, 1)[0] + b'\n' for line in path.read_bytes().splitlines() if line.strip())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--threads-1', required=True, help='runtime run name for --threads 1')
    parser.add_argument('--threads-32', required=True, help='runtime run name for --threads 32')
    args = parser.parse_args()
    one = ROOT / 'runtime' / args.threads_1
    many = ROOT / 'runtime' / args.threads_32
    files = {
        'trajectory': (one / 'trajectory.tum', many / 'trajectory.tum'),
        'diagnostics_time_audit': (one / 'time_audit.csv', many / 'time_audit.csv'),
        'metrics_evaluation': (one / 'evaluation.json', many / 'evaluation.json'),
    }
    parity = {'dataset': 'eee_01', 'threads': [1, 32], 'runs': [args.threads_1, args.threads_32],
              'checks': {}, 'status': 'FAIL'}
    identity_1 = json.loads((one / 'identity.json').read_text())
    identity_32 = json.loads((many / 'identity.json').read_text())
    shared_identity_fields = ('dataset', 'bag', 'bag_sha256', 'config_sha256',
                              'binary_sha256', 'source_head', 'photo', 'photo_policy',
                              'photo_audit', 'coin', 'shadow', 'selector')
    parity['checks']['shared_input_identity'] = {
        'fields': list(shared_identity_fields),
        'byte_identical': all(identity_1.get(key) == identity_32.get(key)
                              for key in shared_identity_fields),
        'threads_1': {key: identity_1.get(key) for key in shared_identity_fields},
        'threads_32': {key: identity_32.get(key) for key in shared_identity_fields},
    }
    parity['infra_source_sha256'] = {
        relative: sha(ROOT / relative) for relative in (
            'src/super_lio/src/apps/cube_offline_node.cpp',
            'tools/offline/run.py', 'tools/offline/validate.py',
            'tools/offline/run_experiment.sh', 'tools/offline/compare_reference.py',
        )
    }
    for label, (a, b) in files.items():
        if not a.is_file() or not b.is_file():
            parity['checks'][label] = {'byte_identical': False,
                                       'missing_threads_1': not a.is_file(),
                                       'missing_threads_32': not b.is_file()}
        else:
            parity['checks'][label] = byte_check(a, b)
    t1, t32 = files['trajectory']
    if t1.is_file() and t32.is_file():
        left, right = timestamp_bytes(t1), timestamp_bytes(t32)
        parity['checks']['timestamps'] = {
            'rows_threads_1': left.count(b'\n'), 'rows_threads_32': right.count(b'\n'),
            'sha256_threads_1': hashlib.sha256(left).hexdigest(),
            'sha256_threads_32': hashlib.sha256(right).hexdigest(),
            'byte_identical': left == right,
        }
    deterministic_keys = ('lidar_read', 'imu_read', 'frames', 'bag_duration_s')
    run_metrics = []
    for folder in (one, many):
        data = json.loads((folder / 'run.json').read_text())
        run_metrics.append({key: data[key] for key in deterministic_keys})
    normalized = [json.dumps(item, sort_keys=True, separators=(',', ':')).encode() + b'\n'
                  for item in run_metrics]
    parity['checks']['deterministic_run_metrics'] = {
        'fields': list(deterministic_keys),
        'sha256_threads_1': hashlib.sha256(normalized[0]).hexdigest(),
        'sha256_threads_32': hashlib.sha256(normalized[1]).hexdigest(),
        'byte_identical': normalized[0] == normalized[1],
    }
    parity['status'] = 'PASS' if all(
        check.get('byte_identical', False) for check in parity['checks'].values()) else 'FAIL'
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    (ARTIFACTS / 'parity.json').write_text(json.dumps(parity, indent=2) + '\n')

    if parity['status'] != 'PASS':
        print(json.dumps(parity, indent=2))
        return 1

    run1 = json.loads((one / 'run.json').read_text())
    run32 = json.loads((many / 'run.json').read_text())
    wall1, wall32 = run1['wall_processing_s'], run32['wall_processing_s']
    logical_cpus = os.cpu_count() or 1
    def performance(run, wall):
        cpu = run['cpu_user_s'] + run['cpu_system_s']
        return {'wall_processing_s': wall, 'cpu_user_s': run['cpu_user_s'],
                'cpu_system_s': run['cpu_system_s'], 'cpu_total_s': cpu,
                'one_core_equivalent_percent': cpu / wall * 100.0,
                'logical_cpu_utilization_percent': cpu / (wall * logical_cpus) * 100.0,
                'peak_rss_kb': run['peak_rss_kb'], 'configured_tbb_threads': run['tbb_threads']}
    benchmark = {'dataset': 'eee_01', 'logical_cpu_count': logical_cpus,
                 'threads_1': performance(run1, wall1), 'threads_32': performance(run32, wall32),
                 'speedup_threads_32_over_1': wall1 / wall32,
                 'measurement_count_per_mode': 1,
                 'interpretation': 'single full-bag measurement per mode; wall time is the offline replay loop interval in run.json'}
    (ARTIFACTS / 'benchmark.json').write_text(json.dumps(benchmark, indent=2) + '\n')
    print(json.dumps({'parity': parity['status'], 'benchmark': benchmark}, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
