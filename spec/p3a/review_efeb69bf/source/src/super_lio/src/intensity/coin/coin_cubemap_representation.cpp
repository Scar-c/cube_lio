#include "intensity/coin/coin_cubemap_representation.hpp"
#include <ros/ros.h>
#include <opencv2/imgproc.hpp>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace cube::coin {

std::string p3aRepresentation(){
  const char* value=std::getenv("CUBE_P3A_REPRESENTATION");
  const std::string mode=value?value:"coin";
  if(mode!="coin" && mode!="cube_raw_no_idw" && mode!="cube_raw_idw" && mode!="cube_igm_idw")
    throw std::invalid_argument("unknown CUBE_P3A_REPRESENTATION: "+mode);
  return mode;
}

Settings p3aCubeSettings(const std::string& representation){
  Settings cfg;ros::NodeHandle nh;
  nh.param("/cubemap/resolution",cfg.resolution,96);
  nh.param("/cubemap/idw_radius",cfg.idw_radius,3);
  nh.param("/cubemap/idw_k",cfg.idw_k,6);
  nh.param("/cubemap/idw_min_support",cfg.idw_min_support,3);
  nh.param("/cubemap/idw_power",cfg.idw_power,2.);
  nh.param("/cubemap/range_absolute",cfg.range_absolute,.3);
  nh.param("/cubemap/range_relative",cfg.range_relative,.02);
  nh.param("/cubemap/gaussian_sigma",cfg.gaussian_sigma,1.);
  cfg.idw_enable=representation!="cube_raw_no_idw";
  cfg.build_igm=representation=="cube_igm_idw";
  cfg.validate();return cfg;
}

namespace {
Vec3 pixelRay(int face,int u,int v,int n){
  const double x=2.*u/(n-1)-1.,y=2.*v/(n-1)-1.;
  const int axis=face/2;const double sign=face%2?-1.:1.;
  Vec3 p=Vec3::Zero();p[axis]=sign;
  if(axis==0){p.y()=sign*x;p.z()=y;}
  if(axis==1){p.x()=-sign*x;p.z()=y;}
  if(axis==2){p.x()=x;p.y()=sign*y;}
  return p.normalized();
}
}

CoinFrame buildCoinCubemap(CubeImage& image,const CoinFrame& calibrated,
    const CoinOusterProjector& calibrated_projector,const CoinImageSettings& settings,
    std::vector<CoinScanPoint>& points){
  std::vector<RepresentationPoint> input;input.reserve(points.size());
  // COIN's acquisition-pixel intensity preprocessing is common to all arms.
  // The point coordinates have already passed the identical support/deskew path.
  for(const auto& point:points){
    int row,col;calibrated_projector.pixelFromRawIndex(point.raw_index,row,col);
    bool masked=false;
    for(const auto& mask:settings.masks)if(mask.contains(cv::Point(col,row))){masked=true;break;}
    input.push_back({point.point_lidar,masked?std::numeric_limits<double>::quiet_NaN():
                     calibrated.intensity.ptr<float>(row)[col]});
  }
  image.build(input);
  const int n=image.coordinateResolution();
  CoinFrame frame;
  frame.intensity=cv::Mat::zeros(n,6*n,CV_32F);
  frame.range=cv::Mat::zeros(n,6*n,CV_32F);
  frame.mask=cv::Mat::zeros(n,6*n,CV_8U);
  frame.image_index=cv::Mat::ones(n,6*n,CV_32S)*(-1);
  frame.dx=cv::Mat::zeros(n,6*n,CV_32F);frame.dy=frame.dx.clone();
  for(int face=0;face<6;++face){
    const auto& source=image.face(face);const cv::Rect roi(face*n,0,n,n);
    const bool igm=!source.igm.empty();
    (igm?source.igm:source.intensity).convertTo(frame.intensity(roi),CV_32F);
    source.depth.convertTo(frame.range(roi),CV_32F);
    cv::Mat mask=(igm?source.igm_mask:source.mask).clone();
    for(int v=0;v<n;++v)for(int u=0;u<n;++u){
      if(!mask.at<uchar>(v,u))continue;
      const double range=source.depth.at<double>(v,u);
      if(range<settings.min_range || range>settings.max_range){mask.at<uchar>(v,u)=0;continue;}
      int index=source.point_index[v*n+u];
      if(index<0){
        CoinScanPoint point;point.point_lidar=range*pixelRay(face,u,v,n);
        point.range=range;point.intensity=frame.intensity.at<float>(v,face*n+u);
        index=static_cast<int>(points.size());points.push_back(point);
      }
      frame.image_index.at<int>(v,face*n+u)=index;
    }
    // The original COIN patch/erosion settings also apply at each chart border.
    const int erosion=settings.patch_size+settings.erosion_margin;
    cv::erode(mask,frame.mask(roi),cv::Mat::ones(erosion,erosion,CV_32F),
              cv::Point(-1,-1),1,cv::BORDER_CONSTANT,cv::Scalar(0));
    cv::Mat kernel_dx=(cv::Mat_<float>(1,3)<<-.5f,0.f,.5f);
    cv::Mat kernel_dy=kernel_dx.t();
    cv::filter2D(frame.intensity(roi),frame.dx(roi),CV_32F,kernel_dx,cv::Point(-1,-1),0,cv::BORDER_CONSTANT);
    cv::filter2D(frame.intensity(roi),frame.dy(roi),CV_32F,kernel_dy,cv::Point(-1,-1),0,cv::BORDER_CONSTANT);
  }
  frame.raw_intensity=frame.intensity.clone();frame.intensity.convertTo(frame.photo_u8,CV_8U);
  frame.T_Li_Lk_vec.push_back(Eigen::Matrix4d::Identity());
  // vec_idx all-zero: frame images and landmarks are both at scan end.
  frame.vec_idx.assign(points.size(),0);
  return frame;
}

} // namespace cube::coin
