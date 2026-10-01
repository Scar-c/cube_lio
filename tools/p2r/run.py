#!/usr/bin/env python3
"""Isolated P2R offline replay with frozen configs and optional time audit."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import xmlrpc.client

ROOT=Path(__file__).resolve().parents[2]
BAGS={
    'tunnel_d':Path('/home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag'),
    'eee_01':Path('/home/lc/super_livo/bag/NTU/eee_01/eee_01.bag'),
    'shield1':Path('/home/lc/algorithm_versa/bag/GEODE/Shield_tunnel1_gamma.bag'),
}

def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda:source.read(1<<20),b''): h.update(block)
    return h.hexdigest()

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('dataset',choices=BAGS)
    parser.add_argument('--name',required=True)
    parser.add_argument('--port',type=int,default=11562)
    parser.add_argument('--coin',action='store_true')
    parser.add_argument('--audit-csv',type=Path)
    parser.add_argument('--fusion-audit-json',type=Path)
    args=parser.parse_args()
    if args.coin and args.dataset!='tunnel_d':
        parser.error('COIN production validation is scoped to TunnelD')
    out=ROOT/'runtime'/args.name
    out.mkdir(parents=True,exist_ok=False)
    env=dict(os.environ,ROS_MASTER_URI=f'http://127.0.0.1:{args.port}',
             ROS_HOSTNAME='127.0.0.1',ROS_HOME=str(out/'ros_home'),
             ROS_LOG_DIR=str(out/'ros_log'))
    config=ROOT/'tools/cube_lio/config'/f'{args.dataset}.yaml'
    photo_config=ROOT/'tools/cube_lio/config/photo.yaml'
    coin_configs=[ROOT/'refs/COIN-LIO/config'/name for name in
                  ('params.yaml','line_removal.yaml','os_enwide.json')]
    paths=[config,photo_config]+(coin_configs if args.dataset=='tunnel_d' else [])
    identity={'dataset':args.dataset,'bag':str(BAGS[args.dataset]),
              'coin':args.coin,'audit_csv':str(args.audit_csv) if args.audit_csv else None,
              'fusion_audit_json':str(args.fusion_audit_json) if args.fusion_audit_json else None,
              'source_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
              'config_sha256':{str(p.relative_to(ROOT)):sha(p) for p in paths},
              'binary_sha256':sha(ROOT/'devel/lib/super_lio/cube_offline_node'),
              'threads':4}
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    master_log=(out/'master.log').open('w')
    master=subprocess.Popen(['roscore','-p',str(args.port)],cwd=ROOT,env=env,
                            stdout=master_log,stderr=master_log)
    result={'status':'NOT_STARTED','node_exit_code':None,'evaluator_exit_code':None}
    try:
        for _ in range(150):
            try:
                xmlrpc.client.ServerProxy(env['ROS_MASTER_URI']).getPid('/p2r_runner')
                break
            except OSError:
                if master.poll() is not None: raise RuntimeError('roscore exited')
                time.sleep(.1)
        else: raise RuntimeError('roscore startup timeout')
        for path in paths: subprocess.run(['rosparam','load',str(path)],env=env,check=True)
        params={
            '/lio/offline/bag':str(BAGS[args.dataset]),
            '/lio/offline/out_dir':str(out),'/lio/offline/threads':'4',
            '/photo/enable':'false','/coin/enable':str(args.coin).lower(),
            '/image/u_shift':'0','/coin/measurement_variance':'0.001',
            '/p2r/time_audit_path':str(args.audit_csv.resolve()) if args.audit_csv else '',
            '/p2r/fusion_audit_path':str(args.fusion_audit_json.resolve()) if args.fusion_audit_json else '',
        }
        for key,value in params.items():
            subprocess.run(['rosparam','set',key,value],env=env,check=True)
        with (out/'node.log').open('w') as log:
            node=subprocess.run([str(ROOT/'devel/lib/super_lio/cube_offline_node')],
                                cwd=ROOT,env=env,stdout=log,stderr=log)
        result['node_exit_code']=node.returncode
        if node.returncode==0:
            with (out/'evaluator.log').open('w') as log:
                evaluator=subprocess.run(['python3',str(ROOT/'eval/evaluate.py'),
                                          args.dataset,str(out)],cwd=ROOT,env=env,
                                         stdout=log,stderr=subprocess.STDOUT)
            result['evaluator_exit_code']=evaluator.returncode
            result['status']='SUCCESS' if evaluator.returncode==0 else 'EVALUATION_FAILED'
        else: result['status']='ESTIMATOR_FAILED'
    except Exception as exc:
        result.update(status='RUNNER_FAILED',error=str(exc))
    finally:
        master.terminate()
        try: master.wait(timeout=10)
        except subprocess.TimeoutExpired:
            master.kill();master.wait()
        master_log.close()
        (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))
    return 0 if result['status']=='SUCCESS' else 1

if __name__=='__main__': raise SystemExit(main())
