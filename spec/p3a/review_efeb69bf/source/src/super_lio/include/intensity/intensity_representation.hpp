#pragma once

#include <Eigen/Core>
#include <vector>

namespace cube {

// A coordinate in a projection chart. face is a cubemap face for cubemaps and
// zero for single-chart projections. Jacobians map sensor-frame XYZ to pixels.
struct Projection {
  int face=-1;
  Eigen::Vector2d uv=Eigen::Vector2d::Zero();
  Eigen::Matrix<double,2,3> jacobian=Eigen::Matrix<double,2,3>::Zero();
  bool seam=false;
};

struct Sample {
  double value=0.,depth=0.;
  Eigen::RowVector2d gradient=Eigen::RowVector2d::Zero();
};

enum class MeasurementChannel { RawIntensity, IntensityGradientMagnitude };

struct RepresentationPoint {
  Eigen::Vector3d p=Eigen::Vector3d::Zero();
  double intensity=0.;
};

// Projection-specific intensity access. Residuals retain the same scalar
// current-minus-reference convention; implementations choose the sampled
// channel when constructed. COIN implements its existing raw-intensity model.
class IntensityRepresentation {
 public:
  virtual ~IntensityRepresentation()=default;
  virtual bool project(const Eigen::Vector3d& point,Projection& projection) const=0;
  virtual bool sampleIntensity(const Projection& projection,double& intensity) const=0;
  virtual bool sampleGradient(const Projection& projection,Eigen::Vector2d& gradient) const=0;
  virtual bool computeResidual(const Projection& projection,double reference_intensity,
                               double& residual) const=0;
  virtual bool validityCheck(const Projection& projection,double expected_depth,
                             double range_absolute,double range_relative,
                             Sample& sample) const=0;
};

// Per-scan raster implementations add construction and deterministic feature
// candidate enumeration without putting image lifecycle into the estimator.
class RasterIntensityRepresentation : public IntensityRepresentation {
 public:
  virtual void build(const std::vector<RepresentationPoint>& points)=0;
  virtual int chartCount() const=0;
  virtual int coordinateResolution() const=0;
  virtual int chartWidth(int chart) const=0;
  virtual int chartHeight(int chart) const=0;
  virtual bool wrapsHorizontally(int chart) const=0;
  virtual const std::vector<int>& candidatePointIndices() const=0;
  virtual double rasterMilliseconds() const=0;
  virtual double interpolationMilliseconds() const=0;
  virtual double featureChannelMilliseconds() const=0;
  virtual int rawPixelCount() const=0;
  virtual int filledPixelCount() const=0;
  virtual int validFeaturePixelCount() const=0;
};

} // namespace cube
