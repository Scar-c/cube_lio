// COIN-LIO calibrated Ouster intensity image pipeline reimplementation.
#pragma once

#include "intensity/coin/coin_ouster_projector.hpp"
#include <opencv2/core.hpp>
#include <cstdint>
#include <vector>

namespace cube::coin {

struct CoinImageSettings {
  bool reflectivity=false,line_removal=true,brightness_filter=true,blur=true;
  double intensity_scale=.25,min_range=.7,max_range=30.;
  int patch_size=5,erosion_margin=2;
  cv::Size brightness_window{41,7};
  std::vector<cv::Rect> masks;
  std::vector<double> highpass,lowpass;
  static CoinImageSettings fromRosParams();
};

struct CoinScanPoint {
  Vec3 point_lidar=Vec3::Zero();
  float intensity=0.f;
  float range=0.f;
  std::size_t raw_index=0;
  double offset_seconds=0;
};

struct CoinFrame {
  cv::Mat raw_intensity,intensity,range;
  cv::Mat image_index,mask,photo_u8,dx,dy;
  // For each rounded geometric projection cell: count followed by <=9 point indices.
  std::vector<int> projected_index;
};

class CoinImageProcessor {
 public:
  CoinImageProcessor(CoinOusterProjector projector,CoinImageSettings settings);
  CoinFrame process(std::vector<CoinScanPoint>& points) const;
  const CoinOusterProjector& projector() const{return projector_;}
 private:
  CoinOusterProjector projector_;
  CoinImageSettings settings_;
};

} // namespace cube::coin
