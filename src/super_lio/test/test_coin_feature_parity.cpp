// Shadow comparison against the pinned official COIN-LIO feature manager.
#include "intensity/coin/coin_feature_manager.hpp"
#include "intensity/coin/coin_photometric_model.hpp"
#include "common_lib.h"
#include "feature_manager.h"
#include "image_processing.h"
#include "preprocess.h"
#include "projector.h"

#include <ros/ros.h>
#include <rosbag/bag.h>
#include <rosbag/view.h>
#include <Eigen/Geometry>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

namespace {
struct Pose { double stamp=0;Eigen::Matrix4d T_GI=Eigen::Matrix4d::Identity(); };

struct MotionTraceMeta {
  std::uint32_t frame=0,point_count=0,transform_count=0;
  std::streamoff payload_offset=0;
};
struct MotionTraceFrame {
  std::vector<Eigen::Matrix4d> transforms;
  std::vector<int> vec_idx;
};
struct WeakTraceFrame {
  std::uint32_t frame=0;
  std::vector<Eigen::Vector3d> global,lidar;
};
struct GeometryTraceFrame {
  std::uint32_t frame=0;
  Eigen::MatrixXd translation_rows;
};

std::vector<MotionTraceMeta> scanMotionTrace(const std::string& path){
  std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("cannot open official COIN motion trace");
  std::vector<MotionTraceMeta> frames;
  while(true){
    MotionTraceMeta f;
    if(!in.read(reinterpret_cast<char*>(&f.frame),sizeof(f.frame))){if(in.eof())break;throw std::runtime_error("failed reading motion trace frame id");}
    if(!in.read(reinterpret_cast<char*>(&f.point_count),sizeof(f.point_count))||
       !in.read(reinterpret_cast<char*>(&f.transform_count),sizeof(f.transform_count)))
      throw std::runtime_error("truncated motion trace header");
    f.payload_offset=in.tellg();
    const std::uint64_t payload=static_cast<std::uint64_t>(f.transform_count)*16*sizeof(double)+
                                static_cast<std::uint64_t>(f.point_count)*sizeof(int);
    in.seekg(static_cast<std::streamoff>(payload),std::ios::cur);
    if(!in)throw std::runtime_error("truncated motion trace payload");
    if(f.frame!=frames.size()||f.point_count==0||f.transform_count==0)
      throw std::runtime_error("invalid motion trace frame sequence");
    frames.push_back(f);
  }
  if(frames.empty())throw std::runtime_error("official COIN motion trace is empty");
  return frames;
}

MotionTraceFrame readMotionTrace(const std::string& path,const MotionTraceMeta& meta){
  std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("cannot reopen official COIN motion trace");
  in.seekg(meta.payload_offset);
  MotionTraceFrame f;f.transforms.resize(meta.transform_count);
  for(auto& T:f.transforms)if(!in.read(reinterpret_cast<char*>(T.data()),16*sizeof(double)))
    throw std::runtime_error("truncated motion transform table");
  f.vec_idx.resize(meta.point_count);
  if(!in.read(reinterpret_cast<char*>(f.vec_idx.data()),static_cast<std::streamsize>(f.vec_idx.size()*sizeof(int))))
    throw std::runtime_error("truncated motion transform index table");
  for(int idx:f.vec_idx)if(idx<0||static_cast<std::size_t>(idx)>=f.transforms.size())
    throw std::runtime_error("official COIN motion trace contains an invalid transform index");
  return f;
}

std::vector<WeakTraceFrame> loadWeakTrace(const std::string& path){
  std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("cannot open official COIN weak-direction trace");
  std::vector<WeakTraceFrame> frames;
  while(true){
    std::uint32_t id=0,ng=0,nl=0;
    if(!in.read(reinterpret_cast<char*>(&id),sizeof(id))){if(in.eof())break;throw std::runtime_error("failed reading weak trace frame id");}
    if(!in.read(reinterpret_cast<char*>(&ng),sizeof(ng))||!in.read(reinterpret_cast<char*>(&nl),sizeof(nl)))
      throw std::runtime_error("truncated weak trace header");
    WeakTraceFrame frame;frame.frame=id;frame.global.resize(ng);frame.lidar.resize(nl);
    for(auto& v:frame.global)if(!in.read(reinterpret_cast<char*>(v.data()),3*sizeof(double)))throw std::runtime_error("truncated global weak directions");
    for(auto& v:frame.lidar)if(!in.read(reinterpret_cast<char*>(v.data()),3*sizeof(double)))throw std::runtime_error("truncated LiDAR weak directions");
    if(frame.frame!=frames.size())throw std::runtime_error("invalid weak trace frame sequence");
    frames.push_back(std::move(frame));
  }
  if(frames.empty())throw std::runtime_error("official COIN weak-direction trace is empty");
  return frames;
}

std::vector<GeometryTraceFrame> loadGeometryTrace(const std::string& path){
  std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("cannot open official COIN geometry trace");
  std::vector<GeometryTraceFrame> frames;
  while(true){
    std::uint32_t id=0,rows=0,cols=0;
    if(!in.read(reinterpret_cast<char*>(&id),sizeof(id))){if(in.eof())break;throw std::runtime_error("failed reading geometry trace frame id");}
    if(!in.read(reinterpret_cast<char*>(&rows),sizeof(rows))||!in.read(reinterpret_cast<char*>(&cols),sizeof(cols)))
      throw std::runtime_error("truncated geometry trace header");
    if((cols!=0&&cols!=3)||(rows>0&&cols!=3))throw std::runtime_error("geometry trace must contain the three translation columns");
    GeometryTraceFrame frame;frame.frame=id;frame.translation_rows=Eigen::MatrixXd::Zero(rows,3);
    for(std::uint32_t row=0;row<rows;++row)for(std::uint32_t col=0;col<cols;++col){
      double value=0;if(!in.read(reinterpret_cast<char*>(&value),sizeof(value)))throw std::runtime_error("truncated geometry translation rows");
      frame.translation_rows(row,col)=value;
    }
    if(frame.frame!=frames.size())throw std::runtime_error("invalid geometry trace frame sequence");
    frames.push_back(std::move(frame));
  }
  if(frames.empty())throw std::runtime_error("official COIN geometry trace is empty");
  return frames;
}

std::vector<Pose> loadTrajectory(const std::string& path){
  std::ifstream in(path);if(!in)throw std::runtime_error("cannot open frozen official trajectory");
  std::vector<Pose> poses;std::string line;
  while(std::getline(in,line)){
    if(line.empty()||line[0]=='#')continue;
    std::istringstream row(line);double tx,ty,tz,qx,qy,qz,qw;Pose pose;
    if(!(row>>pose.stamp>>tx>>ty>>tz>>qx>>qy>>qz>>qw))throw std::runtime_error("malformed TUM trajectory row");
    Eigen::Quaterniond q(qw,qx,qy,qz);q.normalize();
    pose.T_GI.topLeftCorner<3,3>()=q.toRotationMatrix();pose.T_GI.topRightCorner<3,1>()=Eigen::Vector3d(tx,ty,tz);
    poses.push_back(pose);
  }
  if(poses.empty())throw std::runtime_error("frozen official trajectory is empty");
  return poses;
}

struct BagScanMeta { std::uint32_t point_count=0;double header_time=0,end_time=0; };

std::vector<int> pairTrajectoryToBag(const std::vector<Pose>& poses,const std::vector<BagScanMeta>& scans){
  std::vector<int> pose_to_bag(poses.size(),-1);std::vector<bool> used(scans.size(),false);
  for(std::size_t pose=0;pose<poses.size();++pose){
    int best=-1;double best_delta=std::numeric_limits<double>::infinity();
    const auto it=std::lower_bound(scans.begin(),scans.end(),poses[pose].stamp,
      [](const BagScanMeta& scan,double stamp){return scan.end_time<stamp;});
    const std::size_t pivot=static_cast<std::size_t>(it-scans.begin());
    const std::size_t begin=pivot>2?pivot-2:0,end=std::min(scans.size(),pivot+3);
    for(std::size_t i=begin;i<end;++i){
      const double delta=std::abs(scans[i].end_time-poses[pose].stamp);
      if(delta<best_delta){best=static_cast<int>(i);best_delta=delta;}
    }
    if(best<0||best_delta>1e-3)
      throw std::runtime_error("official trajectory row has no bag scan with matching Ouster lidar_end_time");
    if(used[static_cast<std::size_t>(best)])
      throw std::runtime_error("two official trajectory rows paired to the same bag scan");
    pose_to_bag[pose]=best;used[static_cast<std::size_t>(best)]=true;
    if(pose>0&&pose_to_bag[pose]<=pose_to_bag[pose-1])
      throw std::runtime_error("trajectory-to-bag scan pairing is not strictly chronological");
  }
  return pose_to_bag;
}

bool sameBirth(const Feature& a,const Feature& b){
  if(a.p.size()!=b.p.size()||a.intensities.size()!=b.intensities.size())return false;
  for(std::size_t i=0;i<a.p.size();++i)
    if((a.p[i]-b.p[i]).squaredNorm()!=0||a.intensities[i]!=b.intensities[i])return false;
  return true;
}

int newlyAdded(const std::vector<Feature>& before,const std::vector<Feature>& after){
  int added=0;
  for(const auto& feature:after){
    const auto it=std::find_if(before.begin(),before.end(),[&](const Feature& old){return sameBirth(old,feature);});
    if(it==before.end())++added;
  }
  return added;
}

double quantile(std::vector<double> values,double q){
  values.erase(std::remove_if(values.begin(),values.end(),[](double x){return !std::isfinite(x);}),values.end());
  if(values.empty())return std::numeric_limits<double>::quiet_NaN();
  std::sort(values.begin(),values.end());
  const auto i=static_cast<std::size_t>(std::floor(q*(values.size()-1)));
  return values[i];
}

struct CompareResult { int mismatch=0;double center=0,uv=0,world=0,reference=0; };
CompareResult compareFeatures(const std::vector<Feature>& official,const std::vector<cube::coin::CoinFeature>& local){
  CompareResult result;if(official.size()!=local.size())++result.mismatch;
  const std::size_t count=std::min(official.size(),local.size());
  for(std::size_t i=0;i<count;++i){
    const auto& a=official[i];const auto& b=local[i];
    if(a.life_time!=b.lifetime||a.p.size()!=b.points_global.size()||a.intensities.size()!=b.reference_intensity.size()||a.uv.size()!=b.current_uv.size())++result.mismatch;
    result.center=std::max(result.center,(a.center-b.center).cwiseAbs().maxCoeff());
    const std::size_t pn=std::min(a.p.size(),b.points_global.size());
    for(std::size_t j=0;j<pn;++j)result.world=std::max(result.world,(a.p[j]-b.points_global[j]).cwiseAbs().maxCoeff());
    const std::size_t rn=std::min(a.intensities.size(),b.reference_intensity.size());
    for(std::size_t j=0;j<rn;++j)result.reference=std::max(result.reference,std::abs(a.intensities[j]-b.reference_intensity[j]));
    const std::size_t un=std::min(a.uv.size(),b.current_uv.size());
    for(std::size_t j=0;j<un;++j)result.uv=std::max(result.uv,(a.uv[j]-b.current_uv[j]).cwiseAbs().maxCoeff());
  }
  return result;
}

std::string lifetimeHistogram(const std::vector<Feature>& features){
  std::map<int,int> bins;for(const auto& f:features)++bins[f.life_time];
  std::ostringstream out;bool first=true;for(const auto& [life,count]:bins){if(!first)out<<';';first=false;out<<life<<':'<<count;}
  return out.str();
}
}

int main(int argc,char** argv){
  ros::init(argc,argv,"p2_coin_feature_parity",ros::init_options::AnonymousName);
  if(argc!=8){std::cerr<<"usage: test_coin_feature_parity TUNNELD.bag OFFICIAL_TRAJECTORY.tum COIN_MOTION_TRACE.bin COIN_WEAK_TRACE.bin COIN_GEOMETRY_TRACE.bin OUTPUT.csv JACOBIAN_FD.csv\n";return 2;}
  try{
    const auto poses=loadTrajectory(argv[2]);ros::NodeHandle nh;
    const std::string motion_trace_path=argv[3];
    const auto motion_trace=scanMotionTrace(motion_trace_path);
    const auto weak_trace=loadWeakTrace(argv[4]);
    const auto geometry_trace=loadGeometryTrace(argv[5]);
    if(weak_trace.size()!=poses.size())throw std::runtime_error("official weak-direction trace and frozen trajectory frame counts differ");
    if(geometry_trace.size()!=weak_trace.size())throw std::runtime_error("official geometry trace and weak-direction trace frame counts differ");
    auto official_projector=std::make_shared<Projector>(nh);
    auto official_manager=std::make_shared<FeatureManager>(nh,official_projector);
    ImageProcessor official_image(nh,official_projector,official_manager);
    cube::coin::CoinOusterProjector local_projector(cube::coin::OusterMetadata::fromRosParams());
    cube::coin::CoinImageProcessor local_image(local_projector,cube::coin::CoinImageSettings::fromRosParams());
    const auto feature_settings=cube::coin::CoinFeatureSettings::fromRosParams();
    cube::coin::CoinFeatureManager local_manager(local_projector,feature_settings);

    std::vector<double> extrinsic_t,extrinsic_r;
    if(!nh.getParam("mapping/extrinsic_T",extrinsic_t)||extrinsic_t.size()!=3||
       !nh.getParam("mapping/extrinsic_R",extrinsic_r)||extrinsic_r.size()!=9)
      throw std::runtime_error("official ENWIDE mapping extrinsic parameters are missing");
    Eigen::Matrix4d T_IL=Eigen::Matrix4d::Identity();
    for(int r=0;r<3;++r)for(int c=0;c<3;++c)T_IL(r,c)=extrinsic_r[r*3+c];
    for(int r=0;r<3;++r)T_IL(r,3)=extrinsic_t[r];

    Preprocess preprocess;preprocess.lidar_type=OUSTER;nh.param("preprocess/blind",preprocess.blind,.65);
    preprocess.reflectivity=false;std::vector<double> lidar_to_sensor;
    if(nh.getParam("/lidar_intrinsics/lidar_to_sensor_transform",lidar_to_sensor)&&lidar_to_sensor.size()==16)
      preprocess.lidar_sensor_z_offset=lidar_to_sensor[11]*.001;
    else preprocess.lidar_sensor_z_offset=.03618;

    std::vector<BagScanMeta> bag_scans;
    {
      rosbag::Bag count_bag;count_bag.open(argv[1],rosbag::bagmode::Read);
      rosbag::View count_view(count_bag,rosbag::TopicQuery(std::vector<std::string>{"/ouster/points"}));
      bag_scans.reserve(count_view.size());
      for(const auto& msg:count_view){
        const auto cloud_msg=msg.instantiate<sensor_msgs::PointCloud2>();
        if(!cloud_msg)throw std::runtime_error("TunnelD input contains a non-PointCloud2 message");
        PointCloudXYZI::Ptr cloud(new PointCloudXYZI);preprocess.process(cloud_msg,cloud);
        BagScanMeta scan;scan.point_count=static_cast<std::uint32_t>(cloud->size());
        scan.header_time=cloud_msg->header.stamp.toSec();
        scan.end_time=scan.header_time+cloud->header.stamp*1e-9;
        bag_scans.push_back(scan);
      }
      count_bag.close();
    }
    if(motion_trace.size()!=poses.size()+1)
      throw std::runtime_error("expected one pre-feature map-initialization motion trace frame");
    const auto pose_to_bag=pairTrajectoryToBag(poses,bag_scans);
    std::vector<int> bag_to_pose(bag_scans.size(),-1);
    for(std::size_t pose_index=0;pose_index<poses.size();++pose_index){
      const std::size_t expected_motion=pose_index+1;
      const int bag_index=pose_to_bag[pose_index];
      if(bag_index<0||static_cast<std::size_t>(bag_index)>=bag_scans.size())throw std::runtime_error("invalid trajectory-to-bag mapping");
      bag_to_pose[static_cast<std::size_t>(bag_index)]=static_cast<int>(pose_index);
      if(motion_trace[expected_motion].point_count!=bag_scans[static_cast<std::size_t>(bag_index)].point_count){
        std::ostringstream message;message<<"official motion/trajectory/bag association differs at pose "<<pose_index
          <<": trace points="<<motion_trace[expected_motion].point_count
          <<", bag points="<<bag_scans[static_cast<std::size_t>(bag_index)].point_count
          <<", lidar_end_delta="<<std::abs(poses[pose_index].stamp-bag_scans[static_cast<std::size_t>(bag_index)].end_time);
        throw std::runtime_error(message.str());
      }
    }

    std::ofstream out(argv[6]);if(!out)throw std::runtime_error("cannot open feature parity CSV");
    std::ofstream jacobian_out(argv[7]);if(!jacobian_out)throw std::runtime_error("cannot open photometric Jacobian FD CSV");
    out<<std::setprecision(17);
    jacobian_out<<std::setprecision(17);
    out<<"bag_frame,pose_index,motion_frame,timestamp,header_delta_s,lidar_end_delta_s,point_count,weak_global_count,weak_lidar_count,weak_direction_transform_max_abs,weak_geometry_global_max_abs,weak_geometry_lidar_max_abs,official_active,local_active,official_added,local_added,official_removed,local_removed,candidates_after_nms,selected_centers,projection_rejects,border_rejects,mask_rejects,range_rejects,ncc_rejects,lifetime_rejects,ncc_count,ncc_min,ncc_median,ncc_p95,ncc_max,life_histogram,state_mismatches,center_max_abs,uv_max_abs,world_patch_max_abs,reference_max_abs,pass\n";
    jacobian_out<<"pose_index,feature_index,patch_index,distortion_index,u,v,dof,epsilon,analytic_h,numeric_h,absolute_error,relative_error,official_source_h_error,projector_relative_error_x,projector_relative_error_y,image_gradient_relative_error_x,image_gradient_relative_error_y,sign_agreement\n";

    rosbag::Bag bag;bag.open(argv[1],rosbag::bagmode::Read);
    rosbag::View view(bag,rosbag::TopicQuery(std::vector<std::string>{"/ouster/points"}));
    int bag_frame=0,matched=0,unmatched=0,total_mismatch=0;double max_header_delta=0,max_end_delta=0;
    std::vector<double> jacobian_relative_errors,projector_relative_errors,image_gradient_relative_errors;
    int jacobian_sign_disagreements=0,jacobian_samples=0,official_jacobian_mismatches=0;
    double max_official_jacobian_error=0;
    for(const auto& msg:view){
      const int current_bag_frame=bag_frame++;
      const auto cloud_msg=msg.instantiate<sensor_msgs::PointCloud2>();
      if(!cloud_msg)throw std::runtime_error("TunnelD input contains a non-PointCloud2 message");
      const int paired_pose=bag_to_pose.at(static_cast<std::size_t>(current_bag_frame));
      if(paired_pose<0){++unmatched;continue;}
      const std::size_t pose_index=static_cast<std::size_t>(paired_pose);
      PointCloudXYZI::Ptr cloud(new PointCloudXYZI);preprocess.process(cloud_msg,cloud);
      if(cloud->empty())throw std::runtime_error("official COIN preprocessing returned an empty matched scan");
      const int motion_index=static_cast<int>(pose_index+1);
      const MotionTraceMeta& motion_meta=motion_trace[static_cast<std::size_t>(motion_index)];
      if(motion_meta.point_count!=cloud->size())throw std::runtime_error("motion trace point count differs from the preprocessed bag scan");
      const double lidar_end_time=cloud_msg->header.stamp.toSec()+cloud->header.stamp*1e-9;
      const double header_delta=std::abs(poses[pose_index].stamp-cloud_msg->header.stamp.toSec());
      const double end_delta=std::abs(poses[pose_index].stamp-lidar_end_time);
      max_header_delta=std::max(max_header_delta,header_delta);max_end_delta=std::max(max_end_delta,end_delta);
      const MotionTraceFrame motion=readMotionTrace(motion_trace_path,motion_meta);
      std::vector<Eigen::Matrix4d> T_Lk_Li;T_Lk_Li.reserve(motion.transforms.size());
      for(const auto& T_Li_Lk:motion.transforms)T_Lk_Li.push_back(T_Li_Lk.inverse());
      for(std::size_t i=0;i<cloud->size();++i){
        const Eigen::Matrix4d& T_Lk_Li_i=T_Lk_Li[static_cast<std::size_t>(motion.vec_idx[i])];
        const Eigen::Vector3d p_raw=cloud->points[i].getVector3fMap().cast<double>();
        cloud->points[i].getVector3fMap()=(T_Lk_Li_i.topLeftCorner<3,3>()*p_raw+
          T_Lk_Li_i.topRightCorner<3,1>()).cast<float>();
      }
      LidarFrame frame;frame.header=cloud_msg->header;frame.points_corrected=cloud;
      frame.T_Li_Lk_vec=motion.transforms;frame.vec_idx=motion.vec_idx;
      official_image.createImages(frame);
      std::vector<cube::coin::CoinScanPoint> points;points.reserve(cloud->size());
      for(const auto& p:cloud->points){
        cube::coin::CoinScanPoint q;q.point_lidar=p.getVector3fMap().cast<double>();q.intensity=p.intensity;
        q.range=p.normal_y;q.raw_index=static_cast<std::size_t>(p.normal_x);q.offset_seconds=p.curvature*1e-3;
        points.push_back(q);
      }
      auto image=local_image.process(points);image.T_Li_Lk_vec=motion.transforms;image.vec_idx=motion.vec_idx;
      if(cv::countNonZero(frame.img_idx!=image.image_index)!=0||
         cv::countNonZero(frame.img_range!=image.range)!=0||cv::countNonZero(frame.img_intensity!=image.intensity)!=0)
        throw std::runtime_error("current-frame image frontend parity changed during feature shadow run");
      if(frame.proj_idx!=image.projected_index)
        throw std::runtime_error("current-frame motion projection index map differs from official COIN");

      Eigen::Matrix4d T_GL=poses[pose_index].T_GI*T_IL;
      const WeakTraceFrame& weak=weak_trace.at(pose_index);
      if(weak.frame!=pose_index)throw std::runtime_error("weak-direction trace frame does not match frozen trajectory order");
      std::vector<Eigen::Vector3d> expected_weak_lidar;
      if(weak.global.empty()){
        expected_weak_lidar={Eigen::Vector3d::UnitX(),Eigen::Vector3d::UnitY(),Eigen::Vector3d::UnitZ()};
      }else{
        for(const auto& direction:weak.global)
          expected_weak_lidar.push_back(T_GL.topLeftCorner<3,3>().transpose()*direction);
      }
      if(expected_weak_lidar.size()!=weak.lidar.size())throw std::runtime_error("official global/LiDAR weak-direction counts disagree");
      double weak_direction_transform_error=0;
      for(std::size_t d=0;d<weak.lidar.size();++d)
        weak_direction_transform_error=std::max(weak_direction_transform_error,
          (expected_weak_lidar[d]-weak.lidar[d]).cwiseAbs().maxCoeff());
      const GeometryTraceFrame& geometry=geometry_trace.at(pose_index);
      if(geometry.frame!=pose_index)throw std::runtime_error("geometry trace frame does not match frozen trajectory order");
      const auto local_weak=cube::coin::CoinFeatureManager::weakDirectionsFromGeometry(
        geometry.translation_rows,T_GL.topLeftCorner<3,3>(),feature_settings.n_uninformative);
      if(local_weak.global.size()!=weak.global.size()||local_weak.lidar.size()!=weak.lidar.size())
        throw std::runtime_error("local geometry weak-direction counts differ from official COIN");
      auto signInvariantDirectionError=[](const Eigen::Vector3d& a,const Eigen::Vector3d& b){
        return std::min((a-b).norm(),(a+b).norm());
      };
      double weak_geometry_global_error=0,weak_geometry_lidar_error=0;
      for(std::size_t d=0;d<weak.global.size();++d)
        weak_geometry_global_error=std::max(weak_geometry_global_error,
          signInvariantDirectionError(local_weak.global[d],weak.global[d]));
      for(std::size_t d=0;d<weak.lidar.size();++d)
        weak_geometry_lidar_error=std::max(weak_geometry_lidar_error,
          signInvariantDirectionError(local_weak.lidar[d],weak.lidar[d]));
      const std::vector<Eigen::Vector3d>& weak_directions=weak.lidar;
      const std::vector<Feature> old_official=official_manager->features();
      official_manager->updateFeatures(frame,weak_directions,T_GL);
      local_manager.update(image,points,weak_directions,T_GL);
      const auto& official=official_manager->features();const auto& local=local_manager.features();
      const int official_added=newlyAdded(old_official,official),local_added=local_manager.lastStats().added;
      const int official_removed=static_cast<int>(old_official.size())-static_cast<int>(official.size())+official_added;
      const int local_removed=local_manager.lastStats().removed;
      const CompareResult comparison=compareFeatures(official,local);
      const auto& stat=local_manager.lastStats();
      const bool pass=comparison.mismatch==0&&comparison.center<=1e-12&&comparison.uv<=1e-12&&
                      comparison.world<=1e-12&&comparison.reference==0&&official_added==local_added&&
                      official_removed==local_removed&&weak_direction_transform_error<=1e-6&&
                      weak_geometry_global_error<=1e-6&&weak_geometry_lidar_error<=1e-6;
      total_mismatch+=comparison.mismatch+(pass?0:1);

      if(pose_index%23==0&&!local.empty()){
        const std::vector<std::size_t> feature_ids{0,local.size()/2,local.size()-1};
        const std::vector<std::size_t> patch_ids{0,12,24};
        auto awayFromPixelBoundary=[](double x){const double f=x-std::floor(x);return std::min(f,1.-f)>.04;};
        for(const std::size_t feature_index:feature_ids){
          if(feature_index>=local.size())continue;
          const auto& feature=local[feature_index];
          for(const std::size_t patch_index:patch_ids){
            if(patch_index>=feature.points_global.size())continue;
            const auto photo=cube::coin::CoinPhotometricModel::linearize(local_manager,local_projector,
              image,points,poses[pose_index].T_GI,T_IL,feature.points_global[patch_index],
              feature.reference_intensity[patch_index],feature_settings.min_range,
              feature_settings.max_range,feature_settings.margin);
            if(!photo.valid||!awayFromPixelBoundary(photo.uv.x())||!awayFromPixelBoundary(photo.uv.y()))continue;

            V3D p_Li_official;V2D uv_official;int distortion_index_official=-1;
            const V3D p_Lk=photo.point_lidar_end;
            if(!official_projector->projectUndistortedPoint(frame,p_Lk,p_Li_official,uv_official,
                                                             distortion_index_official,true))
              throw std::runtime_error("official photo projector rejected a local valid Jacobian sample");
            if(distortion_index_official!=photo.distortion_index||
               (p_Li_official-photo.point_lidar_acquisition).cwiseAbs().maxCoeff()>1e-10||
               (uv_official-photo.uv).cwiseAbs().maxCoeff()>1e-10)
              throw std::runtime_error("local and official round=true photo projection differ");
            const double residual_official=getSubPixelValue<float>(frame.img_intensity,uv_official.x(),uv_official.y())-
                                           feature.reference_intensity[patch_index];
            Eigen::MatrixXd du_dp_official;official_projector->projectionJacobian(p_Li_official,du_dp_official);
            Eigen::Matrix<double,1,2> dI_du_official;
            dI_du_official<<
              .5*(getSubPixelValue<float>(frame.img_intensity,uv_official.x()+1.,uv_official.y())-
                  getSubPixelValue<float>(frame.img_intensity,uv_official.x()-1.,uv_official.y())),
              .5*(getSubPixelValue<float>(frame.img_intensity,uv_official.x(),uv_official.y()+1.)-
                  getSubPixelValue<float>(frame.img_intensity,uv_official.x(),uv_official.y()-1.));
            const Eigen::Matrix3d R_IG=poses[pose_index].T_GI.topLeftCorner<3,3>().transpose();
            const Eigen::Matrix3d R_Li_I=frame.T_Li_Lk_vec.at(static_cast<std::size_t>(frame.vec_idx.at(
              static_cast<std::size_t>(distortion_index_official)))).topLeftCorner<3,3>()*T_IL.topLeftCorner<3,3>().transpose();
            const V3D p_I=R_IG*(feature.points_global[patch_index]-poses[pose_index].T_GI.topRightCorner<3,1>());
            Eigen::Matrix3d p_I_skew;
            p_I_skew<<0.,-p_I.z(),p_I.y(),p_I.z(),0.,-p_I.x(),-p_I.y(),p_I.x(),0.;
            Eigen::Matrix<double,3,6> dp_dcorrection;
            dp_dcorrection.leftCols<3>()=R_Li_I*R_IG;
            dp_dcorrection.rightCols<3>()=-R_Li_I*p_I_skew;
            const Eigen::Matrix<double,1,6> h_official=dI_du_official*du_dp_official*dp_dcorrection;
            const double source_h_error=(h_official-photo.correction_jacobian_coin).cwiseAbs().maxCoeff();
            max_official_jacobian_error=std::max(max_official_jacobian_error,source_h_error);
            if(std::abs(residual_official-photo.residual)>1e-6||source_h_error>1e-8)++official_jacobian_mismatches;

            const Eigen::Vector2d image_gradient=cube::coin::CoinPhotometricModel::centralImageGradient(
              image.intensity,photo.uv.x(),photo.uv.y());
            const double image_eps=1e-3;
            const Eigen::Vector2d numeric_image_gradient(
              (cube::coin::CoinPhotometricModel::sampleFloat(image.intensity,photo.uv.x()+image_eps,photo.uv.y())-
               cube::coin::CoinPhotometricModel::sampleFloat(image.intensity,photo.uv.x()-image_eps,photo.uv.y()))/(2*image_eps),
              (cube::coin::CoinPhotometricModel::sampleFloat(image.intensity,photo.uv.x(),photo.uv.y()+image_eps)-
               cube::coin::CoinPhotometricModel::sampleFloat(image.intensity,photo.uv.x(),photo.uv.y()-image_eps))/(2*image_eps));
            Eigen::Vector2d image_gradient_relative=Eigen::Vector2d::Zero();
            for(int axis=0;axis<2;++axis)if(std::abs(numeric_image_gradient(axis))>1e-3){
              image_gradient_relative(axis)=std::abs(image_gradient(axis)-numeric_image_gradient(axis)) /
                std::max(std::abs(image_gradient(axis)),std::abs(numeric_image_gradient(axis)));
              image_gradient_relative_errors.push_back(image_gradient_relative(axis));
            }

            const cube::coin::Mat23 projection_jacobian=local_projector.projectionJacobian(photo.point_lidar_acquisition);
            Eigen::Matrix<double,2,3> numeric_projection_jacobian=Eigen::Matrix<double,2,3>::Zero();
            const double point_eps=1e-5;
            bool projection_fd_valid=true;
            for(int axis=0;axis<3;++axis){
              cube::coin::Vec3 plus_point=photo.point_lidar_acquisition,minus_point=photo.point_lidar_acquisition;
              plus_point(axis)+=point_eps;minus_point(axis)-=point_eps;
              const auto plus_projection=local_projector.project(plus_point);
              const auto minus_projection=local_projector.project(minus_point);
              if(!plus_projection.in_fov||!minus_projection.in_fov){projection_fd_valid=false;break;}
              numeric_projection_jacobian.col(axis)=(plus_projection.uv-minus_projection.uv)/(2*point_eps);
            }
            Eigen::Matrix<double,2,3> projection_relative=Eigen::Matrix<double,2,3>::Zero();
            if(projection_fd_valid)for(int row=0;row<2;++row)for(int axis=0;axis<3;++axis)
              if(std::abs(numeric_projection_jacobian(row,axis))>1e-6){
                projection_relative(row,axis)=std::abs(projection_jacobian(row,axis)-numeric_projection_jacobian(row,axis)) /
                  std::max(std::abs(projection_jacobian(row,axis)),std::abs(numeric_projection_jacobian(row,axis)));
                projector_relative_errors.push_back(projection_relative(row,axis));
              }

            for(int dof=0;dof<6;++dof){
              Eigen::Matrix<double,6,1> delta=Eigen::Matrix<double,6,1>::Zero();
              const double epsilon=dof<3?1e-3:1e-4;
              delta(dof)=epsilon;
              const double residual_plus=cube::coin::CoinPhotometricModel::evaluateFixed(local_projector,image,points,
                cube::coin::CoinPhotometricModel::applyCoinCorrection(poses[pose_index].T_GI,delta),T_IL,
                feature.points_global[patch_index],feature.reference_intensity[patch_index],photo.distortion_index);
              delta(dof)=-epsilon;
              const double residual_minus=cube::coin::CoinPhotometricModel::evaluateFixed(local_projector,image,points,
                cube::coin::CoinPhotometricModel::applyCoinCorrection(poses[pose_index].T_GI,delta),T_IL,
                feature.points_global[patch_index],feature.reference_intensity[patch_index],photo.distortion_index);
              if(!std::isfinite(residual_plus)||!std::isfinite(residual_minus))
                throw std::runtime_error("fixed-correspondence photometric FD left the image FOV");
              const double numeric_h=-(residual_plus-residual_minus)/(2*epsilon);
              const double analytic_h=photo.correction_jacobian_coin(dof);
              const double absolute_error=std::abs(analytic_h-numeric_h);
              const double relative_error=absolute_error/std::max({1e-6,std::abs(analytic_h),std::abs(numeric_h)});
              const bool sign_agreement=std::abs(numeric_h)<1e-5||std::abs(analytic_h)<1e-5||
                                        std::signbit(analytic_h)==std::signbit(numeric_h);
              if(std::abs(numeric_h)>=1e-5&&std::abs(analytic_h)>=1e-5&&!sign_agreement)++jacobian_sign_disagreements;
              if(std::max(std::abs(analytic_h),std::abs(numeric_h))>=1e-5)jacobian_relative_errors.push_back(relative_error);
              ++jacobian_samples;
              const double projector_x=std::max(projection_relative(0,0),std::max(projection_relative(0,1),projection_relative(0,2)));
              const double projector_y=std::max(projection_relative(1,0),std::max(projection_relative(1,1),projection_relative(1,2)));
              jacobian_out<<pose_index<<','<<feature_index<<','<<patch_index<<','<<photo.distortion_index<<','
                <<photo.uv.x()<<','<<photo.uv.y()<<','<<dof<<','<<epsilon<<','<<analytic_h<<','<<numeric_h<<','
                <<absolute_error<<','<<relative_error<<','<<source_h_error<<','<<projector_x<<','<<projector_y<<','
                <<image_gradient_relative.x()<<','<<image_gradient_relative.y()<<','<<(sign_agreement?1:0)<<'\n';
            }
          }
        }
      }
      out<<current_bag_frame<<','<<pose_index<<','<<motion_index<<','<<cloud_msg->header.stamp.toSec()<<','<<header_delta<<','<<end_delta<<','<<cloud->size()<<','
         <<weak.global.size()<<','<<weak.lidar.size()<<','<<weak_direction_transform_error<<','
         <<weak_geometry_global_error<<','<<weak_geometry_lidar_error<<','
         <<official.size()<<','<<local.size()<<','<<official_added<<','<<local_added<<','<<official_removed<<','<<local_removed<<','
         <<stat.candidates_after_nms<<','<<stat.selected_centers<<','<<stat.rejected_projection<<','<<stat.rejected_border<<','
         <<stat.rejected_mask<<','<<stat.rejected_range<<','<<stat.rejected_ncc<<','<<stat.rejected_lifetime<<','
         <<stat.ncc_values.size()<<','<<quantile(stat.ncc_values,0)<<','<<quantile(stat.ncc_values,.5)<<','
         <<quantile(stat.ncc_values,.95)<<','<<quantile(stat.ncc_values,1)<<','<<lifetimeHistogram(official)<<','
         <<comparison.mismatch<<','<<comparison.center<<','<<comparison.uv<<','<<comparison.world<<','<<comparison.reference<<','
         <<(pass?"PASS":"FAIL")<<'\n';
      ++matched;
    }
    bag.close();out.close();jacobian_out.close();
    const double photo_fd_p95=quantile(jacobian_relative_errors,.95);
    const double projector_fd_p95=quantile(projector_relative_errors,.95);
    const double image_gradient_fd_p95=quantile(image_gradient_relative_errors,.95);
    const bool jacobian_gate_pass=jacobian_samples>0&&official_jacobian_mismatches==0&&
      jacobian_sign_disagreements==0&&photo_fd_p95<1e-4&&projector_fd_p95<1e-4&&image_gradient_fd_p95<1e-4;
    std::cout<<"matched="<<matched<<" unmatched="<<unmatched<<" poses="<<poses.size()
             <<" max_header_delta_s="<<std::setprecision(12)<<max_header_delta
             <<" max_lidar_end_delta_s="<<max_end_delta<<" feature_mismatch_rows="<<total_mismatch
             <<" jacobian_samples="<<jacobian_samples
             <<" jacobian_relative_p50="<<quantile(jacobian_relative_errors,.5)
             <<" jacobian_relative_p95="<<photo_fd_p95
             <<" jacobian_sign_disagreements="<<jacobian_sign_disagreements
             <<" projector_relative_p95="<<projector_fd_p95
             <<" image_gradient_relative_p95="<<image_gradient_fd_p95
             <<" official_jacobian_mismatches="<<official_jacobian_mismatches
             <<" max_official_jacobian_error="<<max_official_jacobian_error
             <<" photo_jacobian_gate="<<(jacobian_gate_pass?"PASS":"BLOCKED")<<'\n';
    if(matched!=static_cast<int>(poses.size())||unmatched!=static_cast<int>(bag_scans.size()-poses.size())||max_end_delta>1e-3||total_mismatch!=0)
      throw std::runtime_error("feature shadow parity did not satisfy frame/state gates; inspect CSV");
    if(!jacobian_gate_pass)return 3;
    return 0;
  }catch(const std::exception& e){std::cerr<<"P2 COIN feature parity: "<<e.what()<<'\n';return 1;}
}
