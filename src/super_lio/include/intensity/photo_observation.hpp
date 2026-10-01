// Independent GPLv3 implementation. COIN-LIO lifecycle design inspiration:
// Patrick Pfreundschuh, COIN-LIO main 76729cc4 (BSD-3-Clause). No source copied.
#pragma once
#include "cube_image.hpp"
#include "common/ds.h"
#include <fstream>
#include <chrono>
#include <memory>
#include <ros/ros.h>

namespace cube {
struct Feature {
  Vec3 world;
  double reference=0,birth_time=0;
  size_t birth_frame=0;
  int birth_face=-1;
};
struct PhotoTerms {
  Mat6 A=Mat6::Zero();
  Vec6 b=Vec6::Zero();
  int valid=0,fov=0,invalid=0,range=0,outlier=0;
  double mean=0,rms=0,median=0;
};
class PhotoObservation {
 public:
  explicit PhotoObservation(ros::NodeHandle& nh);
  bool enabled() const{return cfg_.enable;}
  void prepare(const LI2Sup::MeasureGroup& measures,
               const std::vector<LI2Sup::DynamicState>& history,const BASIC::SE3& predicted);
  void add(const BASIC::SE3& pose,BASIC::M6& A,BASIC::V6& b);
  void finish(const BASIC::SE3& pose,double timestamp);
 private:
  Settings cfg_;
  std::unique_ptr<CubeImage> image_;
  std::vector<ScanPoint> points_;
  std::vector<Feature> features_;
  std::vector<double> early_residuals_;
  Mat3 R_BL_;
  Vec3 t_BL_;
  size_t frame_=0;
  bool frozen_=false;
  double sigma_=1,deskew_ms_=0,photo_ms_=0,update_ms_=0,replenish_ms_=0;
  std::chrono::steady_clock::time_point update_start_;
  std::ofstream diagnostics_;
  PhotoTerms last_;
  Mat6 geometry_=Mat6::Zero();
  PhotoTerms observe(const BASIC::SE3& pose,bool weighted,std::vector<double>* residuals=nullptr)const;
  void replenish(const BASIC::SE3& pose,double timestamp);
};
} // namespace cube
