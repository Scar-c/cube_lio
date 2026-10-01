#!/usr/bin/env python3
"""Replay a complete frozen dataset, with isolated ROS master and local outputs."""
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
    'eee_01': '/home/lc/super_livo/bag/NTU/eee_01/eee_01.bag',
    'shield1': '/home/lc/algorithm_versa/bag/GEODE/Shield_tunnel1_gamma.bag',
    'tunnel_d': '/home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag',
}

def main():
    p = argparse.ArgumentParser()
    p.add_argument('dataset', choices=BAGS)
    p.add_argument('--name', required=True)
    p.add_argument('--photo', action='store_true')
    p.add_argument('--idw-off', action='store_true')
    p.add_argument('--port', type=int, default=11431)
    a = p.parse_args()
    out = ROOT / 'runtime' / a.name
    out.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ, ROS_MASTER_URI=f'http://127.0.0.1:{a.port}',
               ROS_HOSTNAME='127.0.0.1', ROS_HOME=str(out/'ros_home'),
               ROS_LOG_DIR=str(out/'ros_log'))
    config = ROOT / 'tools/cube_lio/config' / f'{a.dataset}.yaml'
    meta = {'dataset': a.dataset, 'bag': BAGS[a.dataset], 'photo': a.photo,
            'idw_enable': not a.idw_off, 'config_sha256': hashlib.sha256(config.read_bytes()).hexdigest(),
            'head': subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
            'threads': 4}
    (out/'identity.json').write_text(json.dumps(meta,indent=2)+'\n')
    with (out/'master.log').open('w') as log:
        master = subprocess.Popen(['roscore','-p',str(a.port)],env=env,stdout=log,stderr=log)
        try:
            for _ in range(100):
                try:
                    xmlrpc.client.ServerProxy(env['ROS_MASTER_URI']).getPid('/cube_runner')
                    break
                except OSError:
                    if master.poll() is not None: raise RuntimeError('roscore exited')
                    time.sleep(.1)
            else: raise RuntimeError('roscore startup timeout')
            subprocess.run(['rosparam','load',str(config)],env=env,check=True)
            for key,value in {'bag':BAGS[a.dataset], 'out_dir':str(out), 'threads':'4'}.items():
                subprocess.run(['rosparam','set',f'/lio/offline/{key}',value],env=env,check=True)
            photo_config = ROOT/'tools/cube_lio/config/photo.yaml'
            if photo_config.exists(): subprocess.run(['rosparam','load',str(photo_config)],env=env,check=True)
            subprocess.run(['rosparam','set','/photo/enable',str(a.photo).lower()],env=env,check=True)
            if a.idw_off: subprocess.run(['rosparam','set','/cubemap/idw_enable','false'],env=env,check=True)
            with (out/'node.log').open('w') as node_log:
                subprocess.run([str(ROOT/'devel/lib/super_lio/cube_offline_node')],cwd=ROOT,
                               env=env,stdout=node_log,stderr=node_log,check=True)
            subprocess.run(['python3',str(ROOT/'eval/evaluate.py'),a.dataset,str(out)],env=env,check=True)
        finally:
            master.terminate()
            try: master.wait(timeout=10)
            except subprocess.TimeoutExpired:
                master.kill(); master.wait()
    print(out)

if __name__ == '__main__': main()
