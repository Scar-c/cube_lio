#pragma once

#include "common/ds.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <string>

namespace LI2Sup {

struct OusterScanTimeSupport {
  bool has_valid_raw=false;
  std::size_t valid_raw_points=0;
  double max_offset_s=0.;
};

// Match the pinned COIN Ouster raw-point eligibility: reject NaN coordinates
// and ranges below /preprocess/blind, then take the maximum raw point.t.
template<class Points>
OusterScanTimeSupport ousterScanTimeSupport(const Points& points,double blind){
  OusterScanTimeSupport result;
  for(const auto& pt:points){
    if(std::isnan(pt.x)||std::isnan(pt.y)||std::isnan(pt.z))continue;
    const double range=std::sqrt(double(pt.x)*pt.x+double(pt.y)*pt.y+double(pt.z)*pt.z);
    if(range<blind)continue;
    ++result.valid_raw_points;
    result.has_valid_raw=true;
    result.max_offset_s=std::max(result.max_offset_s,pt.t*1e-9);
  }
  return result;
}

inline bool imuHasReachedScanEnd(double latest_imu_time,double scan_end){
  return latest_imu_time>=scan_end;
}

struct OusterImuWindow {
  std::deque<IMUData> samples;
  IMUData end_sample;
  bool has_end_sample=false;
  double bracket_span_s=0.;
  std::string issue;
};

// Retain the right IMU sample in the buffer. Interpolate at scan end only
// when real samples bracket it within the allowed IMU gap; never extrapolate.
inline OusterImuWindow collectOusterImuWindow(std::deque<IMUData>& buffer,
    double scan_end,const IMUData* previous_end,double max_bracket_span_s=0.05){
  OusterImuWindow result;
  while(!buffer.empty()&&buffer.front().secs<=scan_end){
    result.samples.push_back(buffer.front());
    buffer.pop_front();
  }
  if(!result.samples.empty()&&result.samples.back().secs==scan_end){
    result.end_sample=result.samples.back();
    result.has_end_sample=true;
    return result;
  }
  const IMUData* left=result.samples.empty()?previous_end:&result.samples.back();
  if(!left){result.issue="missing_imu_left_bracket";return result;}
  if(buffer.empty()){result.issue="missing_imu_right_bracket";return result;}
  const auto& right=buffer.front();
  result.bracket_span_s=right.secs-left->secs;
  if(!(left->secs<=scan_end&&scan_end<right.secs&&
       result.bracket_span_s>0.&&result.bracket_span_s<=max_bracket_span_s)){
    result.issue="imu_bracket_gap_or_order_invalid";
    return result;
  }
  const double alpha=(scan_end-left->secs)/result.bracket_span_s;
  result.end_sample.secs=scan_end;
  result.end_sample.acc=left->acc+(right.acc-left->acc)*alpha;
  result.end_sample.gyr=left->gyr+(right.gyr-left->gyr)*alpha;
  result.samples.push_back(result.end_sample);
  result.has_end_sample=true;
  return result;
}

} // namespace LI2Sup
