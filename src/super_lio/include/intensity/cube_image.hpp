// CUBE-LIO independent implementation, GPL-3.0-or-later.
#pragma once
#include "cube_projector.hpp"
#include <opencv2/core.hpp>
#include <vector>

namespace cube {
struct Settings {
  bool enable=false,idw_enable=true;
  int resolution=96,idw_radius=3,idw_k=6,idw_min_support=3;
  double idw_power=2.,range_absolute=.3,range_relative=.02,gaussian_sigma=1.;
  int max_features=1200,max_lifetime=10,suppression_radius=2,normalization_frames=20;
  double high_response=5.,weight=1.,sigma_min=1.,robust_gate=4.685,huber_delta=1.345;
  void validate() const;
};
struct ScanPoint {Vec3 p;double intensity;};
struct Sample {
  double value=0,depth=0;
  Eigen::RowVector2d gradient=Eigen::RowVector2d::Zero();
};
struct Face {
  cv::Mat intensity,depth,igm,grad_u,grad_v;
  cv::Mat raw_mask,mask,igm_mask;
  std::vector<int> point_index;
};
class CubeImage {
 public:
  explicit CubeImage(const Settings& cfg);
  void build(const std::vector<ScanPoint>& points);
  bool sample(const Projection& q,Sample& out) const;
  const Face& face(int i) const {return faces_[i];}
  CubeProjector projector;
  double raster_ms=0,idw_ms=0,igm_ms=0;
  int raw_pixels=0,filled_pixels=0,valid_igm_pixels=0;
 private:
  Settings cfg_;
  std::array<Face,6> faces_;
  void fill(Face& face);
  void gradients(Face& face);
};
// Right-local rotation, global translation: R_new=R Exp(w), t_new=t+dt.
// The state is T_GB and the LiDAR extrinsic is T_BL.
inline Vec3 landmarkInLidar(const Vec3& world,const Mat3& R,const Vec3& t,
                           const Mat3& R_BL,const Vec3& t_BL) {
  return R_BL.transpose()*(R.transpose()*(world-t)-t_BL);
}
inline Row6 residualJacobian(const Vec3& world,const Mat3& R,const Vec3& t,
                            const Mat3& R_BL,const Vec3& t_BL,
                            const Projection& q,const Sample& sample) {
  const Vec3 p_B=R.transpose()*(world-t);
  Eigen::Matrix<double,3,6> chain;
  chain.leftCols<3>()=R_BL.transpose()*hat(p_B);
  chain.rightCols<3>()=-R_BL.transpose()*R.transpose();
  return sample.gradient*q.jacobian*chain;
}
} // namespace cube
