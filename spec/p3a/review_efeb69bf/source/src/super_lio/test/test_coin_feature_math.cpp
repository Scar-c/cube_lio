#include "intensity/coin/coin_feature_manager.hpp"
#include "intensity/coin/coin_intensity_representation.hpp"
#include "intensity/coin/coin_cubemap_representation.hpp"
#include "intensity/coin/super_degeneracy_gate.hpp"
#include "intensity/intensity_representation.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
}

int main(){
  using namespace cube::coin;
  try{
    using PureGradientSelectorSignature=std::vector<cv::Point>(*) (
        const std::vector<std::pair<double,cv::Point>>&,int);
    static_assert(std::is_same<decltype(&CoinFeatureManager::selectPureGradient),
                               PureGradientSelectorSignature>::value,
                  "pure-gradient feature selector must not accept GT or trajectory inputs");
    const std::vector<double> reference{1,2,4,8,16};
    std::vector<double> brightened;for(double x:reference)brightened.push_back(3*x+11);
    require(std::abs(CoinFeatureManager::normalizedCrossCorrelation(reference,brightened)-1.)<1e-12,
            "NCC must be invariant to positive affine brightness changes");
    const std::vector<double> monotonic{1,2,3,4,5};
    std::vector<double> reversed=monotonic;std::reverse(reversed.begin(),reversed.end());
    require(CoinFeatureManager::normalizedCrossCorrelation(monotonic,reversed)<-.999999,
            "NCC must reject a reversed contrast pattern");
    require(std::isnan(CoinFeatureManager::normalizedCrossCorrelation({2,2,2},{5,5,5})),
            "constant patches must remain undefined under official NCC semantics");

    Eigen::MatrixXd H=Eigen::MatrixXd::Zero(15,3);int row=0;
    for(int i=0;i<6;++i)H(row++,0)=1;
    for(int i=0;i<5;++i)H(row++,1)=1;
    for(int i=0;i<4;++i)H(row++,2)=1;
    const Eigen::Matrix3d R=Eigen::AngleAxisd(.63,Eigen::Vector3d::UnitY()).toRotationMatrix();
    const auto weak=CoinFeatureManager::weakDirectionsFromGeometry(H,R,5.);
    require((weak.contribution-Eigen::Vector3d(4,5,6)).norm()<1e-12,
            "COIN contribution counts must follow ascending-eigenvalue vector order");
    require(weak.global.size()==1&&std::abs(weak.global[0].dot(Eigen::Vector3d::UnitZ()))>1.-1e-12,
            "only the weak z translation direction should be retained");
    require(weak.lidar.size()==1&&std::abs(weak.lidar[0].dot(R.transpose()*Eigen::Vector3d::UnitZ()))>1.-1e-12,
            "weak global direction must be rotated into LiDAR coordinates");
    require((weak.eigenvalues-Eigen::Vector3d(4.,5.,6.)).norm()<1e-12&&weak.geometry_rows==15,
            "geometry audit must retain ascending Super translation eigenvalues and row count");
    const auto gate=SuperDegeneracyGate::measure(weak,Eigen::Vector3d::UnitZ(),true);
    const auto gate_sign_flipped=SuperDegeneracyGate::measure(weak,-Eigen::Vector3d::UnitZ(),true);
    require(gate.valid&&gate.weakest_axis_stability==1.&&gate.confidence>0.,
            "a distinct, anisotropic and temporally aligned weakest axis must have confidence");
    require(std::abs(gate.confidence-gate_sign_flipped.confidence)<1e-15,
            "weakest-axis temporal confidence must be invariant to eigenvector sign");
    require(SuperDegeneracyGate::activate(gate,gate.confidence)&&
            !SuperDegeneracyGate::activate(gate,gate.confidence+1e-9),
            "geometry-only gate activation must be deterministic at its frozen threshold");
    const auto first_gate=SuperDegeneracyGate::measure(weak,Eigen::Vector3d::UnitX(),false);
    require(first_gate.weakest_axis_stability==0.&&first_gate.confidence==0.,
            "the first frame without temporal support must not claim confident stability");
    static_assert(std::is_abstract<cube::IntensityRepresentation>::value,
                  "future intensity projection contract must remain an interface");
    static_assert(std::is_base_of<cube::IntensityRepresentation,CoinIntensityRepresentation>::value,
                  "COIN must have a documented opt-in representation adapter");
    static_assert(!std::is_abstract<CoinIntensityRepresentation>::value,
                  "COIN adapter must implement projection, samples, residual and validity checks");

    // Exercise the production representation seam, including filled-pixel
    // landmark ownership and the identity transform that prevents double deskew.
    OusterMetadata metadata;metadata.rows=128;metadata.cols=1024;
    metadata.pixel_shift_by_row.assign(128,0);
    for(int i=0;i<128;++i)metadata.beam_altitude_degrees.push_back(22.5-45.*i/127.);
    CoinOusterProjector native_projector(metadata),packed_projector(metadata,96);
    const Vec3 test_point(20.,2.,1.);
    const auto projected=packed_projector.project(test_point);
    const auto jacobian=packed_projector.projectionJacobian(test_point);
    for(int k=0;k<3;++k){
      Vec3 plus=test_point,minus=test_point;plus[k]+=1e-5;minus[k]-=1e-5;
      const Vec2 numerical=(packed_projector.project(plus).uv-packed_projector.project(minus).uv)/2e-5;
      require((numerical-jacobian.col(k)).norm()<1e-7,"packed cube Jacobian must differentiate pixel coordinates");
    }
    require(projected.in_fov&&!packed_projector.project(Vec3(1.,1.,0.)).in_fov,
            "cube seam must not be sampled across charts");
    CoinFrame calibrated;calibrated.intensity=cv::Mat(128,1024,CV_32F,cv::Scalar(100.));
    std::vector<CoinScanPoint> cube_points;
    for(int v=30;v<=65;++v)for(int u=30;u<=65;++u){
      if(u==47&&v==47)continue;
      CoinScanPoint point;point.point_lidar=Vec3(20.,20.*(2.*u/95.-1.),20.*(2.*v/95.-1.));
      point.raw_index=cube_points.size();point.range=point.point_lidar.norm();cube_points.push_back(point);
    }
    cube::Settings cube_cfg;cube_cfg.build_igm=false;
    cube::CubeImage cube_image(cube_cfg,cube::MeasurementChannel::RawIntensity);
    CoinImageSettings image_settings;image_settings.masks.clear();
    const auto packed=buildCoinCubemap(cube_image,calibrated,native_projector,image_settings,cube_points);
    const int filled_index=packed.image_index.at<int>(47,47);
    require(filled_index>=0&&static_cast<std::size_t>(filled_index)<cube_points.size(),
            "IDW patch pixel must own a private depth-derived intensity landmark");
    const auto reprojection=packed_projector.project(cube_points[filled_index].point_lidar);
    require((reprojection.uv-Vec2(47.,47.)).norm()<1e-10,"filled landmark must reproject to its reference pixel");
    require(packed.mask.at<uchar>(47,47)&&!packed.mask.at<uchar>(47,95),
            "patch mask must preserve a supported interior and exclude chart boundaries");
    require(packed.T_Li_Lk_vec.size()==1&&packed.T_Li_Lk_vec[0].isIdentity()&&
            packed.vec_idx.size()==cube_points.size(),"end-frame representation must use identity acquisition transform");
    CoinFeatureManager packed_manager(packed_projector);
    Vec3 acquisition;Vec2 uv;int index=-1;
    require(packed_manager.projectUndistorted(packed,cube_points,cube_points[filled_index].point_lidar,
                                             acquisition,uv,index,true)&&
            (acquisition-cube_points[filled_index].point_lidar).norm()==0.,
            "cubemap matching must never deskew the end-frame point twice");
    const Eigen::Matrix4d identity=Eigen::Matrix4d::Identity();
    const auto photo_row=CoinPhotometricModel::linearize(packed_manager,packed_projector,packed,cube_points,
        identity,identity,cube_points[filled_index].point_lidar,100.,.7,30.,10);
    require(photo_row.valid&&photo_row.residual==0.&&photo_row.correction_jacobian_super.allFinite(),
            "cubemap patch must enter the shared COIN residual and Jacobian path");
    std::cout<<"{\"packed_cube_jacobian\":\"PASS\",\"idw_landmark_ownership\":\"PASS\","
               "\"chart_boundary_mask\":\"PASS\",\"no_double_deskew\":\"PASS\",\"shared_coin_residual\":\"PASS\"}\n";

    Eigen::MatrixXd duplicated(2*H.rows(),3);
    duplicated.topRows(H.rows())=H;duplicated.bottomRows(H.rows())=H;
    const auto duplicated_weak=CoinFeatureManager::weakDirectionsFromGeometry(duplicated,R,5.);
    require((duplicated_weak.contribution-2.*weak.contribution).norm()<1e-12,
            "official absolute contributions must double when every geometry row is duplicated");
    const Eigen::Vector3d normalized=weak.contribution/static_cast<double>(H.rows());
    const Eigen::Vector3d normalized_duplicate=duplicated_weak.contribution/
        static_cast<double>(duplicated.rows());
    require((normalized-normalized_duplicate).norm()<1e-12,
            "row-count-normalized contributions must be invariant to duplicate rows");
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eig(H.transpose()*H);
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eig_duplicate(duplicated.transpose()*duplicated);
    require((eig.eigenvalues()/eig.eigenvalues().maxCoeff()-
             eig_duplicate.eigenvalues()/eig_duplicate.eigenvalues().maxCoeff()).norm()<1e-12,
            "eigenvalue ratios must be invariant to duplicate rows");
    for(int d=0;d<3;++d){
      const Eigen::Vector3d a=eig.eigenvectors().col(d),b=eig_duplicate.eigenvectors().col(d);
      require(std::min((a-b).norm(),(a+b).norm())<1e-12,
              "weakest-eigenvector orientation must be deterministic up to sign");
    }

    const std::vector<std::pair<double,cv::Point>> gradient_candidates{
      {5.,cv::Point(2,1)},{7.,cv::Point(4,4)},{5.,cv::Point(1,2)},{6.,cv::Point(3,0)}};
    const auto gradient_selected=CoinFeatureManager::selectPureGradient(gradient_candidates,3);
    const auto gradient_repeat=CoinFeatureManager::selectPureGradient(gradient_candidates,3);
    require(gradient_selected.size()==3&&gradient_repeat.size()==gradient_selected.size(),
            "pure-gradient selector must honor the feature cap");
    require(gradient_selected[0]==cv::Point(4,4)&&gradient_selected[1]==cv::Point(3,0)&&
            gradient_selected[2]==cv::Point(2,1),
            "pure-gradient selector must rank response and break ties by image row/column");
    for(std::size_t i=0;i<gradient_selected.size();++i)
      require(gradient_repeat[i]==gradient_selected[i],"pure-gradient candidate ordering must repeat exactly");
    std::vector<std::pair<double,cv::Point>> sixty_candidates;
    for(int i=0;i<100;++i)sixty_candidates.emplace_back(100.-i,cv::Point(i%20,i/20));
    require(CoinFeatureManager::selectPureGradient(sixty_candidates,60).size()==60,
            "production selector must respect COIN's 60-center cap");

    Eigen::MatrixXd short_H=Eigen::MatrixXd::Zero(3,3);
    const auto fallback=CoinFeatureManager::weakDirectionsFromGeometry(short_H,R,25.);
    require(fallback.global.empty()&&fallback.lidar.size()==3,
            "COIN must use LiDAR XYZ when geometry has no weak direction set");
    std::cout<<"{\"ncc_affine\":\"PASS\",\"ncc_contrast\":\"PASS\",\"weak_direction_contribution\":\"PASS\",\"row_count_scaling\":\"PASS\",\"weakest_eigenvector_sign\":\"PASS\",\"super_degeneracy_gate\":\"PASS\",\"intensity_representation_interface\":\"PASS\",\"pure_gradient_order_and_60_cap\":\"PASS\",\"no_gt_selector_input\":\"PASS\",\"frame_rotation\":\"PASS\",\"fallback_axes\":\"PASS\"}\n";
    return 0;
  }catch(const std::exception& e){std::cerr<<"COIN feature math test: "<<e.what()<<'\n';return 1;}
}
