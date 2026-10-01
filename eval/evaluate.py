#!/usr/bin/env python3
"""Frozen no-scale SE3 ATE; NTU author association, evo association for stress sets."""
import hashlib
import json
from pathlib import Path
import sys
import numpy as np
from evo.core import sync
import ntu_author as ntu

ROOT = Path(__file__).resolve().parents[1]

def ground_truth(dataset):
    if dataset == 'eee_01':
        cache = ROOT/'runtime/eee_01_gt.tum'
        if not cache.exists():
            import rosbag
            with rosbag.Bag('/home/lc/super_livo/bag/NTU/eee_01/eee_01.bag') as bag, cache.open('w') as out:
                for _,m,_ in bag.read_messages(topics=['/leica/pose/relative']):
                    p,q=m.pose.position,m.pose.orientation
                    out.write(' '.join(format(v,'.17g') for v in [m.header.stamp.to_sec(),p.x,p.y,p.z,q.x,q.y,q.z,q.w])+'\n')
        return cache
    if dataset == 'shield1': return Path('/home/lc/algorithm_versa/bag/GEODE/Shield_tunnel1.txt')
    return Path('/home/lc/algorithm_versa/bag/ENWIDE/gt-tunnel_d.tum')

def main():
    dataset,folder=sys.argv[1:]
    folder=Path(folder)
    if dataset == 'eee_01':
        times,positions,quats=ntu.load_tum(folder/'trajectory.tum')
    else:
        raw=np.loadtxt(folder/'trajectory.tum')
        if not np.all(np.isfinite(raw)) or np.any(np.diff(raw[:,0])<0):
            raise ValueError('invalid estimate')
        times,positions,quats=raw[:,0],raw[:,1:4],raw[:,4:8]
    gt_path=ground_truth(dataset)
    gt=np.loadtxt(gt_path)
    if not np.all(np.isfinite(gt)) or np.any(np.diff(gt[:,0])<=0):
        raise ValueError('invalid GT positions/timestamps')
    if dataset == 'eee_01':
        gt_times,gt_positions=ntu.remove_dataset_position_duplicates(gt[:,0],gt[:,1:4])
        indices,matched_gt=ntu.interpolate_dataset_gt(times,gt_times,gt_positions,max_bracket=.1)
        positions=positions+np.array([ntu.quaternion_to_rotation(q)@ntu.PRISM_B for q in quats])
        estimated=positions[indices]
        association='NTU author linear interpolation strict bracket <0.1s, duplicate removal'
    else:
        if dataset == 'shield1':
            # Official gamma2GT_leica.py: T_eval=T_device @ inverse(T).
            q=np.array([-.00492765,.00575961,.0117651,.999901])
            x,y,z,w=q
            r=np.array([[1-2*y*y-2*z*z,2*x*y-2*z*w,2*x*z+2*y*w],
                        [2*x*y+2*z*w,1-2*x*x-2*z*z,2*y*z-2*x*w],
                        [2*x*z-2*y*w,2*y*z+2*x*w,1-2*x*x-2*y*y]])
            lever=-np.linalg.inv(r)@np.array([.00947221,-.308202,-.365733])
        else: lever=np.array([-.006253,.011775,.10825])
        positions=positions+np.array([ntu.quaternion_to_rotation(q)@lever for q in quats])
        # Stress GT contains position-only Leica data; its quaternion is unused.
        if len(gt)<len(times):
            ids_gt,ids_est=sync.matching_time_indices(gt[:,0],times,max_diff=.1,offset_2=0.)
        else:
            ids_est,ids_gt=sync.matching_time_indices(times,gt[:,0],max_diff=.1,offset_2=0.)
        estimated=positions[ids_est]; matched_gt=gt[ids_gt,1:4]
        association='evo nearest timestamps max_diff=0.1s offset=0'
    rotation,translation=ntu.align_se3(estimated,matched_gt)
    errors=np.linalg.norm(estimated@rotation.T+translation-matched_gt,axis=1)
    result={'dataset':dataset,'alignment':'SE3 no scale','association':association,
            'ate_rmse_m':float(np.sqrt(np.mean(errors**2))), 'ate_mean_m':float(errors.mean()),
            'ate_median_m':float(np.median(errors)), 'ate_max_m':float(errors.max()),
            'matched':len(errors),'duplicate_estimate_timestamps':int(np.sum(np.diff(times)==0)),
            'frames':len(times),'processed_duration_s':float(times[-1]-times[0]),
            'first_timestamp':float(times[0]),'last_timestamp':float(times[-1]),
            'gt_path':str(gt_path),'gt_sha256':hashlib.sha256(gt_path.read_bytes()).hexdigest(),
            'evaluator_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'ntu_author_sha256':hashlib.sha256((ROOT/'eval/ntu_author.py').read_bytes()).hexdigest()}
    (folder/'evaluation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))

if __name__=='__main__': main()
