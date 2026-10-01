#!/usr/bin/env python3
"""Compare one offline run against a retained runtime reference by raw bytes."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'artifacts/offline_infra'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compare_file(reference, candidate):
    return {'reference_sha256': sha(reference), 'candidate_sha256': sha(candidate),
            'reference_bytes': reference.stat().st_size,
            'candidate_bytes': candidate.stat().st_size,
            'byte_identical': reference.read_bytes() == candidate.read_bytes()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', required=True, help='reference run name under runtime/')
    parser.add_argument('--candidate', required=True, help='candidate run name under runtime/')
    parser.add_argument('--output', default='g1_cross_thread_parity.json')
    args = parser.parse_args()
    if any(Path(name).name != name for name in (args.reference, args.candidate)):
        parser.error('run names must be single folders under runtime/')
    reference = ROOT / 'runtime' / args.reference
    candidate = ROOT / 'runtime' / args.candidate
    ref_identity = json.loads((reference / 'identity.json').read_text())
    cand_identity = json.loads((candidate / 'identity.json').read_text())
    ref_run = json.loads((reference / 'run.json').read_text())
    cand_run = json.loads((candidate / 'run.json').read_text())
    shared_fields = ('dataset', 'bag', 'config_sha256', 'coin', 'shadow', 'selector',
                     'gate_g1_threshold', 'gate_g2_threshold')
    prior_build = json.loads((ROOT / 'artifacts/p2t/build_identity.json').read_text())
    estimator_sources = [path for path in prior_build['source_sha256']
                         if path.startswith('src/super_lio/')]
    estimator_source_matches = {
        path: sha(ROOT / path) == prior_build['source_sha256'][path]
        for path in estimator_sources
    }
    output = {
        'reference_run': args.reference, 'candidate_run': args.candidate,
        'thread_counts': [ref_identity.get('threads'), cand_identity.get('threads')],
        'source_heads': [ref_identity.get('source_head'), cand_identity.get('source_head')],
        'binary_sha256': [ref_identity.get('binary_sha256'), cand_identity.get('binary_sha256')],
        'shared_inputs_identical': all(ref_identity.get(key) == cand_identity.get(key)
                                       for key in shared_fields),
        'shared_input_fields': list(shared_fields),
        'p2t_estimator_source_hashes_match': all(estimator_source_matches.values()),
        'p2t_estimator_source_hash_checks': estimator_source_matches,
        'reference_binary_matches_archived_p2t_binary':
            ref_identity.get('binary_sha256') == prior_build.get('binary_sha256'),
        'candidate_binary_sha256': cand_identity.get('binary_sha256'),
        'checks': {},
    }
    for filename in ('trajectory.tum', 'coin_observation.csv', 'time_audit.csv', 'evaluation.json'):
        a, b = reference / filename, candidate / filename
        output['checks'][filename] = compare_file(a, b) if a.is_file() and b.is_file() else {
            'byte_identical': False, 'missing_reference': not a.is_file(),
            'missing_candidate': not b.is_file()}
    metric_fields = ('lidar_read', 'imu_read', 'frames')
    ref_metrics = {key: ref_run[key] for key in metric_fields}
    cand_metrics = {key: cand_run[key] for key in metric_fields}
    ref_metrics['bag_duration_s_6sig'] = format(ref_run['bag_duration_s'], '.6g')
    cand_metrics['bag_duration_s_6sig'] = format(cand_run['bag_duration_s'], '.6g')
    output['checks']['deterministic_run_metrics'] = {
        'fields': list(metric_fields), 'reference': ref_metrics,
        'candidate': cand_metrics, 'identical': ref_metrics == cand_metrics,
    }
    output['status'] = 'PASS' if (output['shared_inputs_identical'] and
                                  output['p2t_estimator_source_hashes_match'] and all(
        check.get('byte_identical', check.get('identical', False))
        for check in output['checks'].values())) else 'FAIL'
    OUT.mkdir(parents=True, exist_ok=True)
    destination = OUT / args.output
    destination.write_text(json.dumps(output, indent=2) + '\n')
    print(json.dumps(output, indent=2))
    return 0 if output['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
