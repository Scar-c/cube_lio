// Independent GPLv3 implementation; see photo_observation.hpp for provenance.
#include "intensity/photo_observation.hpp"
#include "lio/params.h"
#include <Eigen/Eigenvalues>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/enumerable_thread_specific.h>
#include <numeric>
#include <chrono>
#include <iomanip>

namespace cube {
namespace {
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
double median(std::vector<double> v){
  if(v.empty())return 0;auto k=v.begin()+v.size()/2;std::nth_element(v.begin(),k,v.end());return *k;
}
struct Acc {PhotoTerms terms;std::vector<double> residuals;};
}
PhotoObservation::PhotoObservation(ros::NodeHandle& nh){
#define LOAD(group,key,field) nh.param(group "/" key,cfg_.field,cfg_.field)
  LOAD("/photo","enable",enable);LOAD("/photo","weight",weight);
  LOAD("/photo","max_features",max_features);LOAD("/photo","max_lifetime",max_lifetime);
  LOAD("/photo","high_response",high_response);LOAD("/photo","suppression_radius",suppression_radius);
  LOAD("/photo","normalization_frames",normalization_frames);LOAD("/photo","sigma_min",sigma_min);
  LOAD("/photo","robust_gate",robust_gate);LOAD("/photo","huber_delta",huber_delta);
  LOAD("/cubemap","resolution",resolution);LOAD("/cubemap","idw_enable",idw_enable);
  LOAD("/cubemap","idw_radius",idw_radius);LOAD("/cubemap","idw_k",idw_k);
  LOAD("/cubemap","idw_power",idw_power);LOAD("/cubemap","idw_min_support",idw_min_support);
  LOAD("/cubemap","range_absolute",range_absolute);LOAD("/cubemap","range_relative",range_relative);
  LOAD("/cubemap","gaussian_sigma",gaussian_sigma);
#undef LOAD
  cfg_.validate();sigma_=cfg_.sigma_min;
  if(!cfg_.enable)return;
  cv::setNumThreads(1); // TBB owns frontend parallelism; avoid nested OpenCV pools.
  R_BL_=LI2Sup::g_lidar_imu.R_.cast<double>();t_BL_=LI2Sup::g_lidar_imu.t_.cast<double>();
  image_=std::make_unique<CubeImage>(cfg_);
  std::string dir;nh.getParam("/lio/offline/out_dir",dir);
  if(!dir.empty()){
    diagnostics_.open(dir+"/photo.csv");
    if(!diagnostics_)throw std::runtime_error("cannot write photometric diagnostics");
    diagnostics_<<std::setprecision(17);
    diagnostics_<<"frame,timestamp,active_before,active_after,valid,residual_mean,residual_rms,residual_median,reject_fov,reject_invalid,reject_range,reject_outlier,sigma,frozen,trace_Ag,trace_Ap,weak_translation_eigenvalue,photo_in_weak_translation,weak_x,weak_y,weak_z,geo_eig0,geo_eig1,geo_eig2,geo_eig3,geo_eig4,geo_eig5,deskew_ms,cubemap_ms,idw_ms,igm_ms,photo_jacobian_ms,update_ms,replenish_ms,raw_points,raw_pixels,filled_pixels,igm_pixels\n";
  }
}
void PhotoObservation::prepare(const LI2Sup::MeasureGroup& measures,
                              const std::vector<LI2Sup::DynamicState>& history,const BASIC::SE3& predicted){
  if(!cfg_.enable)return;
  ++frame_;photo_ms_=0;auto start=Clock::now();
  const auto& raw=measures.lidar.pc_intensity;
  if(!raw||history.size()<2)throw std::runtime_error("missing dense intensity scan/IMU history");
  points_.resize(raw->size());
  const Mat3 R_end=predicted.R_.cast<double>();const Vec3 t_end=predicted.t_.cast<double>();
  tbb::parallel_for(tbb::blocked_range<size_t>(0,raw->size()),[&](const tbb::blocked_range<size_t>& rows){
    for(size_t i=rows.begin();i<rows.end();++i){
      const auto& p=raw->points[i];Vec3 p_L(p.x,p.y,p.z);const Vec3 p_B=R_BL_*p_L+t_BL_;
      const double time=measures.lidar.start_time+p.offset_time;
      Vec3 corrected=p_B;
      if(time>=history.front().time&&time<=history.back().time){
        auto next=std::upper_bound(history.begin(),history.end(),time,[](double t,const LI2Sup::DynamicState& s){return t<s.time;});
        if(next==history.end())--next;
        const auto& tail=*next;const auto& head=*std::prev(next);
        double dt=tail.time-head.time,tau=time-head.time;
        if(dt>0){
          Eigen::Quaterniond r0(head.R.cast<double>()),r1(tail.R.cast<double>());
          const Mat3 R_i=r0.slerp(tau/dt,r1).toRotationMatrix();
          Vec3 t_i=head.p.cast<double>()+head.v.cast<double>()*tau+.5*tail.a.cast<double>()*tau*tau;
          corrected=R_end.transpose()*(R_i*p_B+t_i-t_end);
        }
      }
      // Return from upstream deskew's IMU-end frame to sensor-centric LiDAR-end.
      points_[i]={R_BL_.transpose()*(corrected-t_BL_),p.intensity};
    }
  });
  deskew_ms_=elapsed(start);image_->build(points_);
  if(!frozen_){
    std::vector<double> samples;observe(predicted,false,&samples);
    early_residuals_.insert(early_residuals_.end(),samples.begin(),samples.end());
    if(frame_>=size_t(cfg_.normalization_frames)&&early_residuals_.size()>=30){
      double center=median(early_residuals_);std::vector<double> deviations;
      for(double r:early_residuals_)deviations.push_back(std::abs(r-center));
      sigma_=std::max(cfg_.sigma_min,1.4826*median(deviations));frozen_=true;early_residuals_.clear();
    }
    // Bound startup samples if sparse scenes never reach the minimum count.
    if(early_residuals_.size()>size_t(cfg_.max_features*cfg_.normalization_frames))
      early_residuals_.erase(early_residuals_.begin(),early_residuals_.begin()+cfg_.max_features);
  }
  update_start_=Clock::now();
}
PhotoTerms PhotoObservation::observe(const BASIC::SE3& pose,bool weighted,std::vector<double>* residuals)const{
  const Mat3 R=pose.R_.cast<double>();const Vec3 t=pose.t_.cast<double>();
  tbb::enumerable_thread_specific<Acc> locals;
  tbb::parallel_for(tbb::blocked_range<size_t>(0,features_.size()),[&](const tbb::blocked_range<size_t>& rows){
    auto& a=locals.local();
    for(size_t i=rows.begin();i<rows.end();++i){
      const auto& feat=features_[i];
      const Vec3 p_L=landmarkInLidar(feat.world,R,t,R_BL_,t_BL_);auto q=image_->projector.project(p_L);
      if(q.face<0||q.seam||p_L.norm()<std::sqrt(LI2Sup::g_blind2)||p_L.norm()>std::sqrt(LI2Sup::g_maxrange2)){++a.terms.fov;continue;}
      Sample sample;
      if(!image_->sample(q,sample)){++a.terms.invalid;continue;}
      if(std::abs(p_L.norm()-sample.depth)>cfg_.range_absolute+cfg_.range_relative*sample.depth){++a.terms.range;continue;}
      const double residual=sample.value-feat.reference;
      if(weighted&&frozen_&&std::abs(residual)>cfg_.robust_gate*sigma_){++a.terms.outlier;continue;}
      ++a.terms.valid;a.residuals.push_back(residual);
      if(weighted&&frozen_){
        const Row6 J=residualJacobian(feat.world,R,t,R_BL_,t_BL_,q,sample);
        const double z=std::abs(residual)/sigma_;
        const double robust=z<=cfg_.huber_delta?1.:cfg_.huber_delta/z;
        const double w=cfg_.weight*robust/(sigma_*sigma_);
        a.terms.A.noalias()+=w*J.transpose()*J;
        a.terms.b.noalias()-=w*J.transpose()*residual; // Exact upstream -J^T W r sign.
      }
    }
  });
  PhotoTerms total;std::vector<double> values;
  for(const auto& a:locals){
    total.A+=a.terms.A;total.b+=a.terms.b;total.valid+=a.terms.valid;
    total.fov+=a.terms.fov;total.invalid+=a.terms.invalid;total.range+=a.terms.range;total.outlier+=a.terms.outlier;
    values.insert(values.end(),a.residuals.begin(),a.residuals.end());
  }
  if(!values.empty()){
    double square=0;for(double v:values){total.mean+=v;square+=v*v;}
    total.mean/=values.size();total.rms=std::sqrt(square/values.size());total.median=median(values);
  }
  if(residuals)*residuals=std::move(values);
  return total;
}
void PhotoObservation::add(const BASIC::SE3& pose,BASIC::M6& A,BASIC::V6& b){
  if(!cfg_.enable)return;auto start=Clock::now();
  geometry_=A.cast<double>();last_=observe(pose,true);
  A+=last_.A.cast<BASIC::scalar>();b+=last_.b.cast<BASIC::scalar>();
  photo_ms_+=elapsed(start);
}
void PhotoObservation::replenish(const BASIC::SE3& pose,double timestamp){
  const Mat3 R=pose.R_.cast<double>();const Vec3 t=pose.t_.cast<double>();
  std::vector<Feature> surviving;std::vector<Projection> centers;
  for(const auto& f:features_){
    if(frame_-f.birth_frame>=size_t(cfg_.max_lifetime))continue;
    Vec3 p_L=landmarkInLidar(f.world,R,t,R_BL_,t_BL_);auto q=image_->projector.project(p_L);Sample s;
    if(!image_->sample(q,s)||std::abs(p_L.norm()-s.depth)>cfg_.range_absolute+cfg_.range_relative*s.depth)continue;
    if(frozen_&&std::abs(s.value-f.reference)>cfg_.robust_gate*sigma_)continue;
    surviving.push_back(f);centers.push_back(q);
  }
  features_=std::move(surviving);
  const int n=cfg_.resolution;std::array<cv::Mat,6> suppression;
  for(auto& m:suppression)m=cv::Mat::zeros(n,n,CV_8U);
  auto suppress=[&](const Projection& q){
    int u=int(q.uv.x()),v=int(q.uv.y()),radius=cfg_.suppression_radius;
    for(int y=std::max(0,v-radius);y<=std::min(n-1,v+radius);++y)
      for(int x=std::max(0,u-radius);x<=std::min(n-1,u+radius);++x)
        suppression[q.face].at<uint8_t>(y,x)=255;
  };
  for(const auto& q:centers)suppress(q);
  struct Candidate{int index;Projection projection;double response;};std::vector<Candidate> candidates;
  for(int face=0;face<6;++face){
    const auto& f=image_->face(face);
    for(int index:f.point_index){
      if(index<0)continue;auto q=image_->projector.project(points_[index].p);Sample s;
      if(image_->sample(q,s)&&s.value>=cfg_.high_response)candidates.push_back({index,q,s.value});
    }
  }
  std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b){if(a.response==b.response)return a.index<b.index;return a.response>b.response;});
  for(const auto& c:candidates){
    if(features_.size()>=size_t(cfg_.max_features))break;
    const auto& q=c.projection;
    if(suppression[q.face].at<uint8_t>(int(q.uv.y()),int(q.uv.x())))continue;
    Vec3 p_B=R_BL_*points_[c.index].p+t_BL_;
    features_.push_back({R*p_B+t,c.response,timestamp,frame_,q.face});suppress(q);
  }
}
void PhotoObservation::finish(const BASIC::SE3& pose,double timestamp){
  if(!cfg_.enable)return;
  update_ms_=elapsed(update_start_);auto start=Clock::now();size_t active=features_.size();
  replenish(pose,timestamp);replenish_ms_=elapsed(start);
  Eigen::SelfAdjointEigenSolver<Mat6> eig6(geometry_);
  Eigen::SelfAdjointEigenSolver<Mat3> eig3(geometry_.bottomRightCorner<3,3>());
  Vec3 weak=eig3.eigenvectors().col(0);double photo_weak=weak.dot(last_.A.bottomRightCorner<3,3>()*weak);
  if(diagnostics_){
    diagnostics_<<frame_<<','<<timestamp<<','<<active<<','<<features_.size()<<','<<last_.valid<<','
      <<last_.mean<<','<<last_.rms<<','<<last_.median<<','<<last_.fov<<','<<last_.invalid<<','<<last_.range<<','<<last_.outlier<<','
      <<sigma_<<','<<frozen_<<','<<geometry_.trace()<<','<<last_.A.trace()<<','<<eig3.eigenvalues()[0]<<','<<photo_weak;
    for(int k=0;k<3;++k)diagnostics_<<','<<weak[k];
    for(int k=0;k<6;++k)diagnostics_<<','<<eig6.eigenvalues()[k];
    diagnostics_<<','<<deskew_ms_<<','<<image_->raster_ms<<','<<image_->idw_ms<<','<<image_->igm_ms<<','<<photo_ms_<<','<<update_ms_<<','<<replenish_ms_
      <<','<<points_.size()<<','<<image_->raw_pixels<<','<<image_->filled_pixels<<','<<image_->valid_igm_pixels<<'\n';
  }
}
} // namespace cube
