#include "ros/ouster_time_support.hpp"
#include "intensity/coin/coin_acquisition.hpp"
#include "lio/ouster_scan_history.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Raw {float x,y,z;std::uint32_t t;};
LI2Sup::IMUData imu(double time,float ax){
  LI2Sup::IMUData x;x.secs=time;x.acc=BASIC::V3(ax,0,0);return x;
}
Eigen::Matrix4d pose(const Eigen::Matrix3d& R,const Eigen::Vector3d& p){
  Eigen::Matrix4d T=Eigen::Matrix4d::Identity();
  T.topLeftCorner<3,3>()=R;T.topRightCorner<3,1>()=p;return T;
}
}

int main(){
  try{
    using namespace LI2Sup;
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const std::vector<Raw> raw{{1,0,0,0},{1,0,0,10000000},
        {1,0,0,8000000},{nan,0,0,20000000},{.1f,0,0,30000000}};
    const auto support=ousterScanTimeSupport(raw,.65);
    require(support.has_valid_raw&&support.valid_raw_points==3,
            "raw validity must match COIN Ouster preprocessing");
    require(std::abs(support.max_offset_s-.01)<1e-12,
            "scan end must use maximum valid raw timestamp");
    require(raw.back().t*1e-9!=support.max_offset_s&&raw[2].t*1e-9<support.max_offset_s,
            "the last visited and filter-rate selected points need not end the scan");

    require(!imuHasReachedScanEnd(1.014,1.015)&&imuHasReachedScanEnd(1.02,1.015),
            "sync must wait for IMU coverage of the full scan end");
    std::deque<IMUData> buffer{imu(1.,0),imu(1.01,10),imu(1.02,20)};
    auto first=collectOusterImuWindow(buffer,1.015,nullptr);
    require(first.issue.empty()&&first.has_end_sample&&first.samples.size()==3,
            "bracketed scan must receive one interpolated end state");
    require(std::abs(first.end_sample.secs-1.015)<1e-12&&
            std::abs(first.end_sample.acc.x()-15.)<1e-4,
            "end IMU measurement must be interpolated between real samples");
    require(buffer.size()==1&&std::abs(buffer.front().secs-1.02)<1e-12,
            "right IMU sample must remain for the next scan");
    buffer.push_back(imu(1.03,30));
    auto second=collectOusterImuWindow(buffer,1.025,&first.end_sample);
    require(second.issue.empty()&&second.has_end_sample&&
            std::abs(second.end_sample.acc.x()-25.)<1e-4,
            "sequential windows must preserve the inter-scan IMU interval");
    std::deque<IMUData> gap{imu(2.,0),imu(2.1,1)};
    auto invalid=collectOusterImuWindow(gap,2.05,nullptr);
    require(!invalid.has_end_sample&&invalid.issue=="imu_bracket_gap_or_order_invalid",
            "wide IMU gaps must be classified without extrapolation");

    LidarData lidar;
    lidar.coin_raw_points.push_back(CoinRawPoint{});
    std::vector<DynamicState> history;
    require(cube::coin::coinFrameInputIssue(lidar,history)==
            "insufficient_propagated_imu_history",
            "insufficient propagated history must select a full-frame skip");
    history.resize(2);history[0].time=10.;history[1].time=10.01;
    require(cube::coin::coinFrameInputIssue(lidar,history).empty(),
            "supported frame must proceed to acquisition checks");
    lidar.imu_support_issue="imu_bracket_gap_or_order_invalid";
    require(cube::coin::coinFrameInputIssue(lidar,history)==lidar.imu_support_issue,
            "a genuine IMU gap must skip the complete COIN frame");
    lidar.imu_support_issue.clear();
    Eigen::Matrix3d R;Eigen::Vector3d p;
    require(!cube::coin::stateAt(history,9.9,R,p)&&
            !cube::coin::stateAt(history,10.02,R,p),
            "unsupported acquisition times must not use fallback poses");
    require(cube::coin::stateAt(history,10.005,R,p)&&R.allFinite()&&p.allFinite(),
            "bracketed acquisition state must be finite");
    std::vector<DynamicState> overlap_history(3);
    overlap_history[0].time=10.;overlap_history[1].time=10.05;
    overlap_history[2].time=10.1;
    overlap_history[2].p=BASIC::V3(1,0,0);
    DynamicState corrected=overlap_history.back();
    corrected.R=Eigen::AngleAxis<BASIC::scalar>(.1f,BASIC::V3::UnitZ()).toRotationMatrix();
    corrected.p=BASIC::V3(2,1,0);
    corrected.v=BASIC::V3(.5f,0,0);
    retainRebasedOusterHistory(overlap_history,corrected,10.09);
    require(overlap_history.size()==2&&overlap_history.front().time<=10.09&&
            overlap_history.back().time==10.1&&
            (overlap_history.back().p-corrected.p).norm()<1e-6,
            "overlapping scans must retain a pre-start state aligned to the corrected ESKF end");
    require(cube::coin::stateAt(overlap_history,10.09,R,p)&&R.allFinite()&&p.allFinite(),
            "overlap acquisition time must have a valid Super-history bracket");
    const Eigen::Matrix3d R0=Eigen::AngleAxisd(.2,Eigen::Vector3d::UnitZ()).toRotationMatrix();
    const Eigen::Matrix3d R1=Eigen::AngleAxisd(.3,Eigen::Vector3d::UnitY()).toRotationMatrix();
    const Eigen::Vector3d p0(1,2,3),p1(2,3,4);
    const auto end_identity=cube::coin::lidarAcquisitionToEnd(R1,p1,R1,p1);
    require((end_identity-Eigen::Matrix4d::Identity()).norm()<1e-12,
            "acquisition transform at scan end must be identity");
    const auto relative=cube::coin::lidarAcquisitionToEnd(R0,p0,R1,p1);
    require(relative.allFinite()&&
            (pose(R0,p0)*relative-pose(R1,p1)).norm()<1e-12&&
            (relative*relative.inverse()-Eigen::Matrix4d::Identity()).norm()<1e-12,
            "acquisition transform composition and inverse must be finite");
    std::cout<<"{\"scan_end\":\"PASS\",\"sync_window\":\"PASS\","
                "\"full_frame_skip\":\"PASS\",\"transforms\":\"PASS\"}\n";
    return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
