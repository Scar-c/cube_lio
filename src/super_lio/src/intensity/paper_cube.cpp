#include "intensity/paper_cube.hpp"
#include <algorithm>
#include <chrono>
#include <limits>
#include <tbb/parallel_for.h>

namespace cube::paper {
Projector::Projector(int n):n_(n) {
  if(n<8||n>1024)throw std::invalid_argument("P4 resolution out of bounds");
}
const std::array<Basis,6>& Projector::bases() {
  // Eq. (5): gu, gv, ga. Face order is exactly the paper order.
  static const std::array<Basis,6> b{{
    {{0,-1,0},{0,0,-1},{1,0,0}},
    {{-1,0,0},{0,0,-1},{0,-1,0}},
    {{0,1,0},{0,0,-1},{-1,0,0}},
    {{1,0,0},{0,0,-1},{0,1,0}},
    {{0,-1,0},{1,0,0},{0,0,1}},
    {{0,-1,0},{-1,0,0},{0,0,-1}}
  }};
  return b;
}
Projection Projector::project(const Vec3& p) const {
  Projection q;
  if(!p.allFinite()||p.squaredNorm()<1e-16)return q;
  const Vec3 a=p.cwiseAbs();
  // Eq. (4) overlaps on X/Y ties. Deterministic implementation: X,Y,Z.
  if(a.x()>=a.y()&&a.x()>=a.z())q.face=p.x()>0?0:2;
  else if(a.y()>=a.z())q.face=p.y()>0?3:1;
  else q.face=p.z()>0?4:5;
  const auto& b=bases()[q.face];
  const double d=b.a.dot(p),u=b.u.dot(p),v=b.v.dot(p),scale=n_*.5;
  q.uv<<scale*(1+u/d),scale*(1+v/d);
  q.jacobian.row(0)=scale*(b.u/d-u*b.a/(d*d)).transpose();
  q.jacobian.row(1)=scale*(b.v/d-v*b.a/(d*d)).transpose();
  q.seam=std::abs(d-std::max(std::abs(u),std::abs(v)))<=1e-8*d;
  return q;
}
Vec3 Projector::inverse(int face,const Eigen::Vector2d& uv) const {
  if(face<0||face>=6||!uv.allFinite())throw std::invalid_argument("invalid inverse projection");
  const auto& b=bases()[face];
  return b.a+(2*uv.x()/n_-1)*b.u+(2*uv.y()/n_-1)*b.v;
}
Vec3 Projector::reconstruct(int face,const Eigen::Vector2d& uv,double range) const {
  if(!std::isfinite(range)||range<=0)throw std::invalid_argument("invalid feature range");
  return range*inverse(face,uv).normalized();
}
Projection Projector::remap(int face,const Eigen::Vector2d& uv) const {
  if(face<0||face>=6||!uv.allFinite())return {};
  Projection q;
  if(uv.x()>=0&&uv.x()<n_&&uv.y()>=0&&uv.y()<n_) {
    q.face=face;q.uv=uv;return q;
  }
  return project(inverse(face,uv)); // literal Eq. (15), including exact ties
}
void Config::validate() const {
  if(resolution<8||resolution>1024||idw_radius<1||idw_radius>12||idw_k<1||
     idw_min_support<1||idw_min_support>idw_k||!std::isfinite(idw_power)||idw_power<=0||
     !std::isfinite(range_absolute)||range_absolute<=0||!std::isfinite(range_relative)||range_relative<0||
     !std::isfinite(gaussian_sigma)||gaussian_sigma<=0||gaussian_sigma>4)
    throw std::invalid_argument("invalid P4 frame configuration");
}
Frame::Frame(Config cfg):projector(cfg.resolution),cfg_(cfg) { cfg_.validate(); }
void Frame::build(const std::vector<Point>& points) {
  const auto start=std::chrono::steady_clock::now();stats_=Statistics{};
  const int n=cfg_.resolution;
  for(auto& f:faces_) {
    f.intensity=cv::Mat::zeros(n,n,CV_64F);f.range=cv::Mat::zeros(n,n,CV_64F);
    f.raw_mask=cv::Mat::zeros(n,n,CV_8U);f.support_count=cv::Mat::zeros(n,n,CV_32S);
    f.fill_distance=cv::Mat::ones(n,n,CV_64F)*(-1.);
  }
  for(const auto& p:points) {
    const auto q=projector.project(p.body);const double range=p.body.norm();
    if(q.face<0||!std::isfinite(p.intensity)||!std::isfinite(range)||range<=0)continue;
    // Equation has closed +edges at r. Last bin owns those exact boundary points.
    const int u=std::clamp(int(std::floor(q.uv.x())),0,n-1);
    const int v=std::clamp(int(std::floor(q.uv.y())),0,n-1);
    auto& f=faces_[q.face];
    if(!f.raw_mask.at<uint8_t>(v,u)||range<f.range.at<double>(v,u)) {
      f.intensity.at<double>(v,u)=p.intensity;f.range.at<double>(v,u)=range;
      f.raw_mask.at<uint8_t>(v,u)=255;f.support_count.at<int>(v,u)=1;
      f.fill_distance.at<double>(v,u)=0;
    }
  }
  for(auto& f:faces_)f.mask=f.raw_mask.clone();
  // Raw values are copied inside fill; faces never use synthesized support.
  if(cfg_.idw)tbb::parallel_for(0,6,[&](int f){fill(f);});
  tbb::parallel_for(0,6,[&](int f){igm(f);});
  for(const auto& f:faces_) {
    stats_.raw_valid_pixels+=cv::countNonZero(f.raw_mask);
    stats_.valid_range_pixels+=cv::countNonZero(f.mask);
    stats_.valid_igm_pixels+=cv::countNonZero(f.igm_mask);
  }
  stats_.idw_filled_pixels=stats_.valid_range_pixels-stats_.raw_valid_pixels;
  stats_.image_generation_ms=std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now()-start).count();
}
void Frame::fill(int face) {
  auto& f=faces_[face];const int n=cfg_.resolution,r=cfg_.idw_radius;
  const cv::Mat raw_i=f.intensity.clone(),raw_r=f.range.clone();
  struct Neighbor { double distance,range,intensity; int pixel; };
  std::vector<Neighbor> neighbors;
  for(int v=0;v<n;++v)for(int u=0;u<n;++u) {
    if(f.raw_mask.at<uint8_t>(v,u))continue;
    neighbors.clear();
    for(int y=std::max(0,v-r);y<=std::min(n-1,v+r);++y)
      for(int x=std::max(0,u-r);x<=std::min(n-1,u+r);++x) {
        const int d2=(x-u)*(x-u)+(y-v)*(y-v);
        if(d2<=r*r&&f.raw_mask.at<uint8_t>(y,x))
          neighbors.push_back({std::sqrt(double(d2)),raw_r.at<double>(y,x),raw_i.at<double>(y,x),y*n+x});
      }
    if(int(neighbors.size())<cfg_.idw_min_support)continue;
    std::sort(neighbors.begin(),neighbors.end(),[](const auto& a,const auto& b) {
      if(a.distance!=b.distance)return a.distance<b.distance;
      if(a.range!=b.range)return a.range<b.range;
      return a.pixel<b.pixel;
    });
    const double anchor=neighbors.front().range;
    const int count=std::min(cfg_.idw_k,int(neighbors.size()));
    double wsum=0,isum=0,rsum=0;bool discontinuity=false;
    for(int i=0;i<count;++i) {
      const auto& p=neighbors[i];
      if(std::abs(p.range-anchor)>cfg_.range_absolute+cfg_.range_relative*anchor) {
        discontinuity=true;break;
      }
      const double w=1/std::pow(p.distance,cfg_.idw_power);
      wsum+=w;isum+=w*p.intensity;rsum+=w*p.range;
    }
    if(discontinuity)continue;
    f.intensity.at<double>(v,u)=isum/wsum;f.range.at<double>(v,u)=rsum/wsum;
    f.mask.at<uint8_t>(v,u)=255;f.support_count.at<int>(v,u)=count;
    f.fill_distance.at<double>(v,u)=neighbors.front().distance;
  }
}
bool Frame::texel(int face,int u,int v,Channel channel,double& value) const {
  const int n=cfg_.resolution;const auto q=projector.remap(face,{double(u),double(v)});
  if(q.face<0)return false;
  // Exact ties in Eq. (4)/(15) can return u or v == r. Treat these as boundary
  // bins. This explicit engineering extension avoids out-of-bounds recursion.
  const int x=std::clamp(int(std::lround(q.uv.x())),0,n-1);
  const int y=std::clamp(int(std::lround(q.uv.y())),0,n-1);
  const auto& f=faces_[q.face];
  const auto& mask=channel==Channel::Igm?f.igm_mask:f.mask;
  if(mask.empty()||!mask.at<uint8_t>(y,x))return false;
  const auto& im=channel==Channel::Intensity?f.intensity:(channel==Channel::Range?f.range:f.igm);
  value=im.at<double>(y,x);return std::isfinite(value);
}
bool Frame::sample(int face,const Eigen::Vector2d& uv,Channel channel,double& value) const {
  const auto q=projector.remap(face,uv);
  if(q.face<0)return false;
  const int u=int(std::floor(q.uv.x())),v=int(std::floor(q.uv.y()));
  const double a=q.uv.x()-u,b=q.uv.y()-v;double p00,p10,p01,p11;
  if(!texel(q.face,u,v,channel,p00)||!texel(q.face,u+1,v,channel,p10)||
     !texel(q.face,u,v+1,channel,p01)||!texel(q.face,u+1,v+1,channel,p11))return false;
  value=(1-b)*((1-a)*p00+a*p10)+b*((1-a)*p01+a*p11);
  return std::isfinite(value);
}
void Frame::igm(int face) {
  auto& f=faces_[face];const int n=cfg_.resolution;
  f.igm=cv::Mat::zeros(n,n,CV_64F);f.gx=cv::Mat::zeros(n,n,CV_64F);
  f.gy=cv::Mat::zeros(n,n,CV_64F);f.igm_mask=cv::Mat::zeros(n,n,CV_8U);
  const int r=int(std::ceil(3*cfg_.gaussian_sigma));
  std::vector<double> g(2*r+1),d(2*r+1);double sum=0,moment=0;
  for(int k=-r;k<=r;++k) {g[k+r]=std::exp(-.5*k*k/(cfg_.gaussian_sigma*cfg_.gaussian_sigma));sum+=g[k+r];}
  for(int k=-r;k<=r;++k) {g[k+r]/=sum;d[k+r]=k*g[k+r]/(cfg_.gaussian_sigma*cfg_.gaussian_sigma);moment+=k*d[k+r];}
  for(auto& x:d)x/=moment;
  for(int v=0;v<n;++v)for(int u=0;u<n;++u) {
    if(!f.mask.at<uint8_t>(v,u))continue;
    const double anchor=f.range.at<double>(v,u);double gx=0,gy=0;bool valid=true;
    for(int dy=-r;dy<=r&&valid;++dy)for(int dx=-r;dx<=r;++dx) {
      double intensity,range;
      if(!sample(face,{double(u+dx),double(v+dy)},Channel::Intensity,intensity)||
         !sample(face,{double(u+dx),double(v+dy)},Channel::Range,range)||
         std::abs(range-anchor)>cfg_.range_absolute+cfg_.range_relative*anchor) {valid=false;break;}
      gx+=d[dx+r]*g[dy+r]*intensity;gy+=g[dx+r]*d[dy+r]*intensity;
    }
    if(valid) {f.gx.at<double>(v,u)=gx;f.gy.at<double>(v,u)=gy;
      f.igm.at<double>(v,u)=std::hypot(gx,gy);f.igm_mask.at<uint8_t>(v,u)=255;}
  }
}
bool Frame::gradient(int face,const Eigen::Vector2d& uv,Channel channel,Eigen::RowVector2d& out) const {
  for(int axis=0;axis<2;++axis) {
    double value[2];
    for(int sign=0;sign<2;++sign) {
      auto offset=uv;offset[axis]+=sign?1:-1;
      ++stats_.seam_gradient_queries;
      if(offset.x()<0||offset.y()<0||offset.x()>=cfg_.resolution||offset.y()>=cfg_.resolution)
        ++stats_.seam_remapped_queries;
      if(!sample(face,offset,channel,value[sign])) {++stats_.seam_failed_queries;return false;}
    }
    out[axis]=.5*(value[1]-value[0]);
  }
  return out.allFinite();
}
Linearization linearize(const Frame& frame,const Vec3& world,double reference,
                        const Mat3& R,const Vec3& t,Channel channel) {
  Linearization out;const Vec3 body=R.transpose()*(world-t);
  const auto q=frame.projector.project(body);double value;Eigen::RowVector2d gradient;
  if(q.face<0||!std::isfinite(reference)||!frame.sample(q.face,q.uv,channel,value)||
     !frame.gradient(q.face,q.uv,channel,gradient))return out;
  out.residual=value-reference;
  out.H=gradient*q.jacobian*inverseTransformJacobian(world,R,t);out.valid=out.H.allFinite();
  return out;
}
} // namespace cube::paper
