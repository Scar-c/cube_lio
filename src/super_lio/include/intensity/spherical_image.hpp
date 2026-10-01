#pragma once

#include "intensity/cube_image.hpp"
#include <algorithm>

namespace cube {

// Generic azimuth/elevation raster. Width and height are chosen to approximate
// the angular pixel spacing of the reference cubemap face resolution.
class SphericalImage final : public RasterIntensityRepresentation {
 public:
  explicit SphericalImage(const Settings& settings,
                          MeasurementChannel channel=MeasurementChannel::IntensityGradientMagnitude);
  void build(const std::vector<ScanPoint>& points) override;
  bool project(const Vec3& point,Projection& projection) const override;
  bool sampleIntensity(const Projection& projection,double& intensity) const override;
  bool sampleGradient(const Projection& projection,Eigen::Vector2d& gradient) const override;
  bool computeResidual(const Projection& projection,double reference,double& residual) const override;
  bool validityCheck(const Projection& projection,double expected_depth,double range_absolute,
                     double range_relative,Sample& sample) const override;
  bool sample(const Projection& projection,Sample& sample) const;
  int chartCount() const override{return 1;}
  int coordinateResolution() const override{return std::max(width_,height_);}
  int chartWidth(int chart) const override{return chart==0?width_:0;}
  int chartHeight(int chart) const override{return chart==0?height_:0;}
  bool wrapsHorizontally(int chart) const override{return chart==0;}
  const std::vector<int>& candidatePointIndices() const override{return candidate_points_;}
  double rasterMilliseconds() const override{return raster_ms;}
  double interpolationMilliseconds() const override{return idw_ms;}
  double featureChannelMilliseconds() const override{return igm_ms;}
  int rawPixelCount() const override{return raw_pixels;}
  int filledPixelCount() const override{return filled_pixels;}
  int validFeaturePixelCount() const override{return valid_igm_pixels;}
  int width() const{return width_;}
  int height() const{return height_;}
  const Face& chart() const{return chart_;}

  double raster_ms=0.,idw_ms=0.,igm_ms=0.;
  int raw_pixels=0,filled_pixels=0,valid_igm_pixels=0;

 private:
  Settings cfg_;
  MeasurementChannel channel_;
  int width_=0,height_=0;
  Face chart_;
  std::vector<int> candidate_points_;
  void fill();
  void gradients();
};

} // namespace cube
