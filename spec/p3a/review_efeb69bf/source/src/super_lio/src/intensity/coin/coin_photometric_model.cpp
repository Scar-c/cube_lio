// COIN-LIO photometric residual/Jacobian semantics, reimplemented from source.
#include "intensity/coin/coin_photometric_model.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cube::coin {
namespace {
Eigen::Matrix3d skew(const Eigen::Vector3d& v){
  Eigen::Matrix3d m;
  m<<0.,-v.z(),v.y(),v.z(),0.,-v.x(),-v.y(),v.x(),0.;
  return m;
}
}

double CoinPhotometricModel::sampleFloat(const cv::Mat& image,double x,double y){
  x=std::clamp(x,0.,static_cast<double>(image.cols-1));
  y=std::clamp(y,0.,static_cast<double>(image.rows-1));
  const int x0=static_cast<int>(x),x1=x0+1,y0=static_cast<int>(y),y1=y0+1;
  const double ax=x-std::floor(x),ay=y-std::floor(y);
  const double value=(1-ax)*(1-ay)*image.ptr<float>(y0)[x0]+ax*(1-ay)*image.ptr<float>(y0)[x1]+
                     (1-ax)*ay*image.ptr<float>(y1)[x0]+ax*ay*image.ptr<float>(y1)[x1];
  return static_cast<float>(value);
}

Eigen::Vector2d CoinPhotometricModel::centralImageGradient(const cv::Mat& image,double x,double y){
  return Eigen::Vector2d(.5*(sampleFloat(image,x+1.,y)-sampleFloat(image,x-1.,y)),
                         .5*(sampleFloat(image,x,y+1.)-sampleFloat(image,x,y-1.)));
}

CoinPhotoLinearization CoinPhotometricModel::linearize(const CoinFeatureManager& manager,
    const CoinOusterProjector& projector,const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
    const Eigen::Matrix4d& T_GI,const Eigen::Matrix4d& T_IL,const Vec3& point_global,
    double reference_intensity,double min_range,double max_range,int margin){
  CoinPhotoLinearization out;
  const Eigen::Matrix3d R_GI=T_GI.topLeftCorner<3,3>();
  const Eigen::Matrix3d R_IG=R_GI.transpose();
  const Eigen::Matrix3d R_GL=R_GI*T_IL.topLeftCorner<3,3>();
  const Vec3 t_GL=R_GI*T_IL.topRightCorner<3,1>()+T_GI.topRightCorner<3,1>();
  const Eigen::Matrix3d R_LG=R_GL.transpose();
  out.point_lidar_end=R_LG*(point_global-t_GL);
  if(!manager.projectUndistorted(frame,points,out.point_lidar_end,out.point_lidar_acquisition,
                                 out.uv,out.distortion_index,true))return out;
  if(out.point_lidar_acquisition.norm()<min_range||out.point_lidar_acquisition.norm()>max_range)return out;
  if(!(out.uv.x()>margin&&out.uv.x()<frame.intensity.cols-margin&&
       out.uv.y()>margin&&out.uv.y()<frame.intensity.rows-margin))return out;
  if(frame.mask.ptr<uchar>(static_cast<int>(out.uv.y()))[static_cast<int>(out.uv.x())]==0)return out;
  if(out.distortion_index<0||static_cast<std::size_t>(out.distortion_index)>=points.size())return out;
  const int transform_index=frame.vec_idx.empty()?0:frame.vec_idx.at(static_cast<std::size_t>(out.distortion_index));
  if(transform_index<0||static_cast<std::size_t>(transform_index)>=frame.T_Li_Lk_vec.size())return out;
  const Eigen::Matrix4d& T_Li_Lk=frame.T_Li_Lk_vec.at(static_cast<std::size_t>(transform_index));
  const double intensity=sampleFloat(frame.intensity,out.uv.x(),out.uv.y());
  const Eigen::Vector2d image_gradient=centralImageGradient(frame.intensity,out.uv.x(),out.uv.y());
  const Eigen::Matrix<double,1,2> dI_du=image_gradient.transpose();
  const Mat23 du_dp=projector.projectionJacobian(out.point_lidar_acquisition);

  const Eigen::Matrix3d R_Li_I=T_Li_Lk.topLeftCorner<3,3>()*T_IL.topLeftCorner<3,3>().transpose();
  const Vec3 p_I=R_IG*(point_global-T_GI.topRightCorner<3,1>());
  Eigen::Matrix<double,3,6> dp_dcorrection;
  dp_dcorrection.leftCols<3>()=R_Li_I*R_IG;
  dp_dcorrection.rightCols<3>()=-R_Li_I*skew(p_I);
  out.correction_jacobian_coin=dI_du*du_dp*dp_dcorrection;
  out.correction_jacobian_super<<out.correction_jacobian_coin.segment<3>(3),
                                  out.correction_jacobian_coin.segment<3>(0);
  out.residual=intensity-reference_intensity;
  out.valid=true;
  return out;
}

double CoinPhotometricModel::evaluateFixed(const CoinOusterProjector& projector,const CoinFrame& frame,
    const std::vector<CoinScanPoint>& points,const Eigen::Matrix4d& T_GI,const Eigen::Matrix4d& T_IL,
    const Vec3& point_global,double reference_intensity,int distortion_index){
  if(distortion_index<0||static_cast<std::size_t>(distortion_index)>=points.size())
    return std::numeric_limits<double>::quiet_NaN();
  const int transform_index=frame.vec_idx.empty()?0:frame.vec_idx.at(static_cast<std::size_t>(distortion_index));
  if(transform_index<0||static_cast<std::size_t>(transform_index)>=frame.T_Li_Lk_vec.size())
    return std::numeric_limits<double>::quiet_NaN();
  const Eigen::Matrix3d R_GI=T_GI.topLeftCorner<3,3>();
  const Eigen::Matrix3d R_GL=R_GI*T_IL.topLeftCorner<3,3>();
  const Vec3 t_GL=R_GI*T_IL.topRightCorner<3,1>()+T_GI.topRightCorner<3,1>();
  const Vec3 p_Lk=R_GL.transpose()*(point_global-t_GL);
  const Eigen::Matrix4d& T_Li_Lk=frame.T_Li_Lk_vec.at(static_cast<std::size_t>(transform_index));
  const Vec3 p_Li=T_Li_Lk.topLeftCorner<3,3>()*p_Lk+T_Li_Lk.topRightCorner<3,1>();
  const ProjectedPoint projection=projector.project(p_Li);
  if(!projection.in_fov)return std::numeric_limits<double>::quiet_NaN();
  return sampleFloat(frame.intensity,projection.uv.x(),projection.uv.y())-reference_intensity;
}

Eigen::Matrix4d CoinPhotometricModel::applyCoinCorrection(const Eigen::Matrix4d& T_GI,
    const Eigen::Matrix<double,6,1>& delta){
  Eigen::Matrix4d corrected=T_GI;
  const Eigen::Vector3d rotation=delta.tail<3>();
  const double angle=rotation.norm();
  if(angle>0.)corrected.topLeftCorner<3,3>()=T_GI.topLeftCorner<3,3>()*
      Eigen::AngleAxisd(angle,rotation/angle).toRotationMatrix();
  corrected.topRightCorner<3,1>()+=delta.head<3>();
  return corrected;
}

} // namespace cube::coin
