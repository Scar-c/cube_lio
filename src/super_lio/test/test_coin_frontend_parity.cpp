// Standalone official-vs-Super COIN frontend parity runner.
// Build only when the exact upstream oracle checkout and binary are available.
#include "intensity/coin/coin_image_processor.hpp"
#include "common_lib.h"
#include "feature_manager.h"
#include "image_processing.h"
#include "preprocess.h"
#include "projector.h"

#include <ros/ros.h>
#include <rosbag/bag.h>
#include <rosbag/view.h>
#include <opencv2/core.hpp>
#include <Eigen/Core>
#include <set>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>

namespace {
double maxAbs(const cv::Mat& a,const cv::Mat& b){
  cv::Mat d;cv::absdiff(a,b,d);cv::Mat flat=d.reshape(1);double lo=0,hi=0;cv::minMaxLoc(flat,&lo,&hi);return hi;
}
int mismatch(const cv::Mat& a,const cv::Mat& b){cv::Mat neq;cv::compare(a,b,neq,cv::CMP_NE);return cv::countNonZero(neq.reshape(1));}
void stats(const cv::Mat& m,double& mean,double& stddev,double& minimum,double& maximum){
  cv::Scalar mu,sd;cv::meanStdDev(m,mu,sd);mean=mu[0];stddev=sd[0];cv::minMaxLoc(m,&minimum,&maximum);
}
}

int main(int argc,char** argv){
  ros::init(argc,argv,"p2_coin_frontend_parity",ros::init_options::AnonymousName);
  if(argc!=3){std::cerr<<"usage: test_coin_frontend_parity TUNNELD.bag OUTPUT.csv\n";return 2;}
  try{
    ros::NodeHandle nh;
    auto official_projector=std::make_shared<Projector>(nh);
    auto manager=std::make_shared<FeatureManager>(nh,official_projector);
    ImageProcessor official_image(nh,official_projector,manager);
    cube::coin::CoinOusterProjector local_projector(cube::coin::OusterMetadata::fromRosParams());
    cube::coin::CoinImageProcessor local_image(local_projector,cube::coin::CoinImageSettings::fromRosParams());
    Preprocess preprocess;preprocess.lidar_type=OUSTER;nh.param("preprocess/blind",preprocess.blind,.65);
    preprocess.reflectivity=false;std::vector<double> lidar_to_sensor;
    if(nh.getParam("/lidar_intrinsics/lidar_to_sensor_transform",lidar_to_sensor)&&lidar_to_sensor.size()==16)
      preprocess.lidar_sensor_z_offset=lidar_to_sensor[11]*.001;
    else preprocess.lidar_sensor_z_offset=.03618;

    const std::set<int> wanted{0,132,264,396,528,660,792,924,1056,1188};
    rosbag::Bag bag;bag.open(argv[1],rosbag::bagmode::Read);
    rosbag::View view(bag,rosbag::TopicQuery(std::vector<std::string>{"/ouster/points"}));
    std::ofstream out(argv[2]);if(!out)throw std::runtime_error("cannot open parity CSV");
    out<<std::setprecision(17);
    out<<"frame,timestamp,point_count,rows,cols,valid_intensity_pixels,valid_range_pixels,valid_mask_pixels,raw_mean,raw_std,raw_min,raw_max,raw_max_abs,raw_exact_mismatches,filtered_mean,filtered_std,filtered_min,filtered_max,filtered_max_abs,filtered_exact_mismatches,dx_mean,dx_std,dx_min,dx_max,dx_max_abs,dx_exact_mismatches,dy_mean,dy_std,dy_min,dy_max,dy_max_abs,dy_exact_mismatches,mask_mismatches,range_mismatches,owner_mismatches,photo_u8_mismatches,proj_index_mismatches,projected_samples,project_valid_mismatches,project_max_uv_error,pass\n";
    int frame_index=0,selected=0;bool all_pass=true;
    for(const auto& msg:view){
      if(!wanted.count(frame_index)){++frame_index;continue;}
      const auto cloud_msg=msg.instantiate<sensor_msgs::PointCloud2>();
      if(!cloud_msg)throw std::runtime_error("TunnelD selected message is not PointCloud2");
      PointCloudXYZI::Ptr points(new PointCloudXYZI);preprocess.process(cloud_msg,points);
      LidarFrame frame;frame.points_corrected=points;frame.T_Li_Lk_vec={M4D::Identity()};frame.vec_idx.assign(points->size(),0);
      official_projector->createImages(frame);
      double raw_mean=0,raw_std=0,raw_min=0,raw_max=0;stats(frame.img_intensity,raw_mean,raw_std,raw_min,raw_max);
      const cv::Mat official_raw_intensity=frame.img_intensity.clone();
      official_image.createImages(frame);
      std::vector<cube::coin::CoinScanPoint> local_points;local_points.reserve(points->size());
      for(const auto& p:points->points){
        cube::coin::CoinScanPoint q;q.point_lidar=p.getVector3fMap().cast<double>();q.intensity=p.intensity;
        q.range=p.normal_y;q.raw_index=static_cast<std::size_t>(p.normal_x);q.offset_seconds=p.curvature*1e-3;
        local_points.push_back(q);
      }
      auto image=local_image.process(local_points);
      if(frame.img_intensity.rows!=image.intensity.rows||frame.img_intensity.cols!=image.intensity.cols)
        throw std::runtime_error("COIN frontend image dimension mismatch");
      int uv_count=0,uv_bad=0;double uv_error=0;
      const std::size_t stride=std::max<std::size_t>(1,points->size()/2048);
      for(std::size_t i=0;i<points->size();i+=stride){
        V2D a;const V3D p=points->points[i].getVector3fMap().cast<double>();bool va=official_projector->projectPoint(p,a);
        const auto b=local_projector.project(p);++uv_count;if(va!=b.in_fov)++uv_bad;
        if(va&&b.in_fov)uv_error=std::max(uv_error,(a-b.uv).cwiseAbs().maxCoeff());
      }
      double im=0,is=0,imin=0,imax=0;stats(frame.img_intensity,im,is,imin,imax);
      double dx_mean=0,dx_std=0,dx_min=0,dx_max=0,dy_mean=0,dy_std=0,dy_min=0,dy_max=0;
      stats(frame.img_dx,dx_mean,dx_std,dx_min,dx_max);stats(frame.img_dy,dy_mean,dy_std,dy_min,dy_max);
      const int valid_i=cv::countNonZero(frame.img_idx>=0),valid_r=cv::countNonZero(frame.img_range>0);
      const int valid_mask=cv::countNonZero(frame.img_mask);
      if(frame.proj_idx.size()!=image.projected_index.size())throw std::runtime_error("COIN projected index map size differs");
      int proj_bad=0;for(std::size_t i=0;i<frame.proj_idx.size();++i)proj_bad+=frame.proj_idx[i]!=image.projected_index[i];
      const double raw_error=maxAbs(official_raw_intensity,image.raw_intensity);
      const int raw_bad=mismatch(official_raw_intensity,image.raw_intensity),filtered_bad=mismatch(frame.img_intensity,image.intensity);
      const int dx_bad=mismatch(frame.img_dx,image.dx),dy_bad=mismatch(frame.img_dy,image.dy);
      const int mask_bad=mismatch(frame.img_mask,image.mask),range_bad=mismatch(frame.img_range,image.range),owner_bad=mismatch(frame.img_idx,image.image_index);
      const int u8_bad=mismatch(frame.img_photo_u8,image.photo_u8);
      const double filtered_error=maxAbs(frame.img_intensity,image.intensity),dx_error=maxAbs(frame.img_dx,image.dx),dy_error=maxAbs(frame.img_dy,image.dy);
      const bool pass=raw_bad==0&&filtered_bad==0&&dx_bad==0&&dy_bad==0&&mask_bad==0&&range_bad==0&&owner_bad==0&&u8_bad==0&&proj_bad==0&&uv_bad==0&&
                      raw_error<=1e-6&&filtered_error<=1e-6&&dx_error<=1e-6&&dy_error<=1e-6&&uv_error<=1e-12&&valid_i==cv::countNonZero(image.image_index>=0)&&
                      valid_r==cv::countNonZero(image.range>0)&&valid_mask==cv::countNonZero(image.mask);
      all_pass=all_pass&&pass;
      out<<frame_index<<','<<cloud_msg->header.stamp.toSec()<<','<<points->size()<<','<<frame.img_intensity.rows<<','<<frame.img_intensity.cols<<','
        <<valid_i<<','<<valid_r<<','<<valid_mask<<','<<raw_mean<<','<<raw_std<<','<<raw_min<<','<<raw_max<<','<<raw_error<<','<<raw_bad<<','
        <<im<<','<<is<<','<<imin<<','<<imax<<','<<filtered_error<<','<<filtered_bad<<','
        <<dx_mean<<','<<dx_std<<','<<dx_min<<','<<dx_max<<','<<dx_error<<','<<dx_bad<<','
        <<dy_mean<<','<<dy_std<<','<<dy_min<<','<<dy_max<<','<<dy_error<<','<<dy_bad<<','
        <<mask_bad<<','<<range_bad<<','<<owner_bad<<','<<u8_bad<<','<<proj_bad<<','<<uv_count<<','<<uv_bad<<','<<uv_error<<','<<(pass?"PASS":"FAIL")<<'\n';
      ++selected;++frame_index;
    }
    bag.close();out.close();
    if(selected!=static_cast<int>(wanted.size()))throw std::runtime_error("one or more requested representative scans were absent");
    if(!all_pass)throw std::runtime_error("official-vs-Super frontend parity failed; inspect the per-frame CSV metrics");
    std::cout<<"processed "<<selected<<" TunnelD frames into "<<argv[2]<<'\n';
    return 0;
  }catch(const std::exception& e){std::cerr<<"P2 COIN frontend parity: "<<e.what()<<'\n';return 1;}
}
