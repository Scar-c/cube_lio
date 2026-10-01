// CUBE-LIO replay adapter, GPL-3.0-or-later. Estimator calls remain upstream.
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sys/resource.h>
#include <tbb/task_scheduler_init.h>
#include <rosbag/bag.h>
#include <rosbag/view.h>
#include "lio/super_lio.h"

class RecordedLIO : public LI2Sup::SuperLIO {
 public:
  explicit RecordedLIO(const std::string& dir) : trajectory_(dir + "/trajectory.tum") {
    if (!trajectory_) throw std::runtime_error("cannot write trajectory");
    trajectory_ << std::setprecision(17);
  }
  size_t frames = 0;
 protected:
  void Output() override {
    const auto state = kf_->GetNavState();
    const auto q = state.R.coeffs();
    trajectory_ << state.timestamp << ' ' << state.p.transpose() << ' '
                << q.transpose() << '\n';
    ++frames;
    SuperLIO::Output();
  }
 private:
  std::ofstream trajectory_;
};

int main(int argc, char** argv) {
  ros::init(argc, argv, "cube_offline");
  ros::NodeHandle nh;
  LI2Sup::LoadParamFromRos(nh);
  std::string bag_path, dir;
  nh.getParam("/lio/offline/bag", bag_path);
  nh.getParam("/lio/offline/out_dir", dir);
  int threads = 32;
  nh.param("/lio/offline/threads", threads, 32);
  if (threads < 1) throw std::invalid_argument("offline TBB thread count must be positive");
  tbb::task_scheduler_init scheduler(threads);
  auto wrapper = std::make_shared<LI2Sup::ROSWrapper>();
  RecordedLIO lio(dir);
  lio.setROSWrapper(wrapper);
  lio.init();
  rosbag::Bag bag(bag_path, rosbag::bagmode::Read);
  rosbag::View view(bag, rosbag::TopicQuery({LI2Sup::g_lidar_topic, LI2Sup::g_imu_topic}));
  if (!view.size()) throw std::runtime_error("no selected bag messages");
  struct rusage usage_before;
  getrusage(RUSAGE_SELF, &usage_before);
  const auto start = std::chrono::steady_clock::now();
  size_t lidar_count = 0, imu_count = 0;
  for (const auto& entry : view) {
    if (entry.getTopic() == LI2Sup::g_imu_topic) {
      auto m = entry.instantiate<sensor_msgs::Imu>();
      if (!m) throw std::runtime_error("IMU type mismatch");
      wrapper->replay(m);
      ++imu_count;
    } else if (LI2Sup::g_lidar_type == LI2Sup::LID_TYPE::LIVOX) {
      auto m = entry.instantiate<livox_ros_driver::CustomMsg>();
      if (!m) throw std::runtime_error("Livox type mismatch");
      wrapper->replay(m);
      ++lidar_count;
    } else {
      auto m = entry.instantiate<sensor_msgs::PointCloud2>();
      if (!m) throw std::runtime_error("PointCloud2 type mismatch");
      wrapper->replay(m);
      ++lidar_count;
    }
    lio.process();
  }
  for (int i = 0; i < 5; ++i) lio.process();
  const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  const auto cpu_seconds = [](const timeval& value) {
    return static_cast<double>(value.tv_sec) + static_cast<double>(value.tv_usec) / 1e6;
  };
  const double cpu_user = cpu_seconds(usage.ru_utime) - cpu_seconds(usage_before.ru_utime);
  const double cpu_system = cpu_seconds(usage.ru_stime) - cpu_seconds(usage_before.ru_stime);
  std::ofstream metrics(dir + "/run.json");
  metrics << std::setprecision(17);
  metrics << "{\"lidar_read\":" << lidar_count << ",\"imu_read\":" << imu_count
          << ",\"frames\":" << lio.frames << ",\"wall_processing_s\":" << wall
          << ",\"bag_duration_s\":" << (view.getEndTime() - view.getBeginTime()).toSec()
          << ",\"cpu_user_s\":" << cpu_user << ",\"cpu_system_s\":" << cpu_system
          << ",\"peak_rss_kb\":" << usage.ru_maxrss << ",\"tbb_threads\":" << threads << "}\n";
  lio.printTimeRecord();
  ros::shutdown();
}
