// One-shot diagnostic COIN measurement channel for the Super-LIO pose update.
#pragma once

#include "common/ds.h"
#include "intensity/coin/coin_photometric_model.hpp"
#include <ros/ros.h>

#include <fstream>
#include <memory>

namespace cube::coin {

class CoinObservation {
 public:
  CoinObservation(ros::NodeHandle& nh,const Eigen::Matrix4d& T_IL);
  bool enabled() const { return enabled_; }
  bool shadow() const { return shadow_; }

  void prepare(const LI2Sup::LidarData& lidar,
               const std::vector<LI2Sup::DynamicState>& history,
               const BASIC::SE3& predicted_pose);
  void add(const BASIC::SE3& pose,BASIC::M6& A,BASIC::V6& b);
  void finish(const BASIC::SE3& pose,double timestamp,
              const Eigen::MatrixXd& geometry_translation_rows);

 private:
  Eigen::Matrix4d poseMatrix(const BASIC::SE3& pose) const;

  bool enabled_=false,prepared_=false,shadow_=false;
  std::string selector_mode_="original";
  double photo_scale_=0.00095,measurement_variance_=0.001;
  Eigen::Matrix4d T_IL_=Eigen::Matrix4d::Identity();
  CoinImageSettings image_settings_;
  CoinFeatureSettings feature_settings_;
  std::unique_ptr<CoinOusterProjector> projector_;
  std::unique_ptr<CoinImageProcessor> image_processor_;
  std::unique_ptr<CoinFeatureManager> feature_manager_;
  std::vector<CoinScanPoint> points_;
  CoinFrame frame_;
  std::ofstream diagnostics_;
  std::ofstream fusion_audit_;
  bool fusion_audited_=false;
  std::size_t scan_index_=0;
  int active_before_=0,valid_patches_=0,photo_rows_=0,motion_fallback_points_=0;
  double residual_square_sum_=0.,photo_A_norm_=0.,photo_b_norm_=0.;
  double coin_scan_end_delta_s_=0.;
};

} // namespace cube::coin
