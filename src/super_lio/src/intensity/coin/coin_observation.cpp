#include "intensity/coin/coin_observation.hpp"

#include "lio/params.h"
#include <ros/ros.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cube::coin {
namespace {
using Mat3=Eigen::Matrix3d;
using Vec3d=Eigen::Vector3d;

Eigen::Matrix4d transform(const Mat3& R,const Vec3d& t){
  Eigen::Matrix4d T=Eigen::Matrix4d::Identity();
  T.topLeftCorner<3,3>()=R;T.topRightCorner<3,1>()=t;return T;
}

bool stateAt(const std::vector<LI2Sup::DynamicState>& history,double time,
             Mat3& R,Vec3d& p){
  if(history.size()<2||time>history.back().time)return false;
  auto tail=std::upper_bound(history.begin(),history.end(),time,
      [](double t,const LI2Sup::DynamicState& s){return t<s.time;});
  if(tail==history.end()){
    const auto& last=history.back();R=last.R.cast<double>();p=last.p.cast<double>();return true;
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
}

CoinObservation::CoinObservation(ros::NodeHandle& nh,const Eigen::Matrix4d& T_IL)
  :T_IL_(T_IL){
  nh.param("/coin/enable",enabled_,false);
  if(!enabled_)return;
  if(LI2Sup::g_lidar_type!=LI2Sup::LID_TYPE::OUSTER)
    throw std::runtime_error("faithful COIN observation currently requires Ouster");
  bool photo_enabled=false;nh.param("/photo/enable",photo_enabled,false);
  if(photo_enabled)throw std::runtime_error("enable either /coin/enable or /photo/enable, not both");
  nh.param("/filter/photo_scale",photo_scale_,0.00095);
  nh.param("/coin/measurement_variance",measurement_variance_,0.001);
  double sensor_z_offset=0.03618;
  std::vector<double> lidar_to_sensor;
  if((nh.getParam("/lidar_to_sensor_transform",lidar_to_sensor)||
      nh.getParam("/lidar_intrinsics/lidar_to_sensor_transform",lidar_to_sensor))&&
     lidar_to_sensor.size()==16)
    sensor_z_offset=lidar_to_sensor[11]*0.001;
  // The COIN preprocessor subtracts this sensor-origin offset from z. Adjust
  // the sensor-to-IMU translation so the transformed physical point is the
  // same point used by Super-LIO's unshifted geometry cloud.
  T_IL_.topRightCorner<3,1>()+=T_IL_.topLeftCorner<3,3>()*Eigen::Vector3d(0.,0.,sensor_z_offset);
  if(!(photo_scale_>0.)||!(measurement_variance_>0.))
    throw std::invalid_argument("invalid COIN scale or measurement variance");
  const OusterMetadata metadata=OusterMetadata::fromRosParams();
  projector_=std::make_unique<CoinOusterProjector>(metadata);
  image_settings_=CoinImageSettings::fromRosParams();
  feature_settings_=CoinFeatureSettings::fromRosParams();
  image_processor_=std::make_unique<CoinImageProcessor>(*projector_,image_settings_);
  feature_manager_=std::make_unique<CoinFeatureManager>(*projector_,feature_settings_);
  std::string output_dir;
  nh.getParam("/lio/offline/out_dir",output_dir);
  if(!output_dir.empty()){
    diagnostics_.open(output_dir+"/coin_observation.csv");
    if(!diagnostics_)throw std::runtime_error("cannot write COIN observation diagnostics");
    diagnostics_<<"frame,timestamp,raw_points,motion_fallback_points,coin_minus_super_scan_end_s,active_before,valid_patches,photo_rows,residual_rms,photo_A_norm,photo_b_norm,weak_dirs,active_after,added,removed,status,skip_reason\n";
  }
}

Eigen::Matrix4d CoinObservation::poseMatrix(const BASIC::SE3& pose) const{
  Eigen::Matrix4d T=Eigen::Matrix4d::Identity();
  T.topLeftCorner<3,3>()=pose.R_.cast<double>();
  T.topRightCorner<3,1>()=pose.t_.cast<double>();
  return T;
}

void CoinObservation::prepare(const LI2Sup::LidarData& lidar,
                              const std::vector<LI2Sup::DynamicState>& history,
                              const BASIC::SE3& predicted_pose){
  if(!enabled_)return;
  prepared_=false;
  double max_offset=0.;
  for(const auto& raw:lidar.coin_raw_points)max_offset=std::max(max_offset,raw.offset_time);
  coin_scan_end_delta_s_=lidar.coin_raw_points.empty()?0.:
    max_offset-(lidar.end_time-lidar.start_time);
  const char* skip_reason=nullptr;
  if(lidar.coin_raw_points.empty())skip_reason="no_raw_ouster_samples";
  else if(history.size()<2)skip_reason="insufficient_propagated_imu_history";
  if(skip_reason){
    const int active=static_cast<int>(feature_manager_->features().size());
    if(diagnostics_){
      diagnostics_<<scan_index_<<','<<lidar.end_time<<','<<lidar.coin_raw_points.size()
        <<",0,"<<coin_scan_end_delta_s_<<','<<active<<",0,0,0,0,0,0,"<<active
        <<",0,0,SKIPPED,"<<skip_reason<<'\n';
    }
    ROS_WARN_THROTTLE(5.0,"COIN observation skipped at scan %zu: %s; geometric update continues",
                      scan_index_,skip_reason);
    ++scan_index_;
    return;
  }
  const Eigen::Matrix4d T_GI=poseMatrix(predicted_pose);
  const Mat3 R_GI=T_GI.topLeftCorner<3,3>();
  const Vec3d t_GI=T_GI.topRightCorner<3,1>();
  const Mat3 R_IL=T_IL_.topLeftCorner<3,3>();
  const Vec3d t_IL=T_IL_.topRightCorner<3,1>();
  const Mat3 R_LI=R_IL.transpose();
  const Mat3 R_GL_end=R_GI*R_IL;
  const Vec3d t_GL_end=R_GI*t_IL+t_GI;

  points_.clear();points_.reserve(lidar.coin_raw_points.size());
  motion_fallback_points_=0;
  std::vector<Eigen::Matrix4d> transforms;transforms.reserve(lidar.coin_raw_points.size());
  std::vector<int> transform_indices;transform_indices.reserve(lidar.coin_raw_points.size());
  for(const auto& raw:lidar.coin_raw_points){
    CoinScanPoint point;
    point.point_lidar=Vec3(raw.x,raw.y,raw.z);
    point.intensity=raw.intensity;point.range=raw.range;
    point.raw_index=raw.raw_index;point.offset_seconds=raw.offset_time;
    Mat3 R_GI_acq;Vec3d t_GI_acq;
    Eigen::Matrix4d T_Li_Lk=Eigen::Matrix4d::Identity();
    if(stateAt(history,lidar.start_time+raw.offset_time,R_GI_acq,t_GI_acq)){
      const Mat3 R_GL_acq=R_GI_acq*R_IL;
      const Vec3d t_GL_acq=R_GI_acq*t_IL+t_GI_acq;
      const Vec3d p_global=R_GI_acq*(R_IL*point.point_lidar+t_IL)+t_GI_acq;
      point.point_lidar=R_GL_end.transpose()*(p_global-t_GL_end);
      T_Li_Lk.topLeftCorner<3,3>()=R_GL_acq.transpose()*R_GL_end;
      T_Li_Lk.topRightCorner<3,1>()=R_GL_acq.transpose()*(t_GL_end-t_GL_acq);
    }else{
      // Match Super-LIO's existing post-history fallback: retain the raw scan point.
      ++motion_fallback_points_;
      point.point_lidar=R_LI*(R_GI.transpose()*(R_IL*point.point_lidar+t_IL)-t_IL);
    }
    points_.push_back(point);
    transforms.push_back(T_Li_Lk);
    transform_indices.push_back(static_cast<int>(transform_indices.size()));
  }
  frame_=image_processor_->process(points_);
  frame_.T_Li_Lk_vec=std::move(transforms);
  frame_.vec_idx=std::move(transform_indices);
  prepared_=true;active_before_=static_cast<int>(feature_manager_->features().size());
  valid_patches_=photo_rows_=0;residual_square_sum_=photo_A_norm_=photo_b_norm_=0.;
}

void CoinObservation::add(const BASIC::SE3& pose,BASIC::M6& A,BASIC::V6& b){
  if(!enabled_||!prepared_)return;
  const auto T_GI=poseMatrix(pose);
  Eigen::Matrix<double,6,6> photo_A=Eigen::Matrix<double,6,6>::Zero();
  Eigen::Matrix<double,6,1> photo_b=Eigen::Matrix<double,6,1>::Zero();
  valid_patches_=photo_rows_=0;residual_square_sum_=0.;
  const auto& features=feature_manager_->features();
  for(const auto& feature:features){
    std::vector<CoinPhotoLinearization> patch;
    patch.reserve(feature.points_global.size());bool valid=true;
    for(std::size_t i=0;i<feature.points_global.size();++i){
      auto row=CoinPhotometricModel::linearize(*feature_manager_,*projector_,frame_,points_,
          T_GI,T_IL_,feature.points_global[i],feature.reference_intensity[i],
          feature_settings_.min_range,feature_settings_.max_range,feature_settings_.margin);
      if(!row.valid){valid=false;break;}
      patch.push_back(row);
    }
    if(!valid||patch.size()!=feature.points_global.size())continue;
    ++valid_patches_;
    for(const auto& row:patch){
      const Eigen::Matrix<double,1,6> H=row.correction_jacobian_super;
      photo_A.noalias()+=H.transpose()*H;
      photo_b.noalias()+=H.transpose()*row.residual;
      residual_square_sum_+=row.residual*row.residual;++photo_rows_;
    }
  }
  const double factor=photo_scale_*photo_scale_/measurement_variance_;
  photo_A*=factor;photo_b*=factor;
  photo_A_norm_=photo_A.norm();photo_b_norm_=photo_b.norm();
  A+=photo_A.cast<BASIC::scalar>();b+=photo_b.cast<BASIC::scalar>();
}

void CoinObservation::finish(const BASIC::SE3& pose,double timestamp,
                             const Eigen::MatrixXd& geometry_translation_rows){
  if(!enabled_||!prepared_)return;
  const Eigen::Matrix4d T_GI=poseMatrix(pose);
  const Eigen::Matrix3d R_GL=T_GI.topLeftCorner<3,3>()*T_IL_.topLeftCorner<3,3>();
  const Eigen::Vector3d t_GL=T_GI.topLeftCorner<3,3>()*T_IL_.topRightCorner<3,1>()+
                             T_GI.topRightCorner<3,1>();
  const Eigen::Matrix4d T_GL=transform(R_GL,t_GL);
  const auto weak=CoinFeatureManager::weakDirectionsFromGeometry(
      geometry_translation_rows,R_GL,feature_settings_.n_uninformative);
  feature_manager_->update(frame_,points_,weak.lidar,T_GL);
  const auto& stats=feature_manager_->lastStats();
  if(diagnostics_){
    const double rms=photo_rows_?std::sqrt(residual_square_sum_/photo_rows_):0.;
    diagnostics_<<scan_index_<<','<<timestamp<<','<<points_.size()<<','<<motion_fallback_points_<<','
      <<coin_scan_end_delta_s_<<','<<active_before_<<','
      <<valid_patches_<<','<<photo_rows_<<','<<rms<<','<<photo_A_norm_<<','<<photo_b_norm_<<','<<weak.lidar.size()<<','
      <<feature_manager_->features().size()<<','<<stats.added<<','<<stats.removed<<",USED,\n";
  }
  ++scan_index_;prepared_=false;
}

} // namespace cube::coin
