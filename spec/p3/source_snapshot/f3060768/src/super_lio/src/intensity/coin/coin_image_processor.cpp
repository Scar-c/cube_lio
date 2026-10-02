// COIN-LIO image processing reimplementation (BSD-3-Clause source authority).
#include "intensity/coin/coin_image_processor.hpp"
#include <ros/ros.h>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cube::coin {
CoinImageSettings CoinImageSettings::fromRosParams(){
  CoinImageSettings s;ros::NodeHandle nh;
  nh.param("image/reflectivity",s.reflectivity,false);
  nh.param("image/line_removal",s.line_removal,true);
  nh.param("image/brightness_filter",s.brightness_filter,true);
  nh.param("image/blur",s.blur,true);
  nh.param("image/intensity_scale",s.intensity_scale,.25);
  nh.param("image/erosion_margin",s.erosion_margin,2);
  nh.param("image/patch_size",s.patch_size,5);
  std::vector<int> window,masks;std::vector<double> hpf,lpf;
  if(!nh.getParam("image/window",window)||window.size()!=2)throw std::runtime_error("COIN image/window must contain two integers");
  if(!nh.getParam("image/masks",masks)||masks.size()%4!=0)throw std::runtime_error("COIN image/masks malformed");
  if(!nh.getParam("image/highpass",hpf)||hpf.empty())throw std::runtime_error("COIN image/highpass is missing");
  if(!nh.getParam("image/lowpass",lpf)||lpf.empty())throw std::runtime_error("COIN image/lowpass is missing");
  s.brightness_window=cv::Size(window[0],window[1]);s.highpass=std::move(hpf);s.lowpass=std::move(lpf);
  for(std::size_t i=0;i<masks.size();i+=4)s.masks.emplace_back(masks[i],masks[i+1],masks[i+2],masks[i+3]);
  nh.param("image/min_range",s.min_range,.7);nh.param("image/max_range",s.max_range,30.);
  return s;
}

CoinImageProcessor::CoinImageProcessor(CoinOusterProjector projector,CoinImageSettings settings)
  :projector_(std::move(projector)),settings_(std::move(settings)){
  if(settings_.patch_size<1||settings_.brightness_window.width<1||settings_.brightness_window.height<1||
     settings_.intensity_scale<=0||settings_.min_range<0||settings_.max_range<=settings_.min_range||
     settings_.highpass.empty()||settings_.lowpass.empty())throw std::invalid_argument("invalid official COIN image settings");
  for(const auto& r:settings_.masks)if(r.x<0||r.y<0||r.x+r.width>projector_.cols()||r.y+r.height>projector_.rows())
    throw std::invalid_argument("COIN image mask lies outside calibrated image");
}

CoinFrame CoinImageProcessor::process(std::vector<CoinScanPoint>& points) const {
  const int rows=projector_.rows(),cols=projector_.cols();
  CoinFrame frame;
  frame.intensity=cv::Mat::zeros(rows,cols,CV_32FC1);
  frame.range=cv::Mat::zeros(rows,cols,CV_32FC1);
  frame.image_index=cv::Mat::ones(rows,cols,CV_32SC1)*(-1);
  frame.projected_index.assign(static_cast<std::size_t>(rows)*cols*10,0);
  std::vector<int> u(points.size(),-1),v(points.size(),-1);
  for(std::size_t j=0;j<points.size();++j){
    auto q=projector_.project(points[j].point_lidar);
    if(!q.in_fov)continue;
    u[j]=static_cast<int>(std::round(q.uv.x()));v[j]=static_cast<int>(std::round(q.uv.y()));
  }
  for(std::size_t j=0;j<points.size();++j){
    int row,col;projector_.pixelFromRawIndex(points[j].raw_index,row,col);
    frame.range.ptr<float>(row)[col]=points[j].range;
    frame.intensity.ptr<float>(row)[col]=points[j].intensity;
    frame.image_index.ptr<int>(row)[col]=static_cast<int>(j);
  }
  frame.raw_intensity=frame.intensity.clone();
  for(std::size_t j=0;j<points.size();++j){
    if(u[j]<0||v[j]<0)continue;
    const std::size_t start=(static_cast<std::size_t>(v[j])*cols+u[j])*10;
    const int offset=frame.projected_index[start]+1;
    if(offset>=10)continue;
    frame.projected_index[start+offset]=static_cast<int>(j);
    frame.projected_index[start]=offset;
  }

  if(!settings_.reflectivity)frame.intensity*=settings_.intensity_scale;
  if(settings_.line_removal){
    cv::Mat im_hpf,im_lpf;
    cv::Mat hpf(settings_.highpass),lpf(settings_.lowpass);
    cv::filter2D(frame.intensity,im_hpf,CV_32F,hpf);
    cv::filter2D(im_hpf,im_lpf,CV_32F,lpf.t());
    frame.intensity-=im_lpf;frame.intensity.setTo(0,frame.intensity<0);
  }
  if(settings_.brightness_filter){
    cv::Mat brightness,normalized;
    cv::blur(frame.intensity,brightness,settings_.brightness_window);
    brightness+=1;normalized=140.*frame.intensity/brightness;frame.intensity=normalized;
  }
  if(settings_.blur){cv::Mat smoothed;cv::GaussianBlur(frame.intensity,smoothed,cv::Size(3,3),0);frame.intensity=smoothed;}
  cv::threshold(frame.intensity,frame.intensity,255.,255.,cv::THRESH_TRUNC);
  frame.intensity.convertTo(frame.photo_u8,CV_8UC1,1);
  cv::Mat kernel_dx=cv::Mat::zeros(1,3,CV_32F),kernel_dy=cv::Mat::zeros(3,1,CV_32F);
  kernel_dx.at<float>(0,0)=-.5f;kernel_dx.at<float>(0,2)=.5f;
  kernel_dy.at<float>(0,0)=-.5f;kernel_dy.at<float>(2,0)=.5f;
  cv::filter2D(frame.intensity,frame.dx,CV_32F,kernel_dx);
  cv::filter2D(frame.intensity,frame.dy,CV_32F,kernel_dy);
  frame.mask=cv::Mat::ones(rows,cols,CV_8UC1);
  for(const auto& r:settings_.masks)frame.mask(r)=0;
  for(int row=0;row<rows;++row)for(int col=0;col<cols;++col){
    const float range=frame.range.ptr<float>(row)[col];
    if(range<settings_.min_range||range>settings_.max_range)frame.mask.ptr<uchar>(row)[col]=0;
  }
  const int erosion=settings_.patch_size+settings_.erosion_margin;
  cv::Mat eroded;cv::erode(frame.mask,eroded,cv::Mat::ones(erosion,erosion,CV_32FC1));frame.mask=eroded;
  return frame;
}
} // namespace cube::coin
