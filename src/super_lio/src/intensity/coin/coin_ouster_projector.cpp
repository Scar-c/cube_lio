// COIN-LIO Ouster projector reimplementation (BSD-3-Clause source authority).
#include "intensity/coin/coin_ouster_projector.hpp"
#include <ros/ros.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cube::coin {
OusterMetadata OusterMetadata::fromRosParams() {
  OusterMetadata m;
  float rows=0,cols=0,u_shift=0;
  auto required=[](bool ok,const char* name){if(!ok)throw std::runtime_error(std::string("missing COIN Ouster param: ")+name);};
  required(ros::param::get("/lidar_data_format/pixels_per_column",rows),"pixels_per_column");
  required(ros::param::get("/lidar_data_format/columns_per_frame",cols),"columns_per_frame");
  required(ros::param::get("/beam_intrinsics/lidar_origin_to_beam_origin_mm",m.beam_offset_mm),"beam_offset_mm");
  required(ros::param::get("/lidar_data_format/pixel_shift_by_row",m.pixel_shift_by_row),"pixel_shift_by_row");
  required(ros::param::get("/beam_intrinsics/beam_altitude_angles",m.beam_altitude_degrees),"beam_altitude_angles");
  required(ros::param::get("/image/u_shift",u_shift),"image/u_shift");
  m.rows=static_cast<int>(rows);m.cols=static_cast<int>(cols);m.u_shift=static_cast<int>(u_shift);
  ros::param::param("/image/destagger",m.destagger,true);
  m.validate();return m;
}

void OusterMetadata::validate() const {
  if(rows<2||cols<2||pixel_shift_by_row.size()!=static_cast<std::size_t>(rows)||
     beam_altitude_degrees.size()!=static_cast<std::size_t>(rows)||!std::isfinite(beam_offset_mm))
    throw std::invalid_argument("invalid COIN Ouster metadata dimensions or calibration");
  if(u_shift<0||u_shift>=cols)throw std::invalid_argument("invalid COIN Ouster image/u_shift");
  for(double a:beam_altitude_degrees)if(!std::isfinite(a))throw std::invalid_argument("nonfinite Ouster elevation angle");
}

CoinOusterProjector::CoinOusterProjector(OusterMetadata metadata):metadata_(std::move(metadata)) {
  metadata_.validate();
  elevation_radians_.reserve(metadata_.beam_altitude_degrees.size());
  for(double a:metadata_.beam_altitude_degrees)elevation_radians_.push_back(a*M_PI/180.);
  const double fy=-static_cast<double>(metadata_.rows)/std::abs(elevation_radians_.front()-elevation_radians_.back());
  const double fx=-static_cast<double>(metadata_.cols)/(2*M_PI);
  K_<<fx,0,metadata_.cols/2,0,fy,metadata_.rows/2,0,0,1;
  beam_offset_m_=metadata_.beam_offset_mm*1e-3;
  raw_to_row_.assign(metadata_.rows*metadata_.cols,0);
  raw_to_col_.assign(metadata_.rows*metadata_.cols,0);
  for(int row=0;row<metadata_.rows;++row)for(int col=0;col<metadata_.cols;++col){
    const auto raw=indexFromPixel(row,col);
    raw_to_row_[raw]=row;
    int destaggered=col-metadata_.u_shift;
    if(destaggered<0)destaggered+=metadata_.cols;
    if(destaggered>=metadata_.cols)destaggered-=metadata_.cols;
    raw_to_col_[raw]=destaggered;
  }
}

std::size_t CoinOusterProjector::indexFromPixel(int row,int col) const {
  const int v=(col+metadata_.cols-metadata_.pixel_shift_by_row.at(row))%metadata_.cols;
  return static_cast<std::size_t>(row)*metadata_.cols+(metadata_.destagger?v:col);
}

void CoinOusterProjector::pixelFromRawIndex(std::size_t raw,int& row,int& col) const {
  if(raw>=raw_to_row_.size())throw std::out_of_range("COIN Ouster raw point index exceeds calibrated image");
  row=raw_to_row_[raw];col=raw_to_col_[raw];
}

ProjectedPoint CoinOusterProjector::project(const Vec3& p) const {
  ProjectedPoint out;
  if(!p.allFinite())return out;
  const double L=std::sqrt(p.x()*p.x()+p.y()*p.y())-beam_offset_m_;
  const double R=std::sqrt(p.z()*p.z()+L*L);
  if(!(R>0)||!std::isfinite(R))return out;
  const double phi=std::atan2(p.y(),p.x());
  const double theta=std::asin(std::clamp(p.z()/R,-1.,1.));
  out.uv.x()=K_(0,0)*phi+K_(0,2);
  if(theta>elevation_radians_.front()){out.uv.y()=0;return out;}
  if(theta<elevation_radians_.back()){out.uv.y()=metadata_.rows-1;return out;}
  const auto greater=(std::upper_bound(elevation_radians_.rbegin(),elevation_radians_.rend(),theta)+1).base();
  const auto smaller=greater+1;
  if(greater==elevation_radians_.end())out.uv.y()=metadata_.rows-1;
  else {
    out.uv.y()=std::distance(elevation_radians_.begin(),greater);
    out.uv.y()+=(*greater-theta)/(*greater-*smaller);
  }
  out.in_fov=out.uv.x()>=0&&out.uv.x()<=metadata_.cols-1&&out.uv.y()>=0&&out.uv.y()<=metadata_.rows-1;
  return out;
}

Mat23 CoinOusterProjector::projectionJacobian(const Vec3& p) const {
  const double rxy=p.head<2>().norm(),L=rxy-beam_offset_m_,R2=L*L+p.z()*p.z();
  const double irxy=1./rxy,irxy2=irxy*irxy,fx_irxy2=K_(0,0)*irxy2;
  Mat23 J;
  J<<-fx_irxy2*p.y(),fx_irxy2*p.x(),0,
      -K_(1,1)*p.x()*p.z()/(L*R2),-K_(1,1)*p.y()*p.z()/(L*R2),K_(1,1)*L/R2;
  return J;
}
} // namespace cube::coin
