#!/usr/bin/env python3
"""Archive the P4 stopped mathematical phase without inventing missing results."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
BASELINE = 'f3060768abee4d2ee2343b07edf62346c7cd0f87'


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True)


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1 << 20), b''):
            value.update(block)
    return value.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--paper', type=Path)
    args = parser.parse_args()
    papers = list((ROOT.parents[1] / 'dis').glob('*.pdf'))
    paper = args.paper or (papers[0] if len(papers) == 1 else None)
    if paper is None or not paper.is_file():
        parser.error('provide --paper with the local CUBE-LIO PDF')
    out = ROOT / 'runtime/p4_review'
    out.mkdir(parents=True, exist_ok=True)
    paths = set()
    for folder in ('src/super_lio/include/intensity', 'src/super_lio/src/intensity',
                   'tools/offline', 'tools/p4', 'tools/cube_lio/config', 'spec/p4',
                   'artifacts/p4'):
        paths.update(p for p in (ROOT / folder).rglob('*') if p.is_file()
                     and '__pycache__' not in p.parts)
    for filename in (
            'src/super_lio/CMakeLists.txt',
            'src/super_lio/include/lio/super_lio.h',
            'src/super_lio/include/lio/ESKF.h',
            'src/super_lio/include/lio/params.h',
            'src/super_lio/include/lio/ouster_scan_history.hpp',
            'src/super_lio/include/common/ds.h',
            'src/super_lio/src/lio/super_lio.cpp',
            'src/super_lio/src/lio/ESKF.cpp',
            'src/super_lio/src/lio/params.cpp',
            'src/super_lio/src/ros/ROSWrapper.cpp',
            'src/super_lio/src/apps/cube_offline_node.cpp',
            'src/super_lio/test/test_paper_cube.cpp',
            'src/super_lio/test/test_cube_photo.cpp',
            'src/super_lio/test/test_spherical_photo.cpp',
            'src/super_lio/test/test_coin_feature_math.cpp',
            'src/super_lio/test/test_ouster_time_support.cpp',
            'src/basic/include/basic/Manifold.h',
            'eval/evaluate.py', 'eval/ntu_author.py',
            'refs/COIN-LIO/config/params.yaml',
            'refs/COIN-LIO/config/line_removal.yaml',
            'refs/COIN-LIO/config/os_enwide.json'):
        path = ROOT / filename
        if not path.is_file():
            raise FileNotFoundError(path)
        paths.add(path)
    for run in ('p4_control_super_off', 'p4_control_coin_g1'):
        for filename in ('identity.json', 'evaluation.json', 'result.json', 'run.json',
                         'trajectory.tum', 'time_audit.csv', 'coin_observation.csv',
                         'node.log', 'evaluator.log'):
            path = ROOT / 'runtime' / run / filename
            if path.is_file():
                paths.add(path)
    # Preserve failed sandbox startup as an infrastructure event, not an accuracy run.
    for filename in ('identity.json', 'result.json', 'master.log'):
        path = ROOT / 'runtime/p4_baseline_super_off' / filename
        if path.is_file():
            paths.add(path)
    for path in out.rglob('*'):
        if path.is_file() and path.suffix in ('.log', '.json', '.csv', '.txt', '.md'):
            if not path.name.startswith('P4_FAITHFUL_CUBE_'):
                paths.add(path)
    target = git('rev-parse', 'HEAD').strip()
    manifest = {
        'verdict': 'BLOCKED — PAPER/IMPLEMENTATION SEMANTICS NOT YET RECONCILED',
        'baseline': BASELINE, 'target': target,
        'branch': git('branch', '--show-current').strip(),
        'remote': git('remote', '-v'), 'status': git('status', '--short'),
        'commits': git('log', '--format=%H %s', f'{BASELINE}..HEAD'),
        'paper': {'path': str(paper), 'sha256': digest(paper)},
        'binary_sha256': digest(ROOT / 'devel/lib/super_lio/cube_offline_node'),
        'library_sha256': digest(ROOT / 'devel/lib/liblio.so'),
        'math_binary_sha256': digest(ROOT / 'devel/lib/super_lio/test_paper_cube'),
        'files': {str(p.relative_to(ROOT)): digest(p) for p in sorted(paths)},
        'unavailable': [
            'semi-dense CUBE selector and persistent feature manager',
            'CUBE production dense deskew/prefilter integration',
            'CUBE joint state observation registration and production CSV',
            'CUBE trajectory, ATE and ablations: Gate A failed',
            'Livox time repair and secondary validation'],
    }
    manifest_path = out / 'P4_FAITHFUL_CUBE_MANIFEST.json'
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    sections = [
        ('IDENTITY AND AVAILABILITY', json.dumps(manifest, indent=2)),
        ('BASELINE TO TARGET DIFF STAT', git('diff', '--stat', BASELINE, target)),
        ('FULL BASELINE TO TARGET PATCH', git('diff', '--binary', BASELINE, target)),
    ]
    for path in sorted(paths):
        sections.append((str(path.relative_to(ROOT)), path.read_text()))
    bundle = out / 'P4_FAITHFUL_CUBE_REVIEW.txt'
    with bundle.open('w') as target_file:
        for title, contents in sections:
            target_file.write(f'\n===== {title} =====\n{contents}\n')
    archive = out / 'P4_FAITHFUL_CUBE_AUDIT.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
        for path in sorted(paths):
            output.write(path, str(path.relative_to(ROOT)))
        output.write(paper, 'paper/' + paper.name)
        output.write(manifest_path, manifest_path.name)
        output.write(bundle, bundle.name)
    summary = {'target': target, 'status': manifest['status'],
               'text_bytes': bundle.stat().st_size, 'zip_bytes': archive.stat().st_size,
               'text_sha256': digest(bundle), 'zip_sha256': digest(archive)}
    (out / 'P4_FAITHFUL_CUBE_BUNDLE_SHA.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
