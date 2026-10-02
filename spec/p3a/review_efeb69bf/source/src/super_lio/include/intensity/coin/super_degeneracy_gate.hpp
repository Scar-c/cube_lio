#pragma once

#include "intensity/coin/coin_feature_manager.hpp"
#include <algorithm>
#include <cmath>

namespace cube::coin {

struct SuperDegeneracySignal {
  bool valid=false;
  double lambda1_over_lambda2=0.;
  double lambda1_over_lambda3=0.;
  double weakest_axis_stability=0.;
  double anisotropy_confidence=0.;
  double eigengap_confidence=0.;
  double confidence=0.;
};

class SuperDegeneracyGate {
 public:
  // Confidence combines a weak axis separated from the next axis, overall
  // translation anisotropy, and sign-invariant temporal axis stability.
  static SuperDegeneracySignal measure(const CoinWeakDirections& geometry,
      const Eigen::Vector3d& previous_weak_axis_global,bool has_previous_axis){
    SuperDegeneracySignal signal;
    const Eigen::Vector3d eigenvalues=geometry.eigenvalues;
    if(geometry.geometry_rows<=3||!eigenvalues.allFinite()||eigenvalues[2]<=1e-12||
       !geometry.eigenvectors.allFinite())
      return signal;
    signal.valid=true;
    const double lambda1=std::max(0.,eigenvalues[0]);
    const double lambda2=std::max(0.,eigenvalues[1]);
    const double lambda3=std::max(0.,eigenvalues[2]);
    signal.lambda1_over_lambda2=lambda2>1e-12?lambda1/lambda2:1.;
    signal.lambda1_over_lambda3=lambda1/lambda3;
    signal.anisotropy_confidence=std::clamp(1.-signal.lambda1_over_lambda3,0.,1.);
    signal.eigengap_confidence=std::clamp((lambda2-lambda1)/lambda3,0.,1.);
    if(has_previous_axis&&previous_weak_axis_global.allFinite()&&
       previous_weak_axis_global.norm()>1e-12){
      signal.weakest_axis_stability=std::clamp(
          std::abs(geometry.eigenvectors.col(0).normalized().dot(previous_weak_axis_global.normalized())),
          0.,1.);
    }
    signal.confidence=signal.anisotropy_confidence*signal.eigengap_confidence*
                      signal.weakest_axis_stability;
    return signal;
  }

  static bool activate(const SuperDegeneracySignal& signal,double threshold){
    return signal.valid&&std::isfinite(threshold)&&threshold>=0.&&signal.confidence>=threshold;
  }
};

} // namespace cube::coin
