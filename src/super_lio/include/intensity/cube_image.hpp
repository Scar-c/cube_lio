// CUBE-LIO independent implementation, GPL-3.0-or-later.
#pragma once
#include "cube_projector.hpp"
#include "intensity/intensity_representation.hpp"
#include <opencv2/core.hpp>
#include <vector>

namespace cube {
using ScanPoint=RepresentationPoint;
struct Settings {
  bool enable=false,idw_enable=true;
  bool build_igm=true;
  int resolution=96,idw_radius=3,idw_k=6,idw_min_support=3;
  double idw_power=2.,range_absolute=.3,range_relative=.02,gaussian_sigma=1.;
  int max_features=1200,max_lifetime=10,suppression_radius=2,normalization_frames=20;
  double high_response=5.,weight=1.,sigma_min=1.,robust_gate=4.685,huber_delta=1.345;
  void validate() const;
};
struct Face {
  cv::Mat intensity,depth,igm,grad_u,grad_v;
  cv::Mat raw_mask,mask,igm_mask;
  std::vector<int> point_index;
};
class CubeImage final : public RasterIntensityRepresentation {
 public:
  explicit CubeImage(const Settings& cfg,
                     MeasurementChannel channel=MeasurementChannel::IntensityGradientMagnitude);
  void build(const std::vector<ScanPoint>& points) override;
  bool project(const Vec3& point,Projection& projection) const override;
  bool sampleIntensity(const Projection& projection,double& intensity) const override;
  bool sampleGradient(const Projection& projection,Eigen::Vector2d& gradient) const override;
  bool computeResidual(const Projection& projection,double reference,double& residual) const override;
  bool validityCheck(const Projection& projection,double expected_depth,double range_absolute,
                     double range_relative,Sample& sample) const override;
  bool sample(const Projection& q,Sample& out) const;
  const Face& face(int i) const {return faces_[i];}
  int chartCount() const override {return 6;}
  int coordinateResolution() const override {return cfg_.resolution;}
  int chartWidth(int chart) const override {return chart>=0&&chart<6?cfg_.resolution:0;}
  int chartHeight(int chart) const override {return chart>=0&&chart<6?cfg_.resolution:0;}
  bool wrapsHorizontally(int) const override{return false;}
  const std::vector<int>& candidatePointIndices() const override{return candidate_points_;}
  double rasterMilliseconds() const override{return raster_ms;}
  double interpolationMilliseconds() const override{return idw_ms;}
  double featureChannelMilliseconds() const override{return igm_ms;}
  int rawPixelCount() const override{return raw_pixels;}
  int filledPixelCount() const override{return filled_pixels;}
  int validFeaturePixelCount() const override{return valid_igm_pixels;}
  CubeProjector projector;
  double raster_ms=0,idw_ms=0,igm_ms=0;
  int raw_pixels=0,filled_pixels=0,valid_igm_pixels=0;
 private:
  Settings cfg_;
  MeasurementChannel channel_;
  std::array<Face,6> faces_;
  std::vector<int> candidate_points_;
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
