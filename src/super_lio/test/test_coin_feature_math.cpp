#include "intensity/coin/coin_feature_manager.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
}

int main(){
  using namespace cube::coin;
  try{
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

    Eigen::MatrixXd short_H=Eigen::MatrixXd::Zero(3,3);
    const auto fallback=CoinFeatureManager::weakDirectionsFromGeometry(short_H,R,25.);
    require(fallback.global.empty()&&fallback.lidar.size()==3,
            "COIN must use LiDAR XYZ when geometry has no weak direction set");
    std::cout<<"{\"ncc_affine\":\"PASS\",\"ncc_contrast\":\"PASS\",\"weak_direction_contribution\":\"PASS\",\"frame_rotation\":\"PASS\",\"fallback_axes\":\"PASS\"}\n";
    return 0;
  }catch(const std::exception& e){std::cerr<<"COIN feature math test: "<<e.what()<<'\n';return 1;}
}
