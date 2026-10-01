// COIN-LIO patch feature semantics reimplemented from the pinned BSD-3-Clause oracle.
#pragma once

#include "intensity/coin/coin_image_processor.hpp"
#include <Eigen/Eigenvalues>
#include <cstdint>
#include <vector>

namespace cube::coin {

struct CoinFeatureSettings {
  int patch_size=5;
  double max_range=30.;
  int max_lifetime=25;
  double min_range=.7;
  int suppression_radius=10;
  int num_features=60;
  double grad_min=16.5;
  double ncc_threshold=.7075;
  int margin=10;
  double range_threshold=.2;
  double n_uninformative=25.;
  static CoinFeatureSettings fromRosParams();
  void validate() const;
};

struct CoinFeature {
  std::uint64_t id=0;
  int lifetime=1;
  Vec2 center=Vec2::Zero();
  std::vector<double> reference_intensity;
  std::vector<Vec3> points_global;
  std::vector<Vec2> current_uv;
};

struct CoinFeatureFrameStats {
  int active_before=0,active_after=0,added=0,removed=0;
  int candidates_after_nms=0,selected_centers=0;
  int rejected_projection=0,rejected_border=0,rejected_mask=0,rejected_range=0;
  int rejected_ncc=0,rejected_lifetime=0;
  std::vector<double> ncc_values;
};

struct CoinWeakDirections {
  Eigen::Vector3d contribution=Eigen::Vector3d::Zero();
  Eigen::Matrix3d eigenvectors=Eigen::Matrix3d::Identity();
  std::vector<Vec3> global;
  std::vector<Vec3> lidar;
};

class CoinFeatureManager {
 public:
  CoinFeatureManager(CoinOusterProjector projector,CoinFeatureSettings settings={});
  void update(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
              const std::vector<Vec3>& weak_directions_lidar,const Eigen::Matrix4d& T_GL);
  bool projectUndistorted(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
                          const Vec3& p_Lk,Vec3& p_Li,Vec2& uv,int& distortion_index,
                          bool round_bucket=false) const;
  const std::vector<CoinFeature>& features() const{return features_;}
  const CoinFeatureFrameStats& lastStats() const{return last_stats_;}
  static double normalizedCrossCorrelation(const std::vector<double>& reference,
                                          const std::vector<double>& current);
  static CoinWeakDirections weakDirectionsFromGeometry(const Eigen::MatrixXd& H_translation,
                                                       const Eigen::Matrix3d& R_GL,
                                                       double n_uninformative=25.);

 private:
  void track(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,const Eigen::Matrix4d& T_GL);
  void updateSuppressionMask();
  void detect(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
              const std::vector<Vec3>& weak_directions_lidar,const Eigen::Matrix4d& T_GL);
  void detectComplementary(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
                           const std::vector<Vec3>& weak_directions_lidar,int needed,
                           std::vector<cv::Point>& centers);
  static double sampleBilinearFloat(const cv::Mat& image,double x,double y);

  CoinOusterProjector projector_;
  CoinFeatureSettings settings_;
  std::vector<CoinFeature> features_;
  cv::Mat suppression_mask_,margin_mask_;
  std::vector<Eigen::Vector2i> patch_offsets_;
  CoinFeatureFrameStats last_stats_;
  std::uint64_t next_id_=1;
};

} // namespace cube::coin
