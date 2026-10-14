// Independent implementation of Liu et al., ICRA 2026, Eq. (3)-(16).
// P4 mathematical gate module; not registered as an estimator observation.
#pragma once
#include "intensity/cube_projector.hpp"
#include <opencv2/core.hpp>
#include <array>
#include <vector>

namespace cube::paper {
struct Basis { Vec3 u,v,a; };
class Projector {
 public:
  explicit Projector(int resolution);
  static const std::array<Basis,6>& bases();
  int resolution() const { return n_; }
  Projection project(const Vec3& point) const;
  Vec3 inverse(int face,const Eigen::Vector2d& uv) const;
  Vec3 reconstruct(int face,const Eigen::Vector2d& uv,double range) const;
  Projection remap(int face,const Eigen::Vector2d& uv) const;
 private:
  int n_;
};

struct Config {
  int resolution=96,idw_radius=3,idw_k=6,idw_min_support=3;
  double idw_power=2.,range_absolute=.3,range_relative=.02,gaussian_sigma=1.;
  bool idw=true;
  void validate() const;
};
struct Point { Vec3 body; double intensity; };
struct Face {
  cv::Mat intensity,range,igm,gx,gy,raw_mask,mask,igm_mask;
  cv::Mat support_count,fill_distance;
};
struct Statistics {
  int raw_valid_pixels=0,idw_filled_pixels=0,valid_range_pixels=0,valid_igm_pixels=0;
  double image_generation_ms=0.;
  size_t seam_gradient_queries=0,seam_remapped_queries=0,seam_failed_queries=0;
};
enum class Channel { Intensity,Range,Igm };
class Frame {
 public:
  explicit Frame(Config config);
  void build(const std::vector<Point>& points);
  const Face& face(int index) const { return faces_.at(index); }
  bool sample(int face,const Eigen::Vector2d& uv,Channel channel,double& value) const;
  // Eq. (16), never the derivative of the bilinear interpolant.
  bool gradient(int face,const Eigen::Vector2d& uv,Channel channel,
                Eigen::RowVector2d& result) const;
  const Statistics& statistics() const { return stats_; }
  const Projector projector;
 private:
  Config cfg_;
  std::array<Face,6> faces_;
  mutable Statistics stats_;
  bool texel(int face,int u,int v,Channel channel,double& value) const;
  void fill(int face);
  void igm(int face);
};
// Super state ordering: [right-local rotation, global translation].
inline Eigen::Matrix<double,3,6> inverseTransformJacobian(const Vec3& world,
                                                       const Mat3& R,const Vec3& t) {
  Eigen::Matrix<double,3,6> J;
  J.leftCols<3>()=hat(R.transpose()*(world-t));
  J.rightCols<3>()=-R.transpose();
  return J;
}
struct Linearization {
  bool valid=false;
  double residual=0.;
  Row6 H=Row6::Zero();
};
Linearization linearize(const Frame& frame,const Vec3& world,double reference,
                        const Mat3& R,const Vec3& t,Channel channel=Channel::Igm);
} // namespace cube::paper
