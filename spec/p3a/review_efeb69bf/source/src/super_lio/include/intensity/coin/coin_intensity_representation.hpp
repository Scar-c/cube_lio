#pragma once

#include "intensity/intensity_representation.hpp"
#include "intensity/coin/coin_photometric_model.hpp"
#include <cmath>

namespace cube::coin {

// Adapter for the frozen COIN raw-intensity image and scalar residual. It is
// available for controlled comparisons; the production COIN call path remains
// unchanged and continues to use its calibrated acquisition-time matches.
class CoinIntensityRepresentation final : public cube::IntensityRepresentation {
 public:
  CoinIntensityRepresentation(const CoinOusterProjector& projector,const CoinFrame& frame)
      :projector_(projector),frame_(frame){}

  bool project(const Vec3& point,cube::Projection& projection) const override{
    const auto projected=projector_.project(point);
    if(!projected.in_fov)return false;
    projection=cube::Projection{};projection.face=0;projection.uv=projected.uv;
    projection.jacobian=projector_.projectionJacobian(point);
    return projection.uv.allFinite()&&projection.jacobian.allFinite();
  }

  bool sampleIntensity(const cube::Projection& projection,double& intensity) const override{
    if(!coordinatesValid(projection,false))return false;
    intensity=CoinPhotometricModel::sampleFloat(frame_.intensity,projection.uv.x(),projection.uv.y());
    return std::isfinite(intensity);
  }

  bool sampleGradient(const cube::Projection& projection,Eigen::Vector2d& gradient) const override{
    if(!coordinatesValid(projection,true))return false;
    gradient=CoinPhotometricModel::centralImageGradient(frame_.intensity,
                                                        projection.uv.x(),projection.uv.y());
    return gradient.allFinite();
  }

  bool computeResidual(const cube::Projection& projection,double reference_intensity,
                       double& residual) const override{
    double current=0.;
    if(!std::isfinite(reference_intensity)||!sampleIntensity(projection,current))return false;
    residual=current-reference_intensity;
    return std::isfinite(residual);
  }

  bool validityCheck(const cube::Projection& projection,double expected_depth,
                     double range_absolute,double range_relative,
                     cube::Sample& sample) const override{
    if(!coordinatesValid(projection,false)||frame_.range.empty()||!std::isfinite(expected_depth))return false;
    const int u=static_cast<int>(std::floor(projection.uv.x()));
    const int v=static_cast<int>(std::floor(projection.uv.y()));
    sample.depth=frame_.range.ptr<float>(v)[u];
    if(!(sample.depth>0.)||std::abs(expected_depth-sample.depth)>
       range_absolute+range_relative*sample.depth)return false;
    Eigen::Vector2d gradient;
    if(!sampleIntensity(projection,sample.value)||!sampleGradient(projection,gradient))return false;
    sample.gradient=gradient.transpose();
    return true;
  }

 private:
  bool coordinatesValid(const cube::Projection& projection,bool need_gradient) const{
    if(frame_.intensity.empty()||frame_.mask.empty()||!projection.uv.allFinite()||
       projection.face!=0||projection.uv.x()<0.||projection.uv.y()<0.||
       projection.uv.x()>=frame_.intensity.cols-1||projection.uv.y()>=frame_.intensity.rows-1)
      return false;
    if(need_gradient&&(projection.uv.x()<1.||projection.uv.y()<1.||
       projection.uv.x()+1.>=frame_.intensity.cols-1||
       projection.uv.y()+1.>=frame_.intensity.rows-1))return false;
    const int u=static_cast<int>(std::floor(projection.uv.x()));
    const int v=static_cast<int>(std::floor(projection.uv.y()));
    return frame_.mask.ptr<uchar>(v)[u]!=0;
  }

  const CoinOusterProjector& projector_;
  const CoinFrame& frame_;
};

} // namespace cube::coin
