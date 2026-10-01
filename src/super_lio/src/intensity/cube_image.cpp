// CUBE-LIO independent implementation, GPL-3.0-or-later.
#include "intensity/cube_image.hpp"
#include <opencv2/imgproc.hpp>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <algorithm>
#include <chrono>

namespace cube {
namespace {
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
}
void Settings::validate() const {
  if(resolution<8||resolution>1024||idw_radius<1||idw_radius>12||idw_k<1||idw_k>625||
     idw_min_support<1||idw_min_support>idw_k||idw_power<=0||range_absolute<=0||
     range_relative<0||gaussian_sigma<=0||max_features<1||max_features>20000||
     max_lifetime<1||suppression_radius<0||normalization_frames<1||high_response<0||
     weight<0||sigma_min<=0||robust_gate<=0||huber_delta<=0)
    throw std::invalid_argument("invalid photometric configuration");
}
CubeImage::CubeImage(const Settings& cfg,MeasurementChannel channel)
  :projector(cfg.resolution),cfg_(cfg),channel_(channel){cfg_.validate();}
bool CubeImage::project(const Vec3& point,Projection& projection) const{
  projection=projector.project(point);
  return projection.face>=0;
}
void CubeImage::build(const std::vector<ScanPoint>& points){
  auto t=Clock::now();const int n=cfg_.resolution;
  candidate_points_.clear();
  for(auto& f:faces_){
    f.intensity=cv::Mat::zeros(n,n,CV_64F);f.depth=cv::Mat::zeros(n,n,CV_64F);
    f.raw_mask=cv::Mat::zeros(n,n,CV_8U);f.point_index.assign(n*n,-1);
  }
  for(size_t i=0;i<points.size();++i){
    const auto& p=points[i];auto q=projector.project(p.p);
    if(q.face<0||!std::isfinite(p.intensity))continue;
    int u=std::clamp(int(std::lround(q.uv.x())),0,n-1),v=std::clamp(int(std::lround(q.uv.y())),0,n-1);
    auto& f=faces_[q.face];double r=p.p.norm();
    if(!f.raw_mask.at<uint8_t>(v,u)||r<f.depth.at<double>(v,u)){
      f.intensity.at<double>(v,u)=p.intensity;f.depth.at<double>(v,u)=r;
      f.raw_mask.at<uint8_t>(v,u)=255;f.point_index[v*n+u]=int(i);
    }
  }
  raster_ms=ms(t);t=Clock::now();
  tbb::parallel_for(0,6,[&](int i){auto& f=faces_[i];f.mask=f.raw_mask.clone();if(cfg_.idw_enable)fill(f);});
  idw_ms=ms(t);t=Clock::now();
  if(cfg_.build_igm&&channel_==MeasurementChannel::IntensityGradientMagnitude)
    tbb::parallel_for(0,6,[&](int i){gradients(faces_[i]);});
  igm_ms=ms(t);
  raw_pixels=filled_pixels=valid_igm_pixels=0;
  for(const auto& f:faces_){
    raw_pixels+=cv::countNonZero(f.raw_mask);filled_pixels+=cv::countNonZero(f.mask);
    valid_igm_pixels+=cv::countNonZero(f.igm_mask);
    for(const int index:f.point_index)if(index>=0)candidate_points_.push_back(index);
  }
}
void CubeImage::fill(Face& f){
  const int n=cfg_.resolution,radius=cfg_.idw_radius;
  const cv::Mat raw_i=f.intensity.clone(),raw_d=f.depth.clone();
  // Only raw measurements support filling; no recursive extrapolation.
  tbb::parallel_for(tbb::blocked_range<int>(0,n),[&](const tbb::blocked_range<int>& rows){
    struct Neighbor{double d2,range,intensity;};
    std::vector<Neighbor> neighbors;neighbors.reserve((2*radius+1)*(2*radius+1));
    for(int v=rows.begin();v<rows.end();++v)for(int u=0;u<n;++u){
      if(f.raw_mask.at<uint8_t>(v,u))continue;
      neighbors.clear();
      for(int y=std::max(0,v-radius);y<=std::min(n-1,v+radius);++y)
        for(int x=std::max(0,u-radius);x<=std::min(n-1,u+radius);++x){
          int d2=(x-u)*(x-u)+(y-v)*(y-v);
          if(d2<=radius*radius&&f.raw_mask.at<uint8_t>(y,x))
            neighbors.push_back({double(d2),raw_d.at<double>(y,x),raw_i.at<double>(y,x)});
        }
      if(int(neighbors.size())<cfg_.idw_min_support)continue;
      std::sort(neighbors.begin(),neighbors.end(),[](auto a,auto b){if(a.d2==b.d2)return a.range<b.range;return a.d2<b.d2;});
      const double anchor=neighbors.front().range;
      double wsum=0,isum=0,dsum=0;int support=0;
      // A competing depth layer invalidates the fill rather than smearing edges.
      bool discontinuity=false;
      for(int k=0;k<std::min(int(neighbors.size()),cfg_.idw_k);++k){
        const auto& p=neighbors[k];
        if(std::abs(p.range-anchor)>cfg_.range_absolute+cfg_.range_relative*anchor){discontinuity=true;break;}
        double w=1./std::pow(p.d2,.5*cfg_.idw_power);
        wsum+=w;isum+=w*p.intensity;dsum+=w*p.range;++support;
      }
      if(discontinuity||support<cfg_.idw_min_support)continue;
      f.intensity.at<double>(v,u)=isum/wsum;f.depth.at<double>(v,u)=dsum/wsum;f.mask.at<uint8_t>(v,u)=255;
    }
  });
}
void CubeImage::gradients(Face& f){
  const int n=cfg_.resolution;cv::Mat smooth,smooth_mask=cv::Mat::zeros(n,n,CV_8U);
  // Equivalent Gaussian derivative: fixed 3x3 Gaussian followed by central difference.
  cv::GaussianBlur(f.intensity,smooth,cv::Size(3,3),cfg_.gaussian_sigma,cfg_.gaussian_sigma,cv::BORDER_CONSTANT);
  for(int v=1;v<n-1;++v)for(int u=1;u<n-1;++u){
    if(!f.mask.at<uint8_t>(v,u))continue;
    double anchor=f.depth.at<double>(v,u);bool valid=true;
    for(int y=v-1;y<=v+1&&valid;++y)for(int x=u-1;x<=u+1;++x)
      if(!f.mask.at<uint8_t>(y,x)||std::abs(f.depth.at<double>(y,x)-anchor)>cfg_.range_absolute+cfg_.range_relative*anchor){valid=false;break;}
    if(valid)smooth_mask.at<uint8_t>(v,u)=255;
  }
  f.igm=cv::Mat::zeros(n,n,CV_64F);f.igm_mask=cv::Mat::zeros(n,n,CV_8U);
  for(int v=2;v<n-2;++v)for(int u=2;u<n-2;++u){
    if(!smooth_mask.at<uint8_t>(v,u)||!smooth_mask.at<uint8_t>(v,u-1)||
       !smooth_mask.at<uint8_t>(v,u+1)||!smooth_mask.at<uint8_t>(v-1,u)||!smooth_mask.at<uint8_t>(v+1,u))continue;
    double dx=.5*(smooth.at<double>(v,u+1)-smooth.at<double>(v,u-1));
    double dy=.5*(smooth.at<double>(v+1,u)-smooth.at<double>(v-1,u));
    f.igm.at<double>(v,u)=std::hypot(dx,dy);f.igm_mask.at<uint8_t>(v,u)=255;
  }
  // Diagnostic pixel gradients; residual sampling differentiates its exact bilinear interpolant.
  cv::Sobel(f.igm,f.grad_u,CV_64F,1,0,1,.5,0,cv::BORDER_CONSTANT);
  cv::Sobel(f.igm,f.grad_v,CV_64F,0,1,1,.5,0,cv::BORDER_CONSTANT);
}
bool CubeImage::sample(const Projection& q,Sample& s)const{
  if(q.face<0||q.seam||!q.uv.allFinite())return false;
  int u=int(std::floor(q.uv.x())),v=int(std::floor(q.uv.y()));const int n=cfg_.resolution;
  if(u<2||v<2||u>=n-3||v>=n-3)return false;
  const auto& f=faces_[q.face];
  const bool use_igm=channel_==MeasurementChannel::IntensityGradientMagnitude;
  const cv::Mat& value_mask=use_igm?f.igm_mask:f.mask;
  for(int y=v;y<=v+1;++y)for(int x=u;x<=u+1;++x)if(!value_mask.at<uint8_t>(y,x))return false;
  double x=q.uv.x()-u,y=q.uv.y()-v;
  const cv::Mat& image=use_igm?f.igm:f.intensity;
  double a=image.at<double>(v,u),b=image.at<double>(v,u+1),c=image.at<double>(v+1,u),d=image.at<double>(v+1,u+1);
  s.value=(1-y)*((1-x)*a+x*b)+y*((1-x)*c+x*d);
  s.gradient << (1-y)*(b-a)+y*(d-c),(1-x)*(c-a)+x*(d-b);
  double depths[4]={f.depth.at<double>(v,u),f.depth.at<double>(v,u+1),f.depth.at<double>(v+1,u),f.depth.at<double>(v+1,u+1)};
  const auto mm=std::minmax_element(depths,depths+4);
  if(*mm.second-*mm.first>cfg_.range_absolute+cfg_.range_relative*(*mm.first))return false;
  s.depth=(1-y)*((1-x)*depths[0]+x*depths[1])+y*((1-x)*depths[2]+x*depths[3]);
  return std::isfinite(s.value)&&s.gradient.allFinite();
}
bool CubeImage::sampleIntensity(const Projection& q,double& intensity) const{
  Sample sample;
  if(!this->sample(q,sample))return false;
  intensity=sample.value;return true;
}
bool CubeImage::sampleGradient(const Projection& q,Eigen::Vector2d& gradient) const{
  Sample sample;
  if(!this->sample(q,sample))return false;
  gradient=sample.gradient.transpose();return true;
}
bool CubeImage::computeResidual(const Projection& q,double reference,double& residual) const{
  double current=0.;
  if(!std::isfinite(reference)||!sampleIntensity(q,current))return false;
  residual=current-reference;return std::isfinite(residual);
}
bool CubeImage::validityCheck(const Projection& q,double expected_depth,double range_absolute,
                              double range_relative,Sample& sample) const{
  if(!this->sample(q,sample)||!std::isfinite(expected_depth)||
     std::abs(expected_depth-sample.depth)>range_absolute+range_relative*sample.depth)return false;
  return true;
}
} // namespace cube
