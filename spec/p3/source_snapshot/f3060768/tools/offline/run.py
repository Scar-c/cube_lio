#!/usr/bin/env python3
"""Run CUBE-LIO directly from a ROS1 bag with a bounded TBB scheduler."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import xmlrpc.client

ROOT = Path(__file__).resolve().parents[2]
BAGS = {
    'eee_01': Path('/home/lc/super_livo/bag/NTU/eee_01/eee_01.bag'),
    'tunnel_d': Path('/home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag'),
    'shield1': Path('/home/lc/algorithm_versa/bag/GEODE/Shield_tunnel1_gamma.bag'),
    'shield4': Path('/home/lc/algorithm_versa/bag/GEODE/Shield_tunnel4_gamma.bag'),
}
CONFIGS = {name: ROOT / 'tools/cube_lio/config' / f'{name}.yaml' for name in BAGS}
PHOTO_POLICIES = ('C0', 'C60', 'C100', 'K100')
COIN_SELECTORS = ('original', 'gradient', 'weakest', 'normalized', 's2', 'g1', 'g2')
PROJECTIONS = ('cubemap', 'equirectangular')
MEASUREMENTS = ('raw', 'igm')
PHOTO_SELECTORS = ('all', 'weakest')


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def positive_int(value):
    parsed = int(value)
    if parsed < 1:
        raise argparse.ArgumentTypeError('must be a positive integer')
    return parsed


def repo_path(path):
    path = Path(path)
    return path if path.is_absolute() else ROOT / path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dataset', choices=BAGS)
    parser.add_argument('--name', required=True, help='unique output folder name under runtime/')
    parser.add_argument('--port', type=int, default=11562)
    parser.add_argument('--threads', type=positive_int, default=32,
                        help='TBB worker limit (default: 32; use 1 for serial parity checks)')
    parser.add_argument('--photo', action='store_true', help='enable CUBE-LIO photometric updates')
    parser.add_argument('--policy', choices=PHOTO_POLICIES, default='C0')
    parser.add_argument('--audit', action='store_true', help='write photometric information diagnostics')
    parser.add_argument('--idw-off', action='store_true')
    parser.add_argument('--projection', choices=PROJECTIONS, default='cubemap',
                        help='photo raster projection (default: cubemap)')
    parser.add_argument('--measurement', choices=MEASUREMENTS, default='igm',
                        help='sample raw intensity or IGM (default: igm)')
    parser.add_argument('--photo-selector', choices=PHOTO_SELECTORS, default='all',
                        help='feature selector for the photo side channel')
    parser.add_argument('--photo-gate-threshold', type=float, default=0.31913064578672057,
                        help='frozen Super-native weakest-direction gate threshold')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--coin', action='store_true', help='inject faithful COIN measurements')
    mode.add_argument('--shadow', action='store_true', help='run COIN features without injecting measurements')
    parser.add_argument('--selector', choices=COIN_SELECTORS, default='original')
    parser.add_argument('--gate-g1-threshold', type=float)
    parser.add_argument('--gate-g2-threshold', type=float)
    parser.add_argument('--audit-csv', type=Path, help='write Ouster timing diagnostics')
    parser.add_argument('--fusion-audit-json', type=Path)
    parser.add_argument('--geometry-rows', type=Path, help='diagnostic-only final geometry rows binary trace')
    args = parser.parse_args()

    if Path(args.name).name != args.name or args.name in ('', '.', '..'):
        parser.error('--name must be a single folder name under runtime/')

    bag = BAGS[args.dataset]
    if not bag.is_file():
        parser.error(f'bag does not exist: {bag}')
    if (args.coin or args.shadow) and args.dataset != 'tunnel_d':
        parser.error('COIN selection is scoped to TunnelD')
    if args.coin and args.selector in ('g1', 'g2') and (
            args.gate_g1_threshold is None or args.gate_g2_threshold is None):
        parser.error('gated COIN selectors require geometry-derived G1 and G2 thresholds')

    out = ROOT / 'runtime' / args.name
    out.mkdir(parents=True, exist_ok=False)
    audit_csv = repo_path(args.audit_csv) if args.audit_csv else None
    fusion_audit_json = repo_path(args.fusion_audit_json) if args.fusion_audit_json else None
    geometry_rows = repo_path(args.geometry_rows) if args.geometry_rows else None
    for path in (audit_csv, fusion_audit_json, geometry_rows):
        if path:
            path.parent.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, ROS_MASTER_URI=f'http://127.0.0.1:{args.port}',
               ROS_HOSTNAME='127.0.0.1', ROS_HOME=str(out / 'ros_home'),
               ROS_LOG_DIR=str(out / 'ros_log'))
    config = CONFIGS[args.dataset]
    photo_config = ROOT / 'tools/cube_lio/config/photo.yaml'
    coin_configs = [ROOT / 'refs/COIN-LIO/config' / name
                    for name in ('params.yaml', 'line_removal.yaml', 'os_enwide.json')]
    paths = [config, photo_config] + (coin_configs if args.dataset == 'tunnel_d' else [])
    identity = {
        'dataset': args.dataset, 'bag': str(bag), 'bag_sha256': sha(bag),
        'threads': args.threads, 'photo': args.photo, 'photo_policy': args.policy,
        'photo_audit': args.audit, 'idw_enable': not args.idw_off,
        'projection': args.projection, 'measurement': args.measurement,
        'photo_selector': args.photo_selector,
        'photo_gate_threshold': args.photo_gate_threshold,
        'coin': args.coin, 'shadow': args.shadow, 'selector': args.selector,
        'gate_g1_threshold': args.gate_g1_threshold,
        'gate_g2_threshold': args.gate_g2_threshold,
        'audit_csv': str(audit_csv) if audit_csv else None,
        'fusion_audit_json': str(fusion_audit_json) if fusion_audit_json else None,
        'geometry_rows': str(geometry_rows) if geometry_rows else None,
        'source_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'config_sha256': {str(p.relative_to(ROOT)): sha(p) for p in paths},
        'p3_source_sha256': {relative: sha(ROOT / relative) for relative in (
            'src/super_lio/include/intensity/intensity_representation.hpp',
            'src/super_lio/include/intensity/cube_image.hpp',
            'src/super_lio/include/intensity/spherical_image.hpp',
            'src/super_lio/include/intensity/photo_observation.hpp',
            'src/super_lio/src/intensity/cube_image.cpp',
            'src/super_lio/src/intensity/spherical_image.cpp',
            'src/super_lio/src/intensity/photo_observation.cpp',
            'src/super_lio/src/lio/super_lio.cpp',
            'tools/offline/run.py', 'eval/evaluate.py',
        )},
        'evaluator_sha256': sha(ROOT / 'eval/evaluate.py'),
        'binary_sha256': sha(ROOT / 'devel/lib/super_lio/cube_offline_node'),
    }
    (out / 'identity.json').write_text(json.dumps(identity, indent=2) + '\n')
    master_log = (out / 'master.log').open('w')
    master = subprocess.Popen(['roscore', '-p', str(args.port)], cwd=ROOT, env=env,
                              stdout=master_log, stderr=master_log)
    result = {'status': 'NOT_STARTED', 'node_exit_code': None, 'evaluator_exit_code': None}
    try:
        for _ in range(150):
            try:
                xmlrpc.client.ServerProxy(env['ROS_MASTER_URI']).getPid('/offline_runner')
                break
            except OSError:
                if master.poll() is not None:
                    raise RuntimeError('roscore exited')
                time.sleep(.1)
        else:
            raise RuntimeError('roscore startup timeout')

        for path in paths:
            subprocess.run(['rosparam', 'load', str(path)], env=env, check=True)
        params = {
            '/lio/offline/bag': str(bag), '/lio/offline/out_dir': str(out),
            '/lio/offline/threads': str(args.threads),
            '/photo/enable': str(args.photo).lower(),
            '/photo/projection': args.projection,
            '/photo/measurement': args.measurement,
            '/photo/selector': args.photo_selector,
            '/photo/weakest_gate_threshold': str(args.photo_gate_threshold),
            '/photo/information_policy': args.policy,
            '/photo/information_audit': str(args.audit).lower(),
            '/coin/enable': str(args.coin or args.shadow).lower(),
            '/coin/shadow': str(args.shadow).lower(), '/coin/selector_mode': args.selector,
            '/image/u_shift': '0', '/coin/measurement_variance': '0.001',
            '/p2r/time_audit_path': str(audit_csv) if audit_csv else '',
            '/p2r/fusion_audit_path': str(fusion_audit_json) if fusion_audit_json else '',
            '/p2s/geometry_rows_path': str(geometry_rows) if geometry_rows else '',
        }
        if args.idw_off:
            params['/cubemap/idw_enable'] = 'false'
        if args.gate_g1_threshold is not None:
            params['/coin/gate_g1_confidence_threshold'] = str(args.gate_g1_threshold)
        if args.gate_g2_threshold is not None:
            params['/coin/gate_g2_confidence_threshold'] = str(args.gate_g2_threshold)
        for key, value in params.items():
            subprocess.run(['rosparam', 'set', key, value], env=env, check=True)

        with (out / 'node.log').open('w') as log:
            node = subprocess.run([str(ROOT / 'devel/lib/super_lio/cube_offline_node')],
                                  cwd=ROOT, env=env, stdout=log, stderr=log)
        result['node_exit_code'] = node.returncode
        if node.returncode == 0:
            with (out / 'evaluator.log').open('w') as log:
                evaluator = subprocess.run(['python3', str(ROOT / 'eval/evaluate.py'),
                                            args.dataset, str(out)], cwd=ROOT, env=env,
                                           stdout=log, stderr=subprocess.STDOUT)
            result['evaluator_exit_code'] = evaluator.returncode
            result['status'] = 'SUCCESS' if evaluator.returncode == 0 else 'EVALUATION_FAILED'
        else:
            result['status'] = 'ESTIMATOR_FAILED'
    except Exception as exc:
        result.update(status='RUNNER_FAILED', error=str(exc))
    finally:
        master.terminate()
        try:
            master.wait(timeout=10)
        except subprocess.TimeoutExpired:
            master.kill()
            master.wait()
        master_log.close()
        (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
    return 0 if result['status'] == 'SUCCESS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
