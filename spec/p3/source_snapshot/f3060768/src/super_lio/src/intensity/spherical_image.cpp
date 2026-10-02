#include "intensity/spherical_image.hpp"

#include <opencv2/imgproc.hpp>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace cube {
namespace {
constexpr double kPi=3.141592653589793238462643383279502884;
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
int wrap(int x,int width){x%=width;return x<0?x+width:x;}
}

SphericalImage::SphericalImage(const Settings& settings,MeasurementChannel channel)
  :cfg_(settings),channel_(channel),
   width_(std::max(16,static_cast<int>(std::lround(kPi*settings.resolution)))),
   height_(std::max(8,static_cast<int>(std::lround(.5*kPi*settings.resolution)))){
  cfg_.validate();
}

bool SphericalImage::project(const Vec3& p,Projection& q) const{
  q=Projection{};
  const double r2=p.squaredNorm(),rho2=p.x()*p.x()+p.y()*p.y();
  if(!p.allFinite()||r2<1e-16||rho2<1e-12*r2)return false;
  const double r=std::sqrt(r2),rho=std::sqrt(rho2);
  const double longitude=std::atan2(p.y(),p.x());
  const double latitude=std::asin(std::clamp(p.z()/r,-1.,1.));
  q.face=0;
  q.uv.x()=(longitude+kPi)*(width_/(2.*kPi));
  if(q.uv.x()>=width_)q.uv.x()-=width_;
  q.uv.y()=(.5*kPi-latitude)*(height_/kPi);
  q.jacobian.row(0)<<-p.y()/rho2,p.x()/rho2,0.;
  q.jacobian.row(0)*=width_/(2.*kPi);
  q.jacobian.row(1)<<-p.x()*p.z()/(r2*rho),-p.y()*p.z()/(r2*rho),rho/r2;
  q.jacobian.row(1)*=-height_/kPi;
  return q.uv.allFinite()&&q.jacobian.allFinite()&&q.uv.y()>=0.&&q.uv.y()<height_;
}

void SphericalImage::build(const std::vector<ScanPoint>& points){
  auto start=Clock::now();
  chart_.intensity=cv::Mat::zeros(height_,width_,CV_64F);
  chart_.depth=cv::Mat::zeros(height_,width_,CV_64F);
  chart_.raw_mask=cv::Mat::zeros(height_,width_,CV_8U);
  chart_.point_index.assign(static_cast<std::size_t>(height_)*width_,-1);
  candidate_points_.clear();
  for(std::size_t i=0;i<points.size();++i){
    const auto& point=points[i];Projection q;
    if(!std::isfinite(point.intensity)||!project(point.p,q))continue;
    const int u=wrap(static_cast<int>(std::lround(q.uv.x())),width_);
    const int v=std::clamp(static_cast<int>(std::lround(q.uv.y())),0,height_-1);
    const double range=point.p.norm();
    if(!chart_.raw_mask.at<uint8_t>(v,u)||range<chart_.depth.at<double>(v,u)){
      chart_.intensity.at<double>(v,u)=point.intensity;
      chart_.depth.at<double>(v,u)=range;
      chart_.raw_mask.at<uint8_t>(v,u)=255;
      chart_.point_index[static_cast<std::size_t>(v)*width_+u]=static_cast<int>(i);
    }
  }
  raster_ms=ms(start);start=Clock::now();
  chart_.mask=chart_.raw_mask.clone();
  if(cfg_.idw_enable)fill();
  idw_ms=ms(start);start=Clock::now();
  if(cfg_.build_igm&&channel_==MeasurementChannel::IntensityGradientMagnitude)gradients();
  igm_ms=ms(start);
  raw_pixels=cv::countNonZero(chart_.raw_mask);
  filled_pixels=cv::countNonZero(chart_.mask);
  valid_igm_pixels=cv::countNonZero(chart_.igm_mask);
  for(const int index:chart_.point_index)if(index>=0)candidate_points_.push_back(index);
}

void SphericalImage::fill(){
  const cv::Mat raw_i=chart_.intensity.clone(),raw_d=chart_.depth.clone();
  const int radius=cfg_.idw_radius;
  tbb::parallel_for(tbb::blocked_range<int>(0,height_),[&](const tbb::blocked_range<int>& rows){
    struct Neighbor{double d2,range,intensity;};
    std::vector<Neighbor> neighbors;neighbors.reserve((2*radius+1)*(2*radius+1));
    for(int v=rows.begin();v<rows.end();++v)for(int u=0;u<width_;++u){
      if(chart_.raw_mask.at<uint8_t>(v,u))continue;
      neighbors.clear();
      for(int dy=-radius;dy<=radius;++dy){
        const int y=v+dy;if(y<0||y>=height_)continue;
        for(int dx=-radius;dx<=radius;++dx){
          const int d2=dx*dx+dy*dy;if(d2>radius*radius)continue;
          const int x=wrap(u+dx,width_);
          if(chart_.raw_mask.at<uint8_t>(y,x))
            neighbors.push_back({double(d2),raw_d.at<double>(y,x),raw_i.at<double>(y,x)});
        }
      }
      if(static_cast<int>(neighbors.size())<cfg_.idw_min_support)continue;
      std::sort(neighbors.begin(),neighbors.end(),[](const auto& a,const auto& b){
        return a.d2==b.d2?a.range<b.range:a.d2<b.d2;
      });
      const double anchor=neighbors.front().range;
      double weight_sum=0.,intensity_sum=0.,depth_sum=0.;int support=0;
      bool discontinuity=false;
      for(int i=0;i<std::min(static_cast<int>(neighbors.size()),cfg_.idw_k);++i){
        const auto& neighbor=neighbors[i];
        if(std::abs(neighbor.range-anchor)>cfg_.range_absolute+cfg_.range_relative*anchor){
          discontinuity=true;break;
        }
        const double weight=1./std::pow(neighbor.d2,.5*cfg_.idw_power);
        weight_sum+=weight;intensity_sum+=weight*neighbor.intensity;
        depth_sum+=weight*neighbor.range;++support;
      }
      if(discontinuity||support<cfg_.idw_min_support)continue;
      chart_.intensity.at<double>(v,u)=intensity_sum/weight_sum;
      chart_.depth.at<double>(v,u)=depth_sum/weight_sum;
      chart_.mask.at<uint8_t>(v,u)=255;
    }
  });
}

void SphericalImage::gradients(){
  cv::Mat smooth=cv::Mat::zeros(height_,width_,CV_64F);
  cv::Mat smooth_mask=cv::Mat::zeros(height_,width_,CV_8U);
  const double side=std::exp(-.5/(cfg_.gaussian_sigma*cfg_.gaussian_sigma));
  const double norm=1./(1.+2.*side);
  const double w[3]={side*norm,norm,side*norm};
  for(int v=1;v<height_-1;++v)for(int u=0;u<width_;++u){
    if(!chart_.mask.at<uint8_t>(v,u))continue;
    const double anchor=chart_.depth.at<double>(v,u);bool valid=true;
    for(int dy=-1;dy<=1&&valid;++dy)for(int dx=-1;dx<=1;++dx){
      const int y=v+dy,x=wrap(u+dx,width_);
      if(!chart_.mask.at<uint8_t>(y,x)||
         std::abs(chart_.depth.at<double>(y,x)-anchor)>cfg_.range_absolute+cfg_.range_relative*anchor){
        valid=false;break;
      }
    }
    if(!valid)continue;
    double value=0.;
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)
      value+=w[dy+1]*w[dx+1]*chart_.intensity.at<double>(v+dy,wrap(u+dx,width_));
    smooth.at<double>(v,u)=value;smooth_mask.at<uint8_t>(v,u)=255;
  }
  chart_.igm=cv::Mat::zeros(height_,width_,CV_64F);
  chart_.igm_mask=cv::Mat::zeros(height_,width_,CV_8U);
  for(int v=2;v<height_-2;++v)for(int u=0;u<width_;++u){
    const int left=wrap(u-1,width_),right=wrap(u+1,width_);
    if(!smooth_mask.at<uint8_t>(v,u)||!smooth_mask.at<uint8_t>(v,left)||
       !smooth_mask.at<uint8_t>(v,right)||!smooth_mask.at<uint8_t>(v-1,u)||
       !smooth_mask.at<uint8_t>(v+1,u))continue;
    const double dx=.5*(smooth.at<double>(v,right)-smooth.at<double>(v,left));
    const double dy=.5*(smooth.at<double>(v+1,u)-smooth.at<double>(v-1,u));
    chart_.igm.at<double>(v,u)=std::hypot(dx,dy);
    chart_.igm_mask.at<uint8_t>(v,u)=255;
  }
}

bool SphericalImage::sample(const Projection& q,Sample& s)const{
  if(q.face!=0||!q.uv.allFinite()||q.uv.x()<0.||q.uv.x()>=width_)return false;
  const int u=static_cast<int>(std::floor(q.uv.x()));
  const int v=static_cast<int>(std::floor(q.uv.y()));
  if(v<2||v>=height_-3)return false;
  const int u1=wrap(u+1,width_);const double x=q.uv.x()-u,y=q.uv.y()-v;
  const bool use_igm=channel_==MeasurementChannel::IntensityGradientMagnitude;
  const cv::Mat& image=use_igm?chart_.igm:chart_.intensity;
  const cv::Mat& mask=use_igm?chart_.igm_mask:chart_.mask;
  if(!mask.at<uint8_t>(v,u)||!mask.at<uint8_t>(v,u1)||
     !mask.at<uint8_t>(v+1,u)||!mask.at<uint8_t>(v+1,u1))return false;
  const double a=image.at<double>(v,u),b=image.at<double>(v,u1);
  const double c=image.at<double>(v+1,u),d=image.at<double>(v+1,u1);
  s.value=(1.-y)*((1.-x)*a+x*b)+y*((1.-x)*c+x*d);
  s.gradient<<(1.-y)*(b-a)+y*(d-c),(1.-x)*(c-a)+x*(d-b);
  const double depth[4]={chart_.depth.at<double>(v,u),chart_.depth.at<double>(v,u1),
                         chart_.depth.at<double>(v+1,u),chart_.depth.at<double>(v+1,u1)};
  const auto mm=std::minmax_element(depth,depth+4);
  if(*mm.second-*mm.first>cfg_.range_absolute+cfg_.range_relative*(*mm.first))return false;
  s.depth=(1.-y)*((1.-x)*depth[0]+x*depth[1])+y*((1.-x)*depth[2]+x*depth[3]);
  return std::isfinite(s.value)&&std::isfinite(s.depth)&&s.gradient.allFinite();
}

bool SphericalImage::sampleIntensity(const Projection& q,double& intensity)const{
  Sample sample;if(!this->sample(q,sample))return false;intensity=sample.value;return true;
}
bool SphericalImage::sampleGradient(const Projection& q,Eigen::Vector2d& gradient)const{
  Sample sample;if(!this->sample(q,sample))return false;gradient=sample.gradient.transpose();return true;
}
bool SphericalImage::computeResidual(const Projection& q,double reference,double& residual)const{
  double current=0.;if(!std::isfinite(reference)||!sampleIntensity(q,current))return false;
  residual=current-reference;return std::isfinite(residual);
}
bool SphericalImage::validityCheck(const Projection& q,double expected_depth,double range_absolute,
                                   double range_relative,Sample& sample)const{
  if(!this->sample(q,sample)||!std::isfinite(expected_depth)||
     std::abs(expected_depth-sample.depth)>range_absolute+range_relative*sample.depth)return false;
  return true;
}

} // namespace cube
