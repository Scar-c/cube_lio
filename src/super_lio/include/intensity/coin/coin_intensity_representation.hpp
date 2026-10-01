#pragma once

#include "intensity/intensity_representation.hpp"
#include "intensity/coin/coin_photometric_model.hpp"
#include <cmath>

namespace cube::coin {

// An opt-in adapter for documenting the COIN mapping to the future interface.
// The current estimator continues to call its existing COIN projector and
// photometric model directly; this adapter does not alter production behavior.
class CoinIntensityRepresentation final : public cube::IntensityRepresentation {
 public:
  CoinIntensityRepresentation(const CoinOusterProjector& projector,const CoinFrame& frame)
      :projector_(projector),frame_(frame){}

  bool project(const Eigen::Vector3d& point,Eigen::Vector2d& pixel) const override{
    const ProjectedPoint projected=projector_.project(point);
    if(!projected.in_fov)return false;
    pixel=projected.uv;
    return true;
  }

  bool sample_intensity(const Eigen::Vector2d& pixel,double& intensity) const override{
    if(frame_.intensity.empty()||!pixel.allFinite()||pixel.x()<0.||pixel.y()<0.||
       pixel.x()>=frame_.intensity.cols-1||pixel.y()>=frame_.intensity.rows-1)return false;
    intensity=CoinPhotometricModel::sampleFloat(frame_.intensity,pixel.x(),pixel.y());
    return std::isfinite(intensity);
  }

  bool sample_gradient(const Eigen::Vector2d& pixel,Eigen::Vector2d& gradient) const override{
    if(frame_.intensity.empty()||!pixel.allFinite()||pixel.x()<1.||pixel.y()<1.||
       pixel.x()+1.>=frame_.intensity.cols-1||pixel.y()+1.>=frame_.intensity.rows-1)return false;
    gradient=CoinPhotometricModel::centralImageGradient(frame_.intensity,pixel.x(),pixel.y());
    return gradient.allFinite();
  }

  bool compute_residual(const Eigen::Vector2d& pixel,double reference_intensity,
                        double& residual) const override{
    double current=0.;
    if(!std::isfinite(reference_intensity)||!sample_intensity(pixel,current))return false;
    residual=current-reference_intensity;
    return std::isfinite(residual);
  }

 private:
  const CoinOusterProjector& projector_;
  const CoinFrame& frame_;
};

} // namespace cube::coin
