// COIN-LIO Ouster projection reimplementation. See spec/p2/COIN_SOURCE_MAPPING.md.
#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <cstddef>
#include <vector>

namespace cube::coin {

using Vec2 = Eigen::Vector2d;
using Vec3 = Eigen::Vector3d;
using Mat23 = Eigen::Matrix<double,2,3>;

struct OusterMetadata {
  int rows=0, cols=0, u_shift=0;
  double beam_offset_mm=0;
  bool destagger=true;
  std::vector<int> pixel_shift_by_row;
  std::vector<double> beam_altitude_degrees;
  static OusterMetadata fromRosParams();
  void validate() const;
};

struct ProjectedPoint {
  bool in_fov=false;
  Vec2 uv=Vec2::Zero();
};

class CoinOusterProjector {
 public:
  explicit CoinOusterProjector(OusterMetadata metadata, int cube_resolution=0);
  const OusterMetadata& metadata() const { return metadata_; }
  int rows() const { return cube_resolution_ ? cube_resolution_ : metadata_.rows; }
  int cols() const { return cube_resolution_ ? 6*cube_resolution_ : metadata_.cols; }
  int cubeResolution() const { return cube_resolution_; }
  std::size_t indexFromPixel(int row, int col) const;
  ProjectedPoint project(const Vec3& p) const;
  Mat23 projectionJacobian(const Vec3& p) const;
  void pixelFromRawIndex(std::size_t raw_index, int& row, int& col) const;

 private:
  OusterMetadata metadata_;
  int cube_resolution_=0;
  std::vector<double> elevation_radians_;
  std::vector<int> raw_to_row_, raw_to_col_;
  Eigen::Matrix3d K_=Eigen::Matrix3d::Zero();
  double beam_offset_m_=0;
};

} // namespace cube::coin
