// COIN-LIO patch feature semantics reimplemented from the pinned BSD-3-Clause oracle.
#include "intensity/coin/coin_feature_manager.hpp"
#include <ros/ros.h>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace cube::coin {

CoinFeatureSettings CoinFeatureSettings::fromRosParams(){
  ros::NodeHandle nh;CoinFeatureSettings s;
  nh.param("image/patch_size",s.patch_size,5);
  nh.param("image/max_range",s.max_range,30.);
  nh.param("image/max_lifetime",s.max_lifetime,25);
  nh.param("image/min_range",s.min_range,.7);
  nh.param("image/suppression_radius",s.suppression_radius,10);
  nh.param("image/num_features",s.num_features,60);
  nh.param("image/grad_min",s.grad_min,16.5);
  nh.param("image/ncc_threshold",s.ncc_threshold,.7075);
  nh.param("image/margin",s.margin,10);
  nh.param("image/range_threshold",s.range_threshold,.2);
  nh.param("filter/n_uninformative",s.n_uninformative,25.);
  s.validate();return s;
}

void CoinFeatureSettings::validate() const{
  if(patch_size<1||patch_size%2==0||max_range<=min_range||min_range<0||max_lifetime<1||
     suppression_radius<0||num_features<1||grad_min<0||ncc_threshold<-1||ncc_threshold>1||
     margin<0||range_threshold<0||n_uninformative<0)
    throw std::invalid_argument("invalid COIN feature settings");
}

CoinFeatureManager::CoinFeatureManager(CoinOusterProjector projector,CoinFeatureSettings settings)
  :projector_(std::move(projector)),settings_(std::move(settings)){
  settings_.validate();
  margin_mask_=cv::Mat::zeros(projector_.rows(),projector_.cols(),CV_8UC1);
  const cv::Rect roi(settings_.margin,settings_.margin,
                     projector_.cols()-2*settings_.margin,projector_.rows()-2*settings_.margin);
  if(roi.width<=0||roi.height<=0)throw std::invalid_argument("COIN feature margin removes the full image");
  margin_mask_(roi)=255;
  const int half=settings_.patch_size/2;
  patch_offsets_.reserve(settings_.patch_size*settings_.patch_size);
  // The oracle builds offsets as (i,j), then applies i to image x and j to image y.
  // Store offsets in row/column order while preserving that exact patch traversal.
  for(int i=-half;i<=half;++i)for(int j=-half;j<=half;++j)
    patch_offsets_.emplace_back(j,i);
}

double CoinFeatureManager::normalizedCrossCorrelation(const std::vector<double>& reference,
                                                       const std::vector<double>& current){
  if(reference.size()!=current.size()||reference.empty())return std::numeric_limits<double>::quiet_NaN();
  double ref_mean=0,current_mean=0;
  for(std::size_t i=0;i<reference.size();++i){ref_mean+=reference[i];current_mean+=current[i];}
  ref_mean/=reference.size();current_mean/=current.size();
  double numerator=0,denom_ref=0,denom_current=0;
  for(std::size_t i=0;i<reference.size();++i){
    const double a=reference[i]-ref_mean,b=current[i]-current_mean;
    numerator+=a*b;denom_ref+=a*a;denom_current+=b*b;
  }
  return numerator/std::sqrt(denom_ref*denom_current);
}

CoinWeakDirections CoinFeatureManager::weakDirectionsFromGeometry(const Eigen::MatrixXd& H_translation,
                                                                   const Eigen::Matrix3d& R_GL,
                                                                   double n_uninformative){
  CoinWeakDirections result;
  result.geometry_rows=H_translation.rows();
  if(H_translation.cols()!=3)throw std::invalid_argument("COIN weak-direction Jacobian must have 3 translation columns");
  if(H_translation.rows()>3){
    const Eigen::Matrix3d hth=H_translation.transpose()*H_translation;
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> solver(hth);
    if(solver.info()!=Eigen::Success)throw std::runtime_error("COIN geometry eigenvector calculation failed");
    result.eigenvalues=solver.eigenvalues().cwiseMax(0.);
    result.eigenvectors=solver.eigenvectors();
    for(Eigen::Index i=0;i<H_translation.rows();++i){
      Vec3 normalized_row=H_translation.row(i).transpose();normalized_row.normalize();
      for(int d=0;d<3;++d){
        const float dot=static_cast<float>(std::abs(normalized_row.dot(result.eigenvectors.col(d))));
        if(dot>.5f)result.contribution(d)+=dot;
      }
    }
    for(int d=0;d<3;++d)if(result.contribution(d)<n_uninformative)
      result.global.push_back(result.eigenvectors.col(d));
  }
  for(const auto& direction:result.global)result.lidar.push_back(R_GL.transpose()*direction);
  if(result.lidar.empty()){
    result.lidar.emplace_back(1,0,0);result.lidar.emplace_back(0,1,0);result.lidar.emplace_back(0,0,1);
  }
  return result;
}

std::vector<cv::Point> CoinFeatureManager::selectPureGradient(
    const std::vector<std::pair<double,cv::Point>>& candidates,int cap){
  if(cap<=0)return {};
  auto ranked=candidates;
  std::sort(ranked.begin(),ranked.end(),[](const auto& a,const auto& b){
    if(a.first!=b.first)return a.first>b.first;
    if(a.second.y!=b.second.y)return a.second.y<b.second.y;
    return a.second.x<b.second.x;
  });
  std::vector<cv::Point> result;
  result.reserve(std::min(cap,static_cast<int>(ranked.size())));
  for(const auto& item:ranked){
    result.push_back(item.second);
    if(static_cast<int>(result.size())==cap)break;
  }
  return result;
}

double CoinFeatureManager::sampleBilinearFloat(const cv::Mat& image,double x,double y){
  x=std::clamp(x,0.,static_cast<double>(image.cols-1));
  y=std::clamp(y,0.,static_cast<double>(image.rows-1));
  const int x0=static_cast<int>(x),x1=x0+1,y0=static_cast<int>(y),y1=y0+1;
  const double ax=x-std::floor(x),ay=y-std::floor(y);
  const double value=(1-ax)*(1-ay)*image.ptr<float>(y0)[x0]+ax*(1-ay)*image.ptr<float>(y0)[x1]+
                     (1-ax)*ay*image.ptr<float>(y1)[x0]+ax*ay*image.ptr<float>(y1)[x1];
  return static_cast<float>(value);
}

bool CoinFeatureManager::projectUndistorted(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
                                             const Vec3& p_Lk,Vec3& p_Li,Vec2& uv,int& distortion_index,
                                             bool round_bucket) const{
  ProjectedPoint projected=projector_.project(p_Lk);
  if(!projected.in_fov)return false;
  if(round_bucket){projected.uv.x()=std::round(projected.uv.x());projected.uv.y()=std::round(projected.uv.y());}
  int row=static_cast<int>(projected.uv.y()),col=static_cast<int>(projected.uv.x());
  constexpr std::size_t duplicate_points=10;
  std::size_t cell=(static_cast<std::size_t>(row)*projector_.cols()+col)*duplicate_points;
  if(cell>=frame.projected_index.size())return false;
  if(frame.projected_index[cell]==0){
    row=0;
    while(row<projector_.rows()){
      cell=(static_cast<std::size_t>(row)*projector_.cols()+col)*duplicate_points;
      if(cell<frame.projected_index.size()&&frame.projected_index[cell]>0)break;
      ++row;
    }
  }
  if(row>=projector_.rows())return false;
  const int count=frame.projected_index[cell];
  if(count<=0||cell+static_cast<std::size_t>(count)>=frame.projected_index.size())return false;
  distortion_index=-1;
  if(count>1){
    float min_distance=std::numeric_limits<float>::max();
    for(int i=1;i<=count;++i){
      const int candidate=frame.projected_index[cell+i];
      if(candidate<0||static_cast<std::size_t>(candidate)>=points.size())continue;
      const Vec3 delta=p_Lk-points[candidate].point_lidar;
      const float distance=static_cast<float>(delta.norm());
      if(distance<min_distance){min_distance=distance;distortion_index=candidate;}
    }
  }else distortion_index=frame.projected_index[cell+1];
  if(distortion_index<0||static_cast<std::size_t>(distortion_index)>=points.size())return false;
  const int transform_index=frame.vec_idx.empty()?0:frame.vec_idx.at(distortion_index);
  const Eigen::Matrix4d T_Li_Lk=frame.T_Li_Lk_vec.empty()?Eigen::Matrix4d::Identity():frame.T_Li_Lk_vec.at(transform_index);
  p_Li=T_Li_Lk.topLeftCorner<3,3>()*p_Lk+T_Li_Lk.topRightCorner<3,1>();
  const ProjectedPoint distorted=projector_.project(p_Li);
  if(!distorted.in_fov)return false;
  uv=distorted.uv;return true;
}

void CoinFeatureManager::update(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
                                const std::vector<Vec3>& weak_directions_lidar,const Eigen::Matrix4d& T_GL,
                                bool pure_gradient,const std::vector<Vec3>& audit_eigenvectors_lidar){
  last_stats_=CoinFeatureFrameStats{};
  last_stats_.active_before=static_cast<int>(features_.size());
  track(frame,points,T_GL);
  updateSuppressionMask();
  detect(frame,points,weak_directions_lidar,T_GL,pure_gradient,audit_eigenvectors_lidar);
  last_stats_.active_after=static_cast<int>(features_.size());
}

void CoinFeatureManager::track(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
                               const Eigen::Matrix4d& T_GL){
  const Eigen::Matrix3d R_LG=T_GL.topLeftCorner<3,3>().transpose();
  const Vec3 p_LG=-R_LG*T_GL.topRightCorner<3,1>();
  std::vector<CoinFeature> survivors;survivors.reserve(features_.size());
  for(auto& feature:features_){
    const std::size_t patch_size=feature.points_global.size();
    std::vector<double> current(patch_size,0.);std::vector<Vec2> uv_patch;uv_patch.reserve(patch_size);
    bool visible=true;int reject_reason=0;
    for(std::size_t l=0;l<patch_size;++l){
      const Vec3 p_Lk=R_LG*feature.points_global[l]+p_LG;
      Vec3 p_Li;Vec2 uv;int distortion_index=-1;
      if(!projectUndistorted(frame,points,p_Lk,p_Li,uv,distortion_index)){
        visible=false;reject_reason=1;++last_stats_.rejected_projection;break;
      }
      if(uv.x()<settings_.margin||uv.x()>frame.intensity.cols-settings_.margin||
         uv.y()<settings_.margin||uv.y()>frame.intensity.rows-settings_.margin){
        visible=false;reject_reason=2;++last_stats_.rejected_border;break;
      }
      if(frame.mask.ptr<uchar>(static_cast<int>(uv.y()))[static_cast<int>(uv.x())]==0){
        visible=false;reject_reason=3;++last_stats_.rejected_mask;break;
      }
      const double old_range=p_Li.norm();
      const double new_range=frame.range.ptr<float>(static_cast<int>(uv.y()))[static_cast<int>(uv.x())];
      if(std::abs(old_range-new_range)>settings_.range_threshold){
        visible=false;reject_reason=4;++last_stats_.rejected_range;break;
      }
      current[l]=sampleBilinearFloat(frame.intensity,uv.x(),uv.y());
      uv_patch.push_back(uv);
    }
    feature.current_uv=std::move(uv_patch);
    const double ncc=normalizedCrossCorrelation(feature.reference_intensity,current);
    last_stats_.ncc_values.push_back(ncc);
    if(feature.lifetime<settings_.max_lifetime&&visible&&ncc>settings_.ncc_threshold){
      ++feature.lifetime;feature.center=feature.current_uv.at(patch_size/2);survivors.push_back(std::move(feature));
    }else{
      ++last_stats_.removed;
      if(feature.lifetime>=settings_.max_lifetime)++last_stats_.rejected_lifetime;
      else if(visible||reject_reason==0)++last_stats_.rejected_ncc;
    }
  }
  features_=std::move(survivors);
}

void CoinFeatureManager::updateSuppressionMask(){
  suppression_mask_=margin_mask_.clone();
  for(const auto& feature:features_)
    cv::circle(suppression_mask_,cv::Point2f(feature.center.x(),feature.center.y()),
               settings_.suppression_radius,0,-1);
}

void CoinFeatureManager::detect(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
                                const std::vector<Vec3>& weak_directions_lidar,const Eigen::Matrix4d& T_GL,
                                bool pure_gradient,const std::vector<Vec3>& audit_eigenvectors_lidar){
  const int needed=settings_.num_features-static_cast<int>(features_.size());
  if(needed<=0)return;
  std::vector<Vec3> directions=weak_directions_lidar;
  if(directions.empty()){
    directions.emplace_back(1,0,0);directions.emplace_back(0,1,0);directions.emplace_back(0,0,1);
  }
  std::vector<cv::Point> centers;
  detectComplementary(frame,points,directions,needed,centers,pure_gradient);
  last_stats_.selected_centers=static_cast<int>(centers.size());
  for(const auto& center:centers){
    last_stats_.selected_center_pixels.emplace_back(center.x,center.y);
    const float gx=frame.dx.ptr<float>(center.y)[center.x];
    const float gy=frame.dy.ptr<float>(center.y)[center.x];
    last_stats_.selected_gradient_sum+=std::hypot(gx,gy);
    const int point_index=frame.image_index.ptr<int>(center.y)[center.x];
    if(point_index>=0&&static_cast<std::size_t>(point_index)<points.size()){
      const Mat23 du_dp=projector_.projectionJacobian(points[point_index].point_lidar);
      const Eigen::Matrix<double,1,2> image_gradient(gx,gy);
      for(std::size_t d=0;d<std::min<std::size_t>(3,audit_eigenvectors_lidar.size());++d){
        Vec2 motion=du_dp*audit_eigenvectors_lidar[d];
        const double norm=motion.norm();
        if(norm>1e-12)last_stats_.selected_directional_score_sum[d]+=
            std::abs(image_gradient.dot(motion/norm));
      }
      ++last_stats_.selected_metric_count;
    }
  }
  const Eigen::Matrix3d R_GL=T_GL.topLeftCorner<3,3>();
  const Vec3 t_GL=T_GL.topRightCorner<3,1>();
  for(const auto& center:centers){
    CoinFeature feature;feature.id=next_id_++;feature.lifetime=1;feature.center=Vec2(center.x,center.y);
    feature.points_global.reserve(patch_offsets_.size());
    feature.reference_intensity.reserve(patch_offsets_.size());
    feature.current_uv.reserve(patch_offsets_.size());
    for(const auto& offset:patch_offsets_){
      const int row=center.y+offset.x(),col=center.x+offset.y();
      const int point_index=frame.image_index.ptr<int>(row)[col];
      if(point_index<0||static_cast<std::size_t>(point_index)>=points.size())
        throw std::runtime_error("COIN selected patch pixel has no dense point owner");
      feature.points_global.push_back(R_GL*points[point_index].point_lidar+t_GL);
      feature.reference_intensity.push_back(frame.intensity.ptr<float>(row)[col]);
      feature.current_uv.emplace_back(col,row);
    }
    features_.push_back(std::move(feature));
  }
  last_stats_.added=static_cast<int>(centers.size());
}

void CoinFeatureManager::detectComplementary(const CoinFrame& frame,const std::vector<CoinScanPoint>& points,
                                              const std::vector<Vec3>& directions,int needed,
                                              std::vector<cv::Point>& centers,bool pure_gradient){
  cv::Mat abs_dx,abs_dy,gradient;
  cv::convertScaleAbs(frame.dx,abs_dx);cv::convertScaleAbs(frame.dy,abs_dy);
  cv::addWeighted(abs_dx,.5,abs_dy,.5,0,gradient);
  const cv::Mat valid_mask=frame.mask & suppression_mask_;
  for(int row=0;row<valid_mask.rows;++row)for(int col=0;col<valid_mask.cols;++col)
    if(valid_mask.ptr<uchar>(row)[col]==0)gradient.ptr<uchar>(row)[col]=0;

  std::vector<std::pair<double,cv::Point>> scores;
  scores.reserve(gradient.total());
  for(int row=0;row<gradient.rows;++row)for(int col=0;col<gradient.cols;++col)
    if(gradient.ptr<uchar>(row)[col]>settings_.grad_min)
      scores.emplace_back(gradient.ptr<uchar>(row)[col],cv::Point(col,row));
  std::sort(scores.begin(),scores.end(),[](const auto& a,const auto& b){return a.first>b.first;});

  cv::Mat feature_mask=valid_mask.clone();std::vector<cv::Point> candidates;candidates.reserve(scores.size());
  for(const auto& score:scores){
    const cv::Point& p=score.second;
    if(feature_mask.ptr<uchar>(p.y)[p.x]==0)continue;
    candidates.push_back(p);cv::circle(feature_mask,p,settings_.suppression_radius,0,-1);
  }
  last_stats_.candidates_after_nms=static_cast<int>(candidates.size());
  if(pure_gradient){
    std::vector<std::pair<double,cv::Point>> gradient_candidates;
    gradient_candidates.reserve(candidates.size());
    for(const auto& point:candidates)
      gradient_candidates.emplace_back(gradient.ptr<uchar>(point.y)[point.x],point);
    centers=selectPureGradient(gradient_candidates,needed);
    return;
  }
  std::vector<std::vector<std::pair<double,int>>> directional_scores(
      directions.size(),std::vector<std::pair<double,int>>(candidates.size(),std::make_pair(0.,0)));
  const int offset=settings_.patch_size/2+1;
  for(std::size_t i=0;i<candidates.size();++i){
    const cv::Point& candidate=candidates[i];
    const cv::Rect roi(candidate.x-offset,candidate.y-offset,settings_.patch_size+2,settings_.patch_size+2);
    const cv::Mat local=frame.intensity(roi);cv::Mat eigen;
    cv::cornerEigenValsAndVecs(local,eigen,5,3);
    const cv::Vec6f values=eigen.ptr<cv::Vec6f>(offset)[offset];
    const float ix=values[0]>=values[1]?values[2]:values[4];
    const float iy=values[0]>=values[1]?values[3]:values[5];
    Eigen::Matrix<double,1,2> image_gradient;image_gradient<<ix,iy;
    const int point_index=frame.image_index.ptr<int>(candidate.y)[candidate.x];
    if(point_index<0||static_cast<std::size_t>(point_index)>=points.size())continue;
    const Mat23 du_dp=projector_.projectionJacobian(points[point_index].point_lidar);
    for(std::size_t d=0;d<directions.size();++d){
      Vec2 projected_motion=du_dp*directions[d];projected_motion.normalize();
      const float score=std::abs(image_gradient*projected_motion);
      directional_scores[d][i]=std::make_pair(score,static_cast<int>(i));
    }
  }
  for(auto& direction_scores:directional_scores)
    std::sort(direction_scores.begin(),direction_scores.end(),[](const auto& a,const auto& b){return a.first>b.first;});

  std::vector<int> seen;seen.reserve(candidates.size());
  if(directional_scores.empty())return;
  for(std::size_t rank=0;rank<directional_scores.front().size()&&static_cast<int>(centers.size())<needed;++rank){
    for(std::size_t d=0;d<directional_scores.size()&&static_cast<int>(centers.size())<needed;++d){
      const int candidate_id=directional_scores[d][rank].second;
      if(std::find(seen.begin(),seen.end(),candidate_id)!=seen.end())continue;
      seen.push_back(candidate_id);centers.push_back(candidates[candidate_id]);
    }
  }
}

} // namespace cube::coin
