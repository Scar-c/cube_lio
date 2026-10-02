#!/usr/bin/env python3
"""P3-A orchestration/archival; delegates replay and evaluation unchanged."""
import csv
import argparse
import hashlib
import json
import math
import os
import re
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'artifacts/p3a'
PREFIX = 'p3a'
BASE = '04a48588bcdc4a2210b189aaefe2cf3a29f48302'
G1 = '0.31913064578672057'
G2 = '0.359189249724233'
MODES = {'R0': 'coin', 'R1': 'cube_raw_no_idw',
         'R2': 'cube_raw_idw', 'R3': 'cube_igm_idw'}


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def load(path):
    return json.loads(path.read_text())


def diagnostics(path):
    with path.open() as stream:
        rows = list(csv.DictReader(stream))
    def values(key):
        return [float(row[key]) for row in rows if row.get(key, '') != '']
    result = {'scans': len(rows), 'csv_sha256': sha(path)}
    for key in ('active_before', 'active_after', 'valid_patches', 'photo_rows',
                'residual_rms', 'ncc_median', 'gate_active', 'motion_fallback_points'):
        data = values(key)
        result[key] = {'mean': sum(data)/len(data), 'min': min(data),
                       'max': max(data), 'sum': sum(data)} if data else None
    photo_rows = sum(float(row['photo_rows']) for row in rows)
    result['pooled_residual_rms'] = math.sqrt(sum(
        float(row['photo_rows'])*float(row['residual_rms'])**2 for row in rows)/photo_rows) if photo_rows else None
    result['skipped_scans'] = sum(row['status'] != 'USED' for row in rows)
    return result


def summarize(name, representation, selector):
    folder = ROOT / 'runtime' / name
    result = load(folder / 'result.json')
    if result['status'] != 'SUCCESS':
        raise RuntimeError(f'{name} failed: {result}')
    evaluation = load(folder / 'evaluation.json')
    if not math.isfinite(evaluation['ate_rmse_m']):
        raise RuntimeError(f'{name}: nonfinite ATE')
    record = {'name': name, 'representation': representation, 'selector': selector,
              'trajectory_sha256': sha(folder / 'trajectory.tum'),
              'evaluation_sha256': sha(folder / 'evaluation.json'),
              'evaluation': evaluation, 'runtime': load(folder / 'run.json'),
              'identity': load(folder / 'identity.json'),
              'diagnostics': diagnostics(folder / 'coin_observation.csv')}
    audit = folder / 'fusion.json'
    if audit.exists():
        record['fusion_audit'] = load(audit)
        if record['fusion_audit']['status'] != 'PASS':
            raise RuntimeError(f'{name}: fusion audit failed')
    representation_csv = folder / 'representation.csv'
    if representation_csv.exists():
        with representation_csv.open() as stream:
            rows = list(csv.DictReader(stream))
        record['representation_diagnostics'] = {
            'scans': len(rows), 'sha256': sha(representation_csv),
            'sum': {key: sum(int(row[key]) for row in rows) for key in
                    ('input_points', 'raw_pixels', 'filled_pixels', 'igm_pixels',
                     'patch_mask_pixels', 'intensity_points')}}
        shutil.copyfile(representation_csv, OUT / 'diagnostics' / f'{name}_representation.csv')
    shutil.copyfile(folder / 'coin_observation.csv', OUT / 'diagnostics' / f'{name}_coin.csv')
    return record


def run(representation, selector, repeat, port):
    name = f'{PREFIX}_{representation.lower()}_{selector}_run{repeat}'
    command = ['tools/offline/run_experiment.sh', 'tunnel_d', '--name', name,
               '--port', str(port), '--threads', '32', '--coin', '--selector', selector,
               '--gate-g1-threshold', G1, '--gate-g2-threshold', G2,
               '--fusion-audit-json', f'runtime/{name}/fusion.json']
    mode = MODES[representation]
    with (OUT / 'commands.md').open('a') as stream:
        stream.write('\n```bash\nCUBE_P3A_REPRESENTATION=' + mode + ' ' + ' '.join(command) + '\n```\n')
    env = dict(os.environ, CUBE_P3A_REPRESENTATION=mode)
    print(json.dumps({'starting': name, 'mode': mode, 'threads': 32}), flush=True)
    with (ROOT / 'runtime' / f'{name}_launch.log').open('w') as log:
        subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=log, check=True)
    record = summarize(name, mode, selector)
    record['command'] = command
    record['representation_environment'] = {'CUBE_P3A_REPRESENTATION': mode}
    record['build_source_sha256'] = load(OUT / 'source_manifest.json')['working_source_sha256']
    print(json.dumps({'finished': name, 'ate_rmse_m': record['evaluation']['ate_rmse_m'],
                      'sha256': record['trajectory_sha256']}), flush=True)
    return record


def source_manifest():
    paths = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard',
                                      'src/super_lio', 'src/basic', 'tools/offline', 'eval',
                                      'tools/cube_lio/config'], cwd=ROOT, text=True).splitlines()
    working = {path: sha(ROOT / path) for path in paths if (ROOT / path).is_file()}
    changed = {}
    for path, digest in working.items():
        original = subprocess.run(['git', 'show', f'{BASE}:{path}'], cwd=ROOT,
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        previous = hashlib.sha256(original.stdout).hexdigest() if original.returncode == 0 else None
        if previous != digest:
            changed[path] = {'base_sha256': previous, 'working_sha256': digest}
    protected = ['src/super_lio/src/lio/', 'src/super_lio/include/lio/',
                 'src/super_lio/src/ros/', 'src/super_lio/include/ros/', 'src/basic/',
                 'src/super_lio/src/apps/', 'tools/offline/', 'eval/', 'tools/cube_lio/config/']
    if any(any(path.startswith(prefix) for prefix in protected) or
           path.endswith('super_degeneracy_gate.hpp') for path in changed):
        raise RuntimeError('frozen component changed')
    functions = {
        'fusion_add': ('src/super_lio/src/intensity/coin/coin_observation.cpp',
                       'void CoinObservation::add(', 'void CoinObservation::finish('),
        'gate_and_selector_finish': ('src/super_lio/src/intensity/coin/coin_observation.cpp',
                                     'void CoinObservation::finish(', None),
        'feature_track_detect': ('src/super_lio/src/intensity/coin/coin_feature_manager.cpp',
                                 'void CoinFeatureManager::update(', None),
    }
    function_checks = {}
    for label, (path, start, end) in functions.items():
        before = subprocess.check_output(['git', 'show', f'{BASE}:{path}'], cwd=ROOT, text=True)
        after = (ROOT / path).read_text()
        def section(value):
            value = value[value.index(start):]
            return value[:value.index(end)] if end else value
        previous, current = section(before), section(after)
        function_checks[label] = {'identical': previous == current,
                                 'sha256': hashlib.sha256(current.encode()).hexdigest()}
        if previous != current:
            raise RuntimeError(f'frozen function changed: {label}')
    return {'base_commit': BASE, 'working_source_sha256': working,
            'changed_source_files': changed, 'frozen_components_unchanged': True,
            'frozen_function_checks': function_checks,
            'binary_sha256': sha(ROOT / 'devel/lib/super_lio/cube_offline_node'),
            'lio_library_sha256': sha(ROOT / 'devel/lib/liblio.so'),
            'switch': 'CUBE_P3A_REPRESENTATION; opt-in, default coin',
            'frozen_settings': {'selector': 'g1', 'g1_threshold': float(G1), 'g2_threshold': float(G2),
                'photo_scale': .00095, 'measurement_variance': .001, 'num_features': 60,
                'patch_size': 5, 'max_lifetime': 25, 'suppression_radius': 10, 'grad_min': 16.5,
                'ncc_threshold': .7075, 'margin': 10, 'range_threshold': .2,
                'resolution': 96, 'idw_radius': 3, 'idw_k': 6, 'idw_min_support': 3,
                'idw_power': 2., 'range_absolute': .3, 'range_relative': .02, 'gaussian_sigma': 1.}}


def repeat_check(representation, selector, first, port):
    repeats = [first] + [run(representation, selector, repeat, port+repeat-2) for repeat in (2, 3)]
    identical_sha = len({record['trajectory_sha256'] for record in repeats}) == 1
    identical_metrics = all(record['evaluation'] == first['evaluation'] for record in repeats)
    identical_diagnostics = len({record['diagnostics']['csv_sha256'] for record in repeats}) == 1
    return {'representation': representation, 'selector': selector, 'runs': repeats,
            'trajectory_sha_identical': identical_sha, 'evaluation_metrics_identical': identical_metrics,
            'coin_diagnostics_identical': identical_diagnostics,
            'pass': identical_sha and identical_metrics and identical_diagnostics}


def main():
    global OUT, PREFIX
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=Path('artifacts/p3a'))
    parser.add_argument('--prefix', default='p3a', help='fresh runtime folder prefix for repeats/ablations')
    parser.add_argument('--baseline-run', default='p3a_r0_coin_g1_run1', help='completed native COIN G1 Phase 0 run')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_]+', args.prefix) or Path(args.baseline_run).name != args.baseline_run:
        parser.error('prefix and baseline-run must be single safe folder names')
    PREFIX = args.prefix
    OUT = args.output_dir if args.output_dir.is_absolute() else ROOT / args.output_dir
    OUT.mkdir(exist_ok=False, parents=True)
    (OUT / 'diagnostics').mkdir()
    write('source_manifest.json', source_manifest())
    (OUT / 'commands.md').write_text('# P3-A commands\n\nAll replays use the frozen offline runner and 32T.\n'
        'The explicit environment switch is recorded separately because the frozen runner does not record it.\n'
        'Source and binary hashes are in `source_manifest.json` and each result. No configurations are edited.\n\n'
        '```bash\ngit switch -c p3a-tunneld-cubemap-validation\n'
        'tools/offline/run_experiment.sh tunnel_d --name p3a_r0_coin_g1_run1 --port 11740 --threads 32 '
        '--coin --selector g1 --gate-g1-threshold ' + G1 + ' --gate-g2-threshold ' + G2 + ' '
        '--fusion-audit-json runtime/p3a_r0_coin_g1_run1/fusion.json\n'
        'source /opt/ros/noetic/setup.bash\nsource devel/setup.bash\n'
        'catkin_make --pkg super_lio --make-args cube_offline_node test_coin_feature_math test_cube_photo -j2\n'
        'devel/lib/super_lio/test_coin_feature_math\ndevel/lib/super_lio/test_cube_photo\n'
        'python3 tools/p3a/validate.py\n```\n')
    baseline = summarize(args.baseline_run, 'coin', 'g1')
    write('baseline_coin.json', baseline)
    # Verify opt-in isolation before any Cubemap run; these are also R0's
    # required three repeats, including the clean, pre-integration Phase 0 run.
    checks = [repeat_check('R0', 'g1', baseline, 11745)]
    if not checks[0]['pass']:
        raise RuntimeError('default COIN baseline changed after representation integration')
    records = {'R0': baseline}
    for index, representation in enumerate(('R1', 'R2', 'R3')):
        records[representation] = run(representation, 'g1', 1, 11741+index)
        write('cubemap_results.json', {'same_selector_feature_limit_fusion': True, 'results': records})
    best = min(('R1', 'R2', 'R3'), key=lambda key: records[key]['evaluation']['ate_rmse_m'])
    all_use = run(best, 'gradient', 1, 11744)
    write('selector_ablation.json', {'best_cubemap': best, 'selection_rule': 'minimum R1/R2/R3 full-bag ATE; no tuning',
          'weakest_with_super_native_gate': records[best], 'all_use_no_direction_dependency': all_use,
          'all_use_means': 'existing COIN gradient mode: pure gradient ranking, same cap/NCC/fusion; gate not used'})
    candidates = [(key, record['selector'], record) for key, record in records.items()
                  if key != 'R0' and record['evaluation']['ate_rmse_m'] < 10.]
    if all_use['evaluation']['ate_rmse_m'] < 10.:
        candidates.append((best, 'gradient', all_use))
    for index, (representation, selector, first) in enumerate(candidates):
        checks.append(repeat_check(representation, selector, first, 11747+index*2))
    write('determinism.json', {'criterion': 'three runs for every ATE<10 candidate', 'checks': checks,
          'runtime_not_required_identical': True, 'pass': all(check['pass'] for check in checks)})
    if not all(check['pass'] for check in checks):
        raise RuntimeError('determinism failed')
    original = ROOT / 'runtime/offline_infra_tunneld_g1_threads32'
    write('baseline_parity.json', {'reference': str(original.relative_to(ROOT)),
        'reference_trajectory_sha256': sha(original / 'trajectory.tum'),
        'phase0_trajectory_sha256': baseline['trajectory_sha256'],
        'identical': sha(original / 'trajectory.tum') == baseline['trajectory_sha256']})
    print(json.dumps({'complete': True, 'best_cubemap': best,
        'best_ate_rmse_m': records[best]['evaluation']['ate_rmse_m'],
        'cubemap_below_10m': any(records[key]['evaluation']['ate_rmse_m'] < 10. for key in ('R1', 'R2', 'R3'))}), flush=True)


if __name__ == '__main__':
    main()
