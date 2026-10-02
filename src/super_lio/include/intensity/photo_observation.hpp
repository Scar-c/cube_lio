// Independent GPLv3 implementation. COIN-LIO lifecycle design inspiration:
// Patrick Pfreundschuh, COIN-LIO main 76729cc4 (BSD-3-Clause). No source copied.
#pragma once
#include "cube_image.hpp"
#include "information_budget.hpp"
#include "common/ds.h"
#include <fstream>
#include <chrono>
#include <memory>
#include <string>
#include <ros/ros.h>

namespace cube {
struct Feature {
  Vec3 world;
  double reference=0,birth_time=0;
  size_t birth_frame=0;
  int birth_face=-1;
};
struct PhotoTerms {
  Mat6 A=Mat6::Zero();
  Vec6 b=Vec6::Zero();
  int valid=0,fov=0,invalid=0,range=0,outlier=0;
  double mean=0,rms=0,median=0;
};
class PhotoObservation {
 public:
  explicit PhotoObservation(ros::NodeHandle& nh);
  bool enabled() const{return cfg_.enable;}
  bool needsGeometryRows() const{return cfg_.enable&&selector_mode_=="weakest";}
  void prepare(const LI2Sup::MeasureGroup& measures,
               const std::vector<LI2Sup::DynamicState>& history,const BASIC::SE3& predicted);
  void add(const BASIC::SE3& pose,BASIC::M6& A,BASIC::V6& b,const BASIC::M6& prior_covariance);
  void finish(const BASIC::SE3& pose,double timestamp,
              const Eigen::MatrixXd& geometry_translation_rows);
 private:
  Settings cfg_;
  std::string projection_name_="cubemap",measurement_name_="igm",selector_mode_="all";
  double weakest_gate_threshold_=0.31913064578672057,weakest_gate_confidence_=0.;
  bool weakest_gate_active_=false,has_previous_weak_axis_=false;
  Eigen::Vector3d previous_weak_axis_global_=Eigen::Vector3d::UnitX();
  double previous_weak_axis_timestamp_=0.;
  InformationPolicy policy_=InformationPolicy::C0;
  bool audit_enabled_=false;
  std::ofstream audit_;
  // P3-R attribution controls. Both are off in the production/default path.
  bool time_audit_enabled_=false,history_supported_only_=false;
  std::ofstream time_audit_;
  int audit_iteration_=0;
  BASIC::SE3 predicted_pose_;
  std::vector<ResidualContribution> auditRows(const BASIC::SE3& pose)const;
  void writeAudit(const BASIC::SE3& pose,const BASIC::M6& covariance,
                  const Vec6& geometry_b,const std::vector<ResidualContribution>& rows);
  std::unique_ptr<RasterIntensityRepresentation> image_;
  std::vector<ScanPoint> points_;
  std::vector<Feature> features_;
  std::vector<double> early_residuals_;
  Mat3 R_BL_;
  Vec3 t_BL_;
  size_t frame_=0;
  bool frozen_=false,frame_supported_=false;
  double sigma_=1,deskew_ms_=0,photo_ms_=0,update_ms_=0,replenish_ms_=0;
  std::chrono::steady_clock::time_point update_start_;
  std::ofstream diagnostics_;
  PhotoTerms last_;
  Mat6 geometry_=Mat6::Zero();
  PhotoTerms observe(const BASIC::SE3& pose,bool weighted,std::vector<double>* residuals=nullptr)const;
  void replenish(const BASIC::SE3& pose,double timestamp,
                 const Eigen::MatrixXd& geometry_translation_rows);
};
} // namespace cube
