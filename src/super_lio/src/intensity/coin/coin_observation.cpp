#include "intensity/coin/coin_observation.hpp"
#include "intensity/coin/coin_acquisition.hpp"

#include "lio/params.h"
#include <ros/ros.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <stdexcept>

namespace cube::coin {
namespace {
using Mat3=Eigen::Matrix3d;
using Vec3d=Eigen::Vector3d;
constexpr double kOfficialMedianWeakContributionPerRow=0.01609598;

Eigen::Matrix4d transform(const Mat3& R,const Vec3d& t){
  Eigen::Matrix4d T=Eigen::Matrix4d::Identity();
  T.topLeftCorner<3,3>()=R;T.topRightCorner<3,1>()=t;return T;
}

}

CoinObservation::CoinObservation(ros::NodeHandle& nh,const Eigen::Matrix4d& T_IL)
  :T_IL_(T_IL){
  nh.param("/coin/enable",enabled_,false);
  if(!enabled_)return;
  nh.param("/coin/shadow",shadow_,false);
  nh.param<std::string>("/coin/selector_mode",selector_mode_,"original");
  if(selector_mode_!="original"&&selector_mode_!="gradient"&&
     selector_mode_!="weakest"&&selector_mode_!="normalized")
    throw std::invalid_argument("unknown COIN selector mode: "+selector_mode_);
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
    diagnostics_<<"frame,timestamp,raw_points,motion_fallback_points,coin_minus_super_scan_end_s,active_before,valid_patches,photo_rows,residual_rms,photo_A_norm,photo_b_norm,weak_dirs,active_after,added,removed,status,skip_reason,ncc_count,ncc_median,rejected_ncc,selector_mode,selected_centers_xy,selected_gradient_mean,selected_score_e1,selected_score_e2,selected_score_e3\n";
  }
  std::string fusion_audit_path;
  nh.getParam("/p2r/fusion_audit_path",fusion_audit_path);
  if(!fusion_audit_path.empty()){
    fusion_audit_.open(fusion_audit_path);
    if(!fusion_audit_)throw std::runtime_error("cannot write P2R fusion audit");
    fusion_audit_<<std::setprecision(17);
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
  auto skip_frame=[&](const std::string& reason,int unsupported_points){
    const int active=static_cast<int>(feature_manager_->features().size());
    if(diagnostics_){
      diagnostics_<<scan_index_<<','<<lidar.end_time<<','<<lidar.coin_raw_points.size()<<','
        <<unsupported_points<<','<<coin_scan_end_delta_s_<<','<<active
        <<",0,0,0,0,0,0,"<<active<<",0,0,SKIPPED,"<<reason<<",0,0,0,"
        <<selector_mode_<<",,,,,\n";
    }
    ROS_WARN_THROTTLE(5.0,"COIN observation skipped at scan %zu: %s; geometric update continues",
                      scan_index_,reason.c_str());
    ++scan_index_;
  };
  const auto input_issue=coinFrameInputIssue(lidar,history);
  if(!input_issue.empty()){
    skip_frame(input_issue,static_cast<int>(lidar.coin_raw_points.size()));
    return;
  }
  const Eigen::Matrix4d T_GI=poseMatrix(predicted_pose);
  const Mat3 R_GI=T_GI.topLeftCorner<3,3>();
  const Vec3d t_GI=T_GI.topRightCorner<3,1>();
  const Mat3 R_IL=T_IL_.topLeftCorner<3,3>();
  const Vec3d t_IL=T_IL_.topRightCorner<3,1>();
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
      T_Li_Lk=lidarAcquisitionToEnd(R_GL_acq,t_GL_acq,R_GL_end,t_GL_end);
    }else{
      ++motion_fallback_points_;
      continue;
    }
    if(!point.point_lidar.allFinite()||!T_Li_Lk.allFinite()){
      ++motion_fallback_points_;
      continue;
    }
    points_.push_back(point);
    transforms.push_back(T_Li_Lk);
    transform_indices.push_back(static_cast<int>(transform_indices.size()));
  }
  if(motion_fallback_points_){
    skip_frame("unsupported_or_nonfinite_acquisition_transform",motion_fallback_points_);
    return;
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
  const bool audit_this_frame=fusion_audit_&&!fusion_audited_&&!shadow_;
  std::vector<Eigen::Matrix<double,1,6>> audit_H;
  std::vector<double> audit_r;
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
      if(audit_this_frame){audit_H.push_back(H);audit_r.push_back(row.residual);}
      residual_square_sum_+=row.residual*row.residual;++photo_rows_;
    }
  }
  const double factor=photo_scale_*photo_scale_/measurement_variance_;
  photo_A*=factor;photo_b*=factor;
  photo_A_norm_=photo_A.norm();photo_b_norm_=photo_b.norm();
  BASIC::M6 A_before;
  BASIC::V6 b_before;
  if(audit_this_frame&&photo_rows_){A_before=A;b_before=b;}
  if(!shadow_){A+=photo_A.cast<BASIC::scalar>();b+=photo_b.cast<BASIC::scalar>();}
  if(audit_this_frame&&photo_rows_){
    Eigen::MatrixXd H(photo_rows_,6);
    Eigen::VectorXd residual(photo_rows_);
    for(int i=0;i<photo_rows_;++i){H.row(i)=audit_H[i];residual(i)=audit_r[i];}
    const Eigen::MatrixXd H_scaled=photo_scale_*H;
    const Eigen::VectorXd r_scaled=photo_scale_*residual;
    const Eigen::Matrix<double,6,6> A_explicit=
        (H_scaled.transpose()*H_scaled)/measurement_variance_;
    const Eigen::Matrix<double,6,1> b_explicit=
        (H_scaled.transpose()*r_scaled)/measurement_variance_;
    const double A_max=(A_explicit-photo_A).cwiseAbs().maxCoeff();
    const double b_max=(b_explicit-photo_b).cwiseAbs().maxCoeff();
    const double A_rel=(A_explicit-photo_A).norm()/std::max(1.,A_explicit.norm());
    const double b_rel=(b_explicit-photo_b).norm()/std::max(1.,b_explicit.norm());
    const auto A_cast=A_explicit.cast<BASIC::scalar>();
    const auto b_cast=b_explicit.cast<BASIC::scalar>();
    const double cast_A_rel=(A_cast.cast<double>()-photo_A.cast<BASIC::scalar>().cast<double>()).norm()/
        std::max(1.,A_cast.cast<double>().norm());
    const double cast_b_rel=(b_cast.cast<double>()-photo_b.cast<BASIC::scalar>().cast<double>()).norm()/
        std::max(1.,b_cast.cast<double>().norm());
    const double actual_A_rel=((A-A_before).cast<double>()-A_cast.cast<double>()).norm()/
        std::max(1.,A_cast.cast<double>().norm());
    const double actual_b_rel=((b-b_before).cast<double>()-b_cast.cast<double>()).norm()/
        std::max(1.,b_cast.cast<double>().norm());
    const double actual_A_abs=((A-A_before).cast<double>()-A_cast.cast<double>()).cwiseAbs().maxCoeff();
    const double actual_b_abs=((b-b_before).cast<double>()-b_cast.cast<double>()).cwiseAbs().maxCoeff();
    const double eps=std::numeric_limits<BASIC::scalar>::epsilon();
    const double A_rounding_bound=4*eps*(A_before.cast<double>().cwiseAbs()+
        A_cast.cast<double>().cwiseAbs()).maxCoeff();
    const double b_rounding_bound=4*eps*(b_before.cast<double>().cwiseAbs()+
        b_cast.cast<double>().cwiseAbs()).maxCoeff();
    const bool pass=A_rel<1e-10&&b_rel<1e-10&&cast_A_rel<1e-6&&cast_b_rel<1e-6&&
        actual_A_abs<=A_rounding_bound&&actual_b_abs<=b_rounding_bound;
    fusion_audit_<<"{\"status\":\""<<(pass?"PASS":"FAIL")<<"\","
      <<"\"frame\":"<<scan_index_<<",\"pose_state_fixed_within_iteration\":true,"
      <<"\"photo_rows\":"<<photo_rows_<<",\"valid_patches\":"<<valid_patches_<<','
      <<"\"photo_scale\":"<<photo_scale_<<",\"measurement_variance\":"<<measurement_variance_<<','
      <<"\"information_factor\":"<<factor<<",\"H_column_order\":\"right_local_rotation_then_global_position\","
      <<"\"b_sign\":\"positive_H_transpose_times_raw_intensity_residual\","
      <<"\"A_max_abs_difference\":"<<A_max<<",\"b_max_abs_difference\":"<<b_max<<','
      <<"\"A_relative_norm_difference\":"<<A_rel<<",\"b_relative_norm_difference\":"<<b_rel<<','
      <<"\"scalar_cast_A_relative_difference\":"<<cast_A_rel<<','
      <<"\"scalar_cast_b_relative_difference\":"<<cast_b_rel<<','
      <<"\"actual_total_accumulator_A_relative_rounding\":"<<actual_A_rel<<','
      <<"\"actual_total_accumulator_b_relative_rounding\":"<<actual_b_rel<<','
      <<"\"actual_total_accumulator_A_max_abs_rounding\":"<<actual_A_abs<<','
      <<"\"actual_total_accumulator_b_max_abs_rounding\":"<<actual_b_abs<<','
      <<"\"float_rounding_bound_A\":"<<A_rounding_bound<<','
      <<"\"float_rounding_bound_b\":"<<b_rounding_bound<<"}\n";
    fusion_audit_.flush();
    fusion_audited_=true;
  }
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
  std::vector<Vec3> selector_directions=weak.lidar;
  bool pure_gradient=false;
  if(selector_mode_=="gradient"){
    selector_directions.clear();pure_gradient=true;
  }else if(selector_mode_=="weakest"){
    selector_directions.clear();
    if(geometry_translation_rows.rows()>3)
      selector_directions.push_back(R_GL.transpose()*weak.eigenvectors.col(0));
    else selector_directions=weak.lidar;
  }else if(selector_mode_=="normalized"){
    const double threshold=kOfficialMedianWeakContributionPerRow*
        static_cast<double>(geometry_translation_rows.rows());
    selector_directions=CoinFeatureManager::weakDirectionsFromGeometry(
        geometry_translation_rows,R_GL,threshold).lidar;
  }
  std::vector<Vec3> audit_eigenvectors_lidar;
  for(int d=0;d<3;++d)audit_eigenvectors_lidar.push_back(R_GL.transpose()*weak.eigenvectors.col(d));
  feature_manager_->update(frame_,points_,selector_directions,T_GL,pure_gradient,audit_eigenvectors_lidar);
  const auto& stats=feature_manager_->lastStats();
  if(diagnostics_){
    const double rms=photo_rows_?std::sqrt(residual_square_sum_/photo_rows_):0.;
    std::vector<double> finite_ncc;
    for(double ncc:stats.ncc_values)if(std::isfinite(ncc))finite_ncc.push_back(ncc);
    std::sort(finite_ncc.begin(),finite_ncc.end());
    const double ncc_median=finite_ncc.empty()?0.:
        finite_ncc[finite_ncc.size()/2];
    diagnostics_<<scan_index_<<','<<timestamp<<','<<points_.size()<<','<<motion_fallback_points_<<','
      <<coin_scan_end_delta_s_<<','<<active_before_<<','
      <<valid_patches_<<','<<photo_rows_<<','<<rms<<','<<photo_A_norm_<<','<<photo_b_norm_<<','<<selector_directions.size()<<','
      <<feature_manager_->features().size()<<','<<stats.added<<','<<stats.removed
      <<",USED,,"<<finite_ncc.size()<<','<<ncc_median<<','<<stats.rejected_ncc<<','
      <<selector_mode_<<',';
    for(std::size_t i=0;i<stats.selected_center_pixels.size();++i){
      if(i)diagnostics_<<';';
      diagnostics_<<stats.selected_center_pixels[i].x()<<':'<<stats.selected_center_pixels[i].y();
    }
    auto average=[](double sum,int count){return count?sum/count:0.;};
    diagnostics_<<','<<average(stats.selected_gradient_sum,stats.selected_metric_count);
    for(const double score:stats.selected_directional_score_sum)
      diagnostics_<<','<<average(score,stats.selected_metric_count);
    diagnostics_<<'\n';
  }
  ++scan_index_;prepared_=false;
}

} // namespace cube::coin
