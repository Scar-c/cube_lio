#!/usr/bin/env python3
"""Verify A/B identity, lifecycle and information diagnostics; write small evidence."""
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess
import numpy as np
import yaml

ROOT=Path(__file__).resolve().parents[2]

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def collect(name):
    p=ROOT/'runtime'/name
    m=json.loads((p/'evaluation.json').read_text())
    m.update(json.loads((p/'run.json').read_text()))
    m['identity']=json.loads((p/'identity.json').read_text())
    m['trajectory_path']=str(p/'trajectory.tum')
    m['trajectory_sha256']=sha(p/'trajectory.tum')
    m['timing_mean_ms']={k:float(v) for k,v in re.findall(r'\[ (\w+) \] average time usage: ([\d.]+)',(p/'node.log').read_text())}
    return m

def photo_diagnostics(name):
    x=np.genfromtxt(ROOT/'runtime'/name/'photo.csv',delimiter=',',names=True)
    cfg=yaml.safe_load((ROOT/'tools/cube_lio/config/photo.yaml').read_text())['photo']
    assert np.all(x['active_before']<=cfg['max_features'])
    assert np.all(x['active_after']<=cfg['max_features'])
    assert x['active_before'][0]==0 and x['valid'][0]==0
    assert np.array_equal(x['active_before'][1:],x['active_after'][:-1])
    assert np.all(x['valid']<=x['active_before'])
    assert np.all(x['trace_Ap'][x['frozen']==0]==0)
    if np.any(x['frozen']): assert len(np.unique(x['sigma'][x['frozen']==1]))==1
    assert np.all(x['trace_Ap']>=0) and np.all(x['photo_in_weak_translation']>=-1e-7)
    assert np.all(x['valid'][x['deskew_supported']==0]==0)
    assert np.all(x['trace_Ap'][x['deskew_supported']==0]==0)
    assert np.array_equal(x['active_before'][x['deskew_supported']==0],x['active_after'][x['deskew_supported']==0])
    statistics={}
    for field in x.dtype.names:
        values=x[field]
        assert np.all(np.isfinite(values)),field
        statistics[field]={'mean':float(values.mean()),'median':float(np.median(values)),
                           'min':float(values.min()),'max':float(values.max())}
    valid=x['trace_Ap']>0
    weak=x['weak_translation_eigenvalue']<=np.quantile(x['weak_translation_eigenvalue'],.1)
    ratios=x['photo_in_weak_translation']/np.maximum(x['weak_translation_eigenvalue'],1.)
    return {'frames':len(x),'statistics':statistics,'sigma_frozen':bool(np.any(x['frozen'])),
            'positive_information_frames':int(valid.sum()),'positive_weak_information_frames':int((x['photo_in_weak_translation']>0).sum()),
            'positive_information_frame_fraction':float(valid.mean()),
            'median_photo_to_weak_geo_information_ratio':float(np.median(ratios[valid])) if np.any(valid) else 0.,
            'lowest_decile_geometry_frames':int(weak.sum()),
            'lowest_decile_photo_weak_info_positive_fraction':float((x['photo_in_weak_translation'][weak]>0).mean()),
            'lowest_decile_photo_to_geo_weak_ratio_median':float(np.median(ratios[weak])),
            'unsupported_deskew_frames':int((x['deskew_supported']==0).sum()),
            'lifecycle_checks':'PASS'}

def main():
    results={'phase':'P1','code_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
             'p0_commit':subprocess.check_output(['git','rev-parse','p0-super-lio-baseline'],cwd=ROOT,text=True).strip(),
             'runs':{},'paired':{},'jacobian_tests':json.loads((ROOT/'artifacts/p1/jacobian_tests.json').read_text())}
    rows=[]
    for dataset in ['eee_01','shield1','tunnel_d']:
        geo_name='p1_final_geo_'+dataset; photo_name='p1_final_photo_'+dataset
        geo=collect(geo_name);photo=collect(photo_name)
        a=np.loadtxt(geo['trajectory_path']);b=np.loadtxt(photo['trajectory_path'])
        assert np.array_equal(a[:,0],b[:,0]),'A/B frame times differ'
        for field in ['evaluator_sha256','ntu_author_sha256','gt_sha256','matched','frames','lidar_read','imu_read']:
            assert geo[field]==photo[field],field
        assert geo['identity']['config_sha256']==photo['identity']['config_sha256']
        assert geo['trajectory_sha256']==sha(ROOT/'runtime'/('p0_serial_'+dataset)/'trajectory.tum'),'photo-off baseline parity'
        assert geo['evaluator_sha256']==sha(ROOT/'eval/evaluate.py')
        photo['diagnostics']=photo_diagnostics(photo_name)
        assert photo['diagnostics']['frames']==photo['frames']
        delta=photo['ate_rmse_m']-geo['ate_rmse_m'];percent=100*delta/geo['ate_rmse_m']
        gate=percent<=(5 if dataset=='eee_01' else 10)
        results['runs'][dataset]={'geo':geo,'photo':photo}
        results['paired'][dataset]={'absolute_delta_m':delta,'relative_delta_percent':percent,
            'gate_pass':bool(gate),'frame_timestamp_parity':'PASS','photo_off_pristine_byte_parity':'PASS',
            'wall_overhead_percent':100*(photo['wall_processing_s']/geo['wall_processing_s']-1)}
        for mode,m in [('geo',geo),('photo',photo)]:
            rows.append([dataset,mode,mode=='photo',True,m['ate_rmse_m'],m['frames'],m['wall_processing_s']])
    ntu=results['paired']['eee_01']['gate_pass']
    stress=all(results['paired'][d]['gate_pass'] for d in ['shield1','tunnel_d'])
    improve=any(results['paired'][d]['absolute_delta_m']<0 for d in ['shield1','tunnel_d'])
    results['numerical_acceptance_pass']=bool(ntu and stress and improve)
    results['verdict']='PASS' if ntu and stress and improve else ('NO-GO' if not ntu else 'PARTIAL / NO-GO')
    ablation=ROOT/'runtime/p1_ablation_eee_idw_off/evaluation.json'
    if ablation.exists():
        m=collect('p1_ablation_eee_idw_off');m['diagnostics']=photo_diagnostics('p1_ablation_eee_idw_off')
        results['idw_off_ablation']=m;rows.append(['eee_01','photo_idw_off',True,False,m['ate_rmse_m'],m['frames'],m['wall_processing_s']])
    (ROOT/'artifacts/p1/metrics.json').write_text(json.dumps(results,indent=2)+'\n')
    with (ROOT/'artifacts/p1/ablation_summary.csv').open('w') as f:
        w=csv.writer(f);w.writerow(['dataset','mode','photo_enable','idw_enable','ate_rmse_m','frames','wall_processing_s']);w.writerows(rows)
    print(json.dumps({'verdict':results['verdict'],'paired':results['paired']},indent=2))

if __name__=='__main__':main()
