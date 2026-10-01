#!/usr/bin/env python3
"""Run exactly one explicitly authorized Super+COIN TunnelD diagnostic."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import xmlrpc.client

ROOT=Path(__file__).resolve().parents[2]
BAG=Path('/home/lc/algorithm_versa/bag/ENWIDE/2023-08-08-17-50-31-tunnel_d.bag')
OUT=ROOT/'runtime/p2_coin_c2_single'
PORT=11561

def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1<<20),b''):
            h.update(block)
    return h.hexdigest()

def main():
    if OUT.exists():
        raise SystemExit(f'refusing a second one-shot TunnelD run: {OUT} already exists')
    if not BAG.is_file():
        raise SystemExit(f'missing TunnelD bag: {BAG}')
    OUT.mkdir(parents=True)
    env=dict(os.environ,ROS_MASTER_URI=f'http://127.0.0.1:{PORT}',
             ROS_HOSTNAME='127.0.0.1',ROS_HOME=str(OUT/'ros_home'),
             ROS_LOG_DIR=str(OUT/'ros_log'))
    configs={
        'super_tunneld':'tools/cube_lio/config/tunnel_d.yaml',
        'super_photo_defaults':'tools/cube_lio/config/photo.yaml',
        'coin_params':'refs/COIN-LIO/config/params.yaml',
        'coin_line_removal':'refs/COIN-LIO/config/line_removal.yaml',
        'coin_ouster_metadata':'refs/COIN-LIO/config/os_enwide.json',
    }
    config_sha={name:sha(ROOT/path) for name,path in configs.items()}
    source_paths=[
        'src/basic/include/basic/alias.h','src/super_lio/include/common/ds.h',
        'src/super_lio/include/ros/ROSWrapper.h','src/super_lio/src/ros/ROSWrapper.cpp',
        'src/super_lio/include/intensity/coin/coin_observation.hpp',
        'src/super_lio/src/intensity/coin/coin_observation.cpp',
        'src/super_lio/src/lio/super_lio.cpp',
        'src/super_lio/include/intensity/coin/coin_feature_manager.hpp',
        'src/super_lio/src/intensity/coin/coin_feature_manager.cpp',
        'src/super_lio/include/intensity/coin/coin_photometric_model.hpp',
        'src/super_lio/src/intensity/coin/coin_photometric_model.cpp',
    ]
    head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    coin_sha=subprocess.check_output(['git','-C','refs/COIN-LIO','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    meta={
        'phase':'one-shot P2 C2 diagnostic override',
        'dataset':'ENWIDE TunnelD','bag':str(BAG),'bag_sha256':sha(BAG),
        'coin_sha':coin_sha,'source_head':head,'configs':config_sha,
        'run_source_sha256':{path:sha(ROOT/path) for path in source_paths},
        'binary_sha256':sha(ROOT/'devel/lib/super_lio/cube_offline_node'),
        'photo_mode':'coin_faithful','coin_enable':True,'cube_photo_enable':False,
        'official_photo_scale':0.00095,'measurement_variance':0.001,
        'tbb_threads':4,'run_count_authorized':1,
        'p2_3_gate_override':'User explicitly authorized one C2 run despite the failed numerical residual-Jacobian gate; exploratory evidence only.',
        'known_limits':['P2.3 actual residual finite-difference gate failed.',
                        'Per-point acquisition transforms use Super-LIO propagated IMU history and are not parity-checked against official transforms during this run.',
                        'Super scan synchronization and geometry/IMU/ESKF/evaluator settings remain frozen.'],
    }
    (OUT/'identity.json').write_text(json.dumps(meta,indent=2,ensure_ascii=False)+'\n')
    master_log=(OUT/'master.log').open('w')
    master=subprocess.Popen(['roscore','-p',str(PORT)],cwd=ROOT,env=env,stdout=master_log,stderr=master_log)
    result={'node_exit_code':None,'evaluator_exit_code':None,'status':'NOT_STARTED'}
    try:
        for _ in range(150):
            try:
                xmlrpc.client.ServerProxy(env['ROS_MASTER_URI']).getPid('/p2_coin_c2_once')
                break
            except OSError:
                if master.poll() is not None: raise RuntimeError('isolated roscore exited')
                time.sleep(.1)
        else: raise RuntimeError('isolated roscore startup timeout')
        for name in ['super_tunneld','super_photo_defaults','coin_params','coin_line_removal','coin_ouster_metadata']:
            subprocess.run(['rosparam','load',str(ROOT/configs[name])],cwd=ROOT,env=env,check=True)
        params={
            '/lio/offline/bag':str(BAG),'/lio/offline/out_dir':str(OUT),
            '/lio/offline/threads':'4','/photo/enable':'false','/coin/enable':'true',
            '/image/u_shift':'0','/coin/measurement_variance':'0.001',
        }
        for key,value in params.items():
            subprocess.run(['rosparam','set',key,value],cwd=ROOT,env=env,check=True)
        with (OUT/'node.log').open('w') as log:
            node=subprocess.run([str(ROOT/'devel/lib/super_lio/cube_offline_node')],
                cwd=ROOT,env=env,stdout=log,stderr=log)
        result['node_exit_code']=node.returncode
        if node.returncode==0:
            evaluation=subprocess.run(['python3',str(ROOT/'eval/evaluate.py'),'tunnel_d',str(OUT)],
                cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
            (OUT/'evaluator.log').write_text(evaluation.stdout)
            result['evaluator_exit_code']=evaluation.returncode
            result['status']='SUCCESS' if evaluation.returncode==0 else 'EVALUATION_FAILED'
        else:
            result['status']='ESTIMATOR_FAILED'
    except Exception as error:
        result['status']='RUNNER_FAILED'
        result['error']=str(error)
    finally:
        master.terminate()
        try: master.wait(timeout=10)
        except subprocess.TimeoutExpired:
            master.kill();master.wait()
        master_log.close()
        (OUT/'one_shot_result.json').write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n')
    print(json.dumps(result,ensure_ascii=False))
    return 0 if result['status']=='SUCCESS' else 1

if __name__=='__main__':
    raise SystemExit(main())
