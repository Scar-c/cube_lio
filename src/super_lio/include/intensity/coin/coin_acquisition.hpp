#pragma once

#include "common/ds.h"
#include <algorithm>

namespace cube::coin {

inline std::string coinFrameInputIssue(const LI2Sup::LidarData& lidar,
    const std::vector<LI2Sup::DynamicState>& history){
  if(lidar.coin_raw_points.empty())return "no_raw_ouster_samples";
  if(!lidar.imu_support_issue.empty())return lidar.imu_support_issue;
  if(history.size()<2)return "insufficient_propagated_imu_history";
  return {};
}

inline bool stateAt(const std::vector<LI2Sup::DynamicState>& history,double time,
                    Eigen::Matrix3d& R,Eigen::Vector3d& p){
  constexpr double tolerance=1e-6;
  if(history.size()<2||time<history.front().time-tolerance||
     time>history.back().time+tolerance)return false;
  time=std::clamp(time,history.front().time,history.back().time);
  auto tail=std::upper_bound(history.begin(),history.end(),time,
      [](double t,const LI2Sup::DynamicState& s){return t<s.time;});
  if(tail==history.end()){
    const auto& last=history.back();R=last.R.cast<double>();p=last.p.cast<double>();
    return R.allFinite()&&p.allFinite();
  }
  auto head=tail==history.begin()?tail:std::prev(tail);
  if(head==tail){
    auto next=std::next(tail);if(next==history.end())return false;tail=next;
  }
  const double dt=tail->time-head->time;
  if(!(dt>0.))return false;
  const double tau=time-head->time,s=tau/dt;
  Eigen::Quaterniond q0(head->R.cast<double>()),q1(tail->R.cast<double>());
  R=q0.slerp(s,q1).toRotationMatrix();
  p=head->p.cast<double>()+head->v.cast<double>()*tau+
    0.5*tail->a.cast<double>()*tau*tau;
  return R.allFinite()&&p.allFinite();
}

inline Eigen::Matrix4d lidarAcquisitionToEnd(const Eigen::Matrix3d& R_acq,
    const Eigen::Vector3d& t_acq,const Eigen::Matrix3d& R_end,
    const Eigen::Vector3d& t_end){
  Eigen::Matrix4d T=Eigen::Matrix4d::Identity();
  T.topLeftCorner<3,3>()=R_acq.transpose()*R_end;
  T.topRightCorner<3,1>()=R_acq.transpose()*(t_end-t_acq);
  return T;
}

} // namespace cube::coin
