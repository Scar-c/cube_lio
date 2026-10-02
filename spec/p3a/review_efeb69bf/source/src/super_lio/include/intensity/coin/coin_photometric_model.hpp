// COIN-LIO fixed-correspondence photometric observation model.
#pragma once

#include "intensity/coin/coin_feature_manager.hpp"

namespace cube::coin {

struct CoinPhotoLinearization {
  bool valid=false;
  double residual=0.;
  Vec2 uv=Vec2::Zero();
  Vec3 point_lidar_end=Vec3::Zero();
  Vec3 point_lidar_acquisition=Vec3::Zero();
  int distortion_index=-1;
  Eigen::Matrix<double,1,6> correction_jacobian_coin=Eigen::Matrix<double,1,6>::Zero();
  Eigen::Matrix<double,1,6> correction_jacobian_super=Eigen::Matrix<double,1,6>::Zero();
};

class CoinPhotometricModel {
 public:
  static CoinPhotoLinearization linearize(const CoinFeatureManager& manager,
      const CoinOusterProjector& projector,const CoinFrame& frame,
      const std::vector<CoinScanPoint>& points,const Eigen::Matrix4d& T_GI,
      const Eigen::Matrix4d& T_IL,const Vec3& point_global,double reference_intensity,
      double min_range=.7,double max_range=30.,int margin=10);

  // Evaluate the same residual while holding the selected acquisition-time
  // point correspondence fixed. This is the finite-difference diagnostic path.
  static double evaluateFixed(const CoinOusterProjector& projector,const CoinFrame& frame,
      const std::vector<CoinScanPoint>& points,const Eigen::Matrix4d& T_GI,
      const Eigen::Matrix4d& T_IL,const Vec3& point_global,double reference_intensity,
      int distortion_index);

  static Eigen::Matrix4d applyCoinCorrection(const Eigen::Matrix4d& T_GI,
      const Eigen::Matrix<double,6,1>& delta_position_then_rotation);

  static double sampleFloat(const cv::Mat& image,double x,double y);
  static Eigen::Vector2d centralImageGradient(const cv::Mat& image,double x,double y);
};

} // namespace cube::coin
