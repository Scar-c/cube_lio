#pragma once

#include <Eigen/Core>

namespace cube {

// Projection-independent intensity sampling contract for future image/range
// representations. Points are expressed in the representation's local frame;
// pixels use (u, v) order. Existing COIN production code does not depend on it.
class IntensityRepresentation {
 public:
  virtual ~IntensityRepresentation()=default;
  virtual bool project(const Eigen::Vector3d& point,Eigen::Vector2d& pixel) const=0;
  virtual bool sample_intensity(const Eigen::Vector2d& pixel,double& intensity) const=0;
  virtual bool sample_gradient(const Eigen::Vector2d& pixel,Eigen::Vector2d& gradient) const=0;
  virtual bool compute_residual(const Eigen::Vector2d& pixel,double reference_intensity,
                                double& residual) const=0;
};

} // namespace cube
