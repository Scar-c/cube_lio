// Independent GPLv3 implementation; see photo_observation.hpp for provenance.
#include "intensity/photo_observation.hpp"
#include "intensity/spherical_image.hpp"
#include "intensity/coin/coin_feature_manager.hpp"
#include "intensity/coin/super_degeneracy_gate.hpp"
#include "lio/params.h"
#include <Eigen/Eigenvalues>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
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
  nh.param<std::string>("/photo/projection",projection_name_,"cubemap");
  nh.param<std::string>("/photo/measurement",measurement_name_,"igm");
  nh.param<std::string>("/photo/selector",selector_mode_,"all");
  nh.param("/photo/weakest_gate_threshold",weakest_gate_threshold_,0.31913064578672057);
  if(projection_name_!="cubemap"&&projection_name_!="equirectangular")
    throw std::invalid_argument("unknown intensity projection: "+projection_name_);
  if(measurement_name_!="raw"&&measurement_name_!="igm")
    throw std::invalid_argument("unknown intensity measurement channel: "+measurement_name_);
  if(selector_mode_!="all"&&selector_mode_!="weakest")
    throw std::invalid_argument("unknown photometric selector: "+selector_mode_);
  if(!std::isfinite(weakest_gate_threshold_)||weakest_gate_threshold_<0.)
    throw std::invalid_argument("photo weakest-direction gate threshold must be finite and nonnegative");
  cfg_.build_igm=measurement_name_=="igm";
  cfg_.validate();sigma_=cfg_.sigma_min;
  std::string policy;nh.param<std::string>("/photo/information_policy",policy,"C0");
  policy_=parsePolicy(policy);
  nh.param("/photo/information_audit",audit_enabled_,false);
  if(!cfg_.enable)return;
  cv::setNumThreads(1); // TBB owns frontend parallelism; avoid nested OpenCV pools.
  R_BL_=LI2Sup::g_lidar_imu.R_.cast<double>();t_BL_=LI2Sup::g_lidar_imu.t_.cast<double>();
  const MeasurementChannel channel=cfg_.build_igm?
      MeasurementChannel::IntensityGradientMagnitude:MeasurementChannel::RawIntensity;
  if(projection_name_=="cubemap")image_=std::make_unique<CubeImage>(cfg_,channel);
  else image_=std::make_unique<SphericalImage>(cfg_,channel);
  std::string dir;nh.getParam("/lio/offline/out_dir",dir);
  if(!dir.empty()){
    if(audit_enabled_){
      audit_.open(dir+"/information.csv");
      if(!audit_)throw std::runtime_error("cannot write information audit");
      audit_<<std::setprecision(17);
    }
    diagnostics_.open(dir+"/photo.csv");
    if(!diagnostics_)throw std::runtime_error("cannot write photometric diagnostics");
    diagnostics_<<std::setprecision(17);
    diagnostics_<<"frame,timestamp,projection,measurement,selector,weakest_gate_confidence,weakest_gate_active,active_before,active_after,valid,residual_mean,residual_rms,residual_median,reject_fov,reject_invalid,reject_range,reject_outlier,sigma,frozen,trace_Ag,trace_Ap,weak_translation_eigenvalue,photo_in_weak_translation,weak_x,weak_y,weak_z,geo_eig0,geo_eig1,geo_eig2,geo_eig3,geo_eig4,geo_eig5,deskew_ms,raster_ms,idw_ms,igm_ms,photo_jacobian_ms,update_ms,replenish_ms,raw_points,raw_pixels,filled_pixels,igm_pixels,deskew_supported\n";
  }
}
void PhotoObservation::prepare(const LI2Sup::MeasureGroup& measures,
                              const std::vector<LI2Sup::DynamicState>& history,const BASIC::SE3& predicted){
  if(!cfg_.enable)return;
  ++frame_;audit_iteration_=0;predicted_pose_=predicted;photo_ms_=0;auto start=Clock::now();
  const auto& raw=measures.lidar.pc_intensity;
  if(!raw)throw std::runtime_error("missing dense intensity scan");
  frame_supported_=history.size()>=2 && history.back().time>history.front().time;
  if(!frame_supported_){
    // An inherited synchronization outcome, e.g. a bag IMU gap. Keep the
    // geometry update and output frame; never constrain it with a stale image.
    points_.clear();deskew_ms_=0;update_start_=Clock::now();return;
  }
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
  constexpr size_t kFeatureBlock=32;
  const size_t block_count=(features_.size()+kFeatureBlock-1)/kFeatureBlock;
  std::vector<Acc> accumulators(block_count);
  tbb::parallel_for(tbb::blocked_range<size_t>(0,block_count,1),[&](const tbb::blocked_range<size_t>& blocks){
    for(size_t block=blocks.begin();block<blocks.end();++block){
      auto& a=accumulators[block];
      const size_t begin=block*kFeatureBlock,end=std::min(features_.size(),begin+kFeatureBlock);
      for(size_t i=begin;i<end;++i){
      const auto& feat=features_[i];
      const Vec3 p_L=landmarkInLidar(feat.world,R,t,R_BL_,t_BL_);Projection q;
      if(!image_->project(p_L,q)||q.face<0||q.seam||p_L.norm()<std::sqrt(LI2Sup::g_blind2)||p_L.norm()>std::sqrt(LI2Sup::g_maxrange2)){++a.terms.fov;continue;}
      Sample sample;
      if(!image_->validityCheck(q,p_L.norm(),cfg_.range_absolute,cfg_.range_relative,sample)){
        if(sample.depth>0.&&std::abs(p_L.norm()-sample.depth)>cfg_.range_absolute+cfg_.range_relative*sample.depth)
          ++a.terms.range;
        else ++a.terms.invalid;
        continue;
      }
      double residual=0.;
      if(!image_->computeResidual(q,feat.reference,residual)){++a.terms.invalid;continue;}
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
    }
  });
  PhotoTerms total;std::vector<double> values;
  for(const auto& a:accumulators){
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
std::vector<ResidualContribution> PhotoObservation::auditRows(const BASIC::SE3& pose)const{
  if(!frame_supported_)return {};
  const Mat3 R=pose.R_.cast<double>();const Vec3 t=pose.t_.cast<double>();
  std::vector<ResidualContribution> slots(features_.size());
  std::vector<uint8_t> valid(features_.size(),0); // Separate bytes, never packed bits.
  tbb::parallel_for(tbb::blocked_range<size_t>(0,features_.size()),[&](const tbb::blocked_range<size_t>& rows){
    for(size_t i=rows.begin();i<rows.end();++i){
      const auto& feat=features_[i];
      const Vec3 p_L=landmarkInLidar(feat.world,R,t,R_BL_,t_BL_);Projection q;
      if(!image_->project(p_L,q)||q.face<0||q.seam||
         p_L.norm()<std::sqrt(LI2Sup::g_blind2)||p_L.norm()>std::sqrt(LI2Sup::g_maxrange2))continue;
      Sample sample;
      if(!image_->validityCheck(q,p_L.norm(),cfg_.range_absolute,cfg_.range_relative,sample))continue;
      double residual=0.;if(!image_->computeResidual(q,feat.reference,residual))continue;
      if(frozen_&&std::abs(residual)>cfg_.robust_gate*sigma_)continue;
      double weight=0;
      if(frozen_){
        const double z=std::abs(residual)/sigma_;
        const double robust=z<=cfg_.huber_delta?1.:cfg_.huber_delta/z;
        weight=cfg_.weight*robust/(sigma_*sigma_);
      }
      slots[i]={i,q.face,q.uv,feat.reference,residual,weight,
                residualJacobian(feat.world,R,t,R_BL_,t_BL_,q,sample)};
      valid[i]=1;
    }
  });
  std::vector<ResidualContribution> out;
  for(size_t i=0;i<slots.size();++i)if(valid[i])out.push_back(slots[i]);
  return out;
}
void PhotoObservation::writeAudit(const BASIC::SE3& pose,const BASIC::M6& covariance,
                                 const Vec6& geometry_b,const std::vector<ResidualContribution>& rows){
  const Mat3 delta_R=predicted_pose_.R_.cast<double>().transpose()*pose.R_.cast<double>();
  const Eigen::AngleAxisd rotation(delta_R);
  Vec6 delta;delta.head<3>()=rotation.axis()*rotation.angle();
  delta.tail<3>()=(pose.t_-predicted_pose_.t_).cast<double>();
  Mat6 G=Mat6::Identity();G.topLeftCorner<3,3>()-=.5*hat(delta.head<3>());
  const Mat6 prior=G*covariance.cast<double>()*G.transpose();
  for(auto policy:{InformationPolicy::C0,InformationPolicy::C60,InformationPolicy::C100,InformationPolicy::K100}){
    auto terms=informationBudget(rows,policy,cfg_.suppression_radius);
    auto m=auditInformation(geometry_,geometry_b,prior,G*delta,rows,terms,
                             image_?image_->coordinateResolution():cfg_.resolution);
    m["active"]=double(features_.size());m["frozen"]=frozen_;m["supported"]=frame_supported_;
    // Comparison establishes that the diagnostic sampler retains P1 robust semantics.
    const auto raw=informationBudget(rows,InformationPolicy::C0,cfg_.suppression_radius);
    m["raw_A_relative_error"]=(raw.A-last_.A).norm()/std::max(1.,last_.A.norm());
    m["raw_b_relative_error"]=(raw.b-last_.b).norm()/std::max(1.,last_.b.norm());
    m["valid_count_error"]=double(rows.size())-last_.valid;
    Eigen::SelfAdjointEigenSolver<Mat6> difference(raw.A-terms.A);
    m["authority_difference_min_eigenvalue"]=difference.eigenvalues()[0];
    if(frame_==1&&audit_iteration_==0&&policy==InformationPolicy::C0){
      audit_<<"frame,iteration,policy";for(const auto& kv:m)audit_<<','<<kv.first;audit_<<'\n';
    }
    audit_<<frame_<<','<<audit_iteration_<<','<<policyName(policy);
    for(const auto& kv:m)audit_<<','<<kv.second;audit_<<'\n';
  }
}
void PhotoObservation::add(const BASIC::SE3& pose,BASIC::M6& A,BASIC::V6& b,const BASIC::M6& prior_covariance){
  if(!cfg_.enable)return;auto start=Clock::now();
  geometry_=A.cast<double>();const Vec6 geometry_b=b.cast<double>();
  if(!frame_supported_){last_=PhotoTerms{};last_.invalid=int(features_.size());}
  else last_=observe(pose,true); // Preserve P1 raw accumulation, including C0/shadow control.
  std::vector<ResidualContribution> rows;
  if(audit_enabled_||policy_!=InformationPolicy::C0)rows=auditRows(pose);
  if(audit_enabled_)writeAudit(pose,prior_covariance,geometry_b,rows);
  if(policy_!=InformationPolicy::C0){
    const auto terms=informationBudget(rows,policy_,cfg_.suppression_radius);
    last_.A=terms.A;last_.b=terms.b;
  }
  A+=last_.A.cast<BASIC::scalar>();b+=last_.b.cast<BASIC::scalar>();
  ++audit_iteration_;photo_ms_+=elapsed(start);
}
void PhotoObservation::replenish(const BASIC::SE3& pose,double timestamp,
                                 const Eigen::MatrixXd& geometry_translation_rows){
  const Mat3 R=pose.R_.cast<double>();const Vec3 t=pose.t_.cast<double>();
  Eigen::Vector3d weak_global=Eigen::Vector3d::Zero();bool weak_valid=false;
  if(selector_mode_=="weakest"){
    const Mat3 R_GL=R*R_BL_;
    const auto geometry=coin::CoinFeatureManager::weakDirectionsFromGeometry(
        geometry_translation_rows,R_GL,25.);
    const bool previous_supported=has_previous_weak_axis_&&timestamp>previous_weak_axis_timestamp_&&
        timestamp-previous_weak_axis_timestamp_<=.25;
    const auto signal=coin::SuperDegeneracyGate::measure(
        geometry,previous_weak_axis_global_,previous_supported);
    weakest_gate_confidence_=signal.confidence;
    weakest_gate_active_=coin::SuperDegeneracyGate::activate(signal,weakest_gate_threshold_);
    if(geometry.geometry_rows>3&&geometry.eigenvectors.col(0).allFinite()){
      previous_weak_axis_global_=geometry.eigenvectors.col(0).normalized();
      previous_weak_axis_timestamp_=timestamp;has_previous_weak_axis_=true;
    }
    if(weakest_gate_active_){weak_global=geometry.eigenvectors.col(0).normalized();weak_valid=weak_global.allFinite();}
  }
  std::vector<Feature> surviving;std::vector<Projection> centers;
  for(const auto& f:features_){
    if(frame_-f.birth_frame>=size_t(cfg_.max_lifetime))continue;
    Vec3 p_L=landmarkInLidar(f.world,R,t,R_BL_,t_BL_);Projection q;Sample s;
    if(!image_->project(p_L,q)||!image_->validityCheck(q,p_L.norm(),cfg_.range_absolute,
                                                       cfg_.range_relative,s))continue;
    if(frozen_&&std::abs(s.value-f.reference)>cfg_.robust_gate*sigma_)continue;
    surviving.push_back(f);centers.push_back(q);
  }
  features_=std::move(surviving);
  std::vector<cv::Mat> suppression(image_->chartCount());
  for(int chart=0;chart<image_->chartCount();++chart)
    suppression[chart]=cv::Mat::zeros(image_->chartHeight(chart),image_->chartWidth(chart),CV_8U);
  auto suppress=[&](const Projection& q){
    if(q.face<0||q.face>=image_->chartCount())return;
    const int width=image_->chartWidth(q.face),height=image_->chartHeight(q.face);
    const int u=static_cast<int>(q.uv.x()),v=static_cast<int>(q.uv.y()),radius=cfg_.suppression_radius;
    for(int y=std::max(0,v-radius);y<=std::min(height-1,v+radius);++y)
      for(int dx=-radius;dx<=radius;++dx){
        int x=u+dx;
        if(image_->wrapsHorizontally(q.face)){x%=width;if(x<0)x+=width;}
        else if(x<0||x>=width)continue;
        suppression[q.face].at<uint8_t>(y,x)=255;
      }
  };
  for(const auto& q:centers)suppress(q);
  struct Candidate{int index;Projection projection;double reference,response,rank;};std::vector<Candidate> candidates;
  const bool use_igm=measurement_name_=="igm";
  for(const int index:image_->candidatePointIndices()){
    if(index<0||static_cast<std::size_t>(index)>=points_.size())continue;
    Projection q;Sample sample;
    if(!image_->project(points_[index].p,q)||q.face<0||q.face>=image_->chartCount()||
       !image_->validityCheck(q,points_[index].p.norm(),cfg_.range_absolute,
                              cfg_.range_relative,sample))continue;
    Eigen::Vector2d gradient=sample.gradient.transpose();
    const double response=use_igm?sample.value:gradient.norm();
    if(response>=cfg_.high_response){
      double rank=response;
      if(selector_mode_=="weakest"&&weak_valid){
        const Vec3 delta_pixel=-R_BL_.transpose()*R.transpose()*weak_global;
        const Eigen::Vector2d projected_motion=q.jacobian*delta_pixel;
        rank=std::abs((sample.gradient*projected_motion).value());
      }
      candidates.push_back({index,q,sample.value,response,rank});
    }
  }
  std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b){
    return a.rank==b.rank?a.index<b.index:a.rank>b.rank;
  });
  for(const auto& c:candidates){
    if(features_.size()>=size_t(cfg_.max_features))break;
    const auto& q=c.projection;
    int u=static_cast<int>(q.uv.x()),v=static_cast<int>(q.uv.y());
    if(image_->wrapsHorizontally(q.face)){u%=image_->chartWidth(q.face);if(u<0)u+=image_->chartWidth(q.face);}
    if(u<0||u>=image_->chartWidth(q.face)||v<0||v>=image_->chartHeight(q.face)||
       suppression[q.face].at<uint8_t>(v,u))continue;
    Vec3 p_B=R_BL_*points_[c.index].p+t_BL_;
    features_.push_back({R*p_B+t,c.reference,timestamp,frame_,q.face});suppress(q);
  }
}
void PhotoObservation::finish(const BASIC::SE3& pose,double timestamp,
                              const Eigen::MatrixXd& geometry_translation_rows){
  if(!cfg_.enable)return;
  update_ms_=elapsed(update_start_);auto start=Clock::now();size_t active=features_.size();
  weakest_gate_confidence_=0.;weakest_gate_active_=false;
  if(frame_supported_)replenish(pose,timestamp,geometry_translation_rows);
  replenish_ms_=elapsed(start);
  Eigen::SelfAdjointEigenSolver<Mat6> eig6(geometry_);
  Eigen::SelfAdjointEigenSolver<Mat3> eig3(geometry_.bottomRightCorner<3,3>());
  Vec3 weak=eig3.eigenvectors().col(0);double photo_weak=weak.dot(last_.A.bottomRightCorner<3,3>()*weak);
  if(diagnostics_){
    diagnostics_<<frame_<<','<<timestamp<<','<<projection_name_<<','<<measurement_name_<<','<<selector_mode_<<','
      <<weakest_gate_confidence_<<','<<weakest_gate_active_<<','
      <<active<<','<<features_.size()<<','<<last_.valid<<','
      <<last_.mean<<','<<last_.rms<<','<<last_.median<<','<<last_.fov<<','<<last_.invalid<<','<<last_.range<<','<<last_.outlier<<','
      <<sigma_<<','<<frozen_<<','<<geometry_.trace()<<','<<last_.A.trace()<<','<<eig3.eigenvalues()[0]<<','<<photo_weak;
    for(int k=0;k<3;++k)diagnostics_<<','<<weak[k];
    for(int k=0;k<6;++k)diagnostics_<<','<<eig6.eigenvalues()[k];
    diagnostics_<<','<<deskew_ms_<<','<<(frame_supported_?image_->rasterMilliseconds():0)<<','
      <<(frame_supported_?image_->interpolationMilliseconds():0)<<','
      <<(frame_supported_?image_->featureChannelMilliseconds():0)<<','
      <<photo_ms_<<','<<update_ms_<<','<<replenish_ms_<<','<<points_.size()<<','
      <<(frame_supported_?image_->rawPixelCount():0)<<','
      <<(frame_supported_?image_->filledPixelCount():0)<<','
      <<(frame_supported_?image_->validFeaturePixelCount():0)<<','<<frame_supported_<<'\n';
  }
}
} // namespace cube
