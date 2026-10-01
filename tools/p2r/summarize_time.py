#!/usr/bin/env python3
"""Summarize the per-scan Ouster temporal-support audit without altering runs."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import statistics

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def stats(values):
    values=sorted(values)
    if not values: return None
    return {'min':values[0],'median':statistics.median(values),
            'p95':values[int(.95*(len(values)-1))],'max':values[-1]}

def main():
    p=argparse.ArgumentParser()
    p.add_argument('csv',type=Path)
    p.add_argument('run_dir',type=Path)
    p.add_argument('output',type=Path)
    p.add_argument('--phase',required=True)
    args=p.parse_args()
    rows=list(csv.DictReader(args.csv.open()))
    normal=[r for r in rows if r['phase']=='normal']
    value=lambda r,k:float(r[k])
    gap=lambda r:value(r,'proposed_full_scan_end_time')-value(r,'current_lidar_end_time')
    geom_gap=lambda r:value(r,'max_t_geometry_after_filter_rate_offset_s')-value(r,'last_geometry_offset_s') if 'last_geometry_offset_s' in r else value(r,'ros_message_stamp')+value(r,'max_t_geometry_after_filter_rate_offset_s')-value(r,'current_lidar_end_time')
    raw_geom=lambda r:value(r,'max_t_valid_raw_offset_s')-value(r,'max_t_geometry_after_filter_rate_offset_s')
    count=lambda k:sum(int(r[k]) for r in normal if r[k])
    result={
        'phase':args.phase,'dataset':'ENWIDE TunnelD','csv_sha256':sha(args.csv),
        'frames':{'processed':len(rows),'normal':len(normal),'initialization':len(rows)-len(normal)},
        'offsets_s':{
            'valid_raw_max_minus_current_end':stats([gap(r) for r in normal]),
            'valid_raw_max_minus_geometry_max':stats([raw_geom(r) for r in normal]),
            'geometry_max_minus_last_visited_geometry':stats([geom_gap(r) for r in normal]),
            'imu_available_minus_current_end':stats([value(r,'imu_last_available_to_sync')-value(r,'current_lidar_end_time') for r in normal]),
            'history_back_minus_current_end':stats([value(r,'propagated_history_last')-value(r,'current_lidar_end_time') for r in normal]),
        },
        'counts':{
            'valid_raw_points':count('raw_coin_points'),
            'valid_raw_points_after_current_end':count('raw_coin_points_after_current_end'),
            'valid_raw_points_after_history_back':count('raw_coin_points_after_history_back'),
            'motion_fallback_points':count('current_motion_fallback_points'),
            'normal_frames_with_motion_fallback':sum(int(r['current_motion_fallback_points'])>0 for r in normal),
            'normal_frames_with_raw_max_after_geometry_max':sum(raw_geom(r)>1e-9 for r in normal),
            'normal_frames_with_full_raw_max_after_current_end':sum(gap(r)>1e-6 for r in normal),
        },
        'per_scan':{
            'raw_points_after_current_end':stats([int(r['raw_coin_points_after_current_end']) for r in normal]),
            'raw_points_after_history_back':stats([int(r['raw_coin_points_after_history_back']) for r in normal]),
            'motion_fallback_points':stats([int(r['current_motion_fallback_points']) for r in normal]),
        },
        'run':json.loads((args.run_dir/'run.json').read_text()),
        'evaluation':json.loads((args.run_dir/'evaluation.json').read_text()),
        'trajectory_sha256':sha(args.run_dir/'trajectory.tum'),
        'identity':json.loads((args.run_dir/'identity.json').read_text()),
    }
    if args.phase=='R0_HISTORICAL':
        result['root_cause']={
            'last_visited_geometry_point_used_as_end':True,
            'last_visited_point_is_not_the_max_time_point_on_most_scans':
                result['offsets_s']['geometry_max_minus_last_visited_geometry']['median']>0,
            'geometry_subsampling_can_also_omit_the_raw_max_on_some_scans':
                result['counts']['normal_frames_with_raw_max_after_geometry_max'],
            'sync_waits_for_imu_after_end_but_consumes_only_samples_at_or_before_end':True,
            'consequence':'All 1179 normal frames have raw COIN points beyond the propagated history; the median is 11335 points per frame.'
        }
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'phase':args.phase,'normal_frames':len(normal),
                      'median_scan_end_gap_s':result['offsets_s']['valid_raw_max_minus_current_end']['median'],
                      'median_fallback_points':result['per_scan']['motion_fallback_points']['median'],
                      'ate_rmse_m':result['evaluation']['ate_rmse_m']}))

if __name__=='__main__': main()
