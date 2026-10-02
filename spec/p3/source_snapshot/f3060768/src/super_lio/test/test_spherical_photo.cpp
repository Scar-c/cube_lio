#include "intensity/spherical_image.hpp"
#include <Eigen/Geometry>
#include <iostream>
#include <random>
#include <stdexcept>

using namespace cube;
namespace {
constexpr double kPi=3.141592653589793238462643383279502884;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Vec3 ray(double longitude,double latitude){
  return {std::cos(latitude)*std::cos(longitude),
          std::cos(latitude)*std::sin(longitude),std::sin(latitude)};
}
}

int main(){
  try{
    Settings settings;settings.resolution=64;settings.idw_enable=false;
    SphericalImage projection_only(settings,MeasurementChannel::RawIntensity);
    std::mt19937 rng(20261002);
    std::uniform_real_distribution<double> longitude(-2.8,2.8),latitude(-1.25,1.25),range(3.,30.);
    double worst_projection=0.;
    for(int i=0;i<2000;++i){
      const Vec3 p=range(rng)*ray(longitude(rng),latitude(rng));Projection q;
      require(projection_only.project(p,q)&&q.face==0,"spherical projection validity");
      Eigen::Matrix<double,2,3> fd;
      for(int axis=0;axis<3;++axis){
        Vec3 plus=p,minus=p;plus[axis]+=1e-6;minus[axis]-=1e-6;
        Projection qp,qm;require(projection_only.project(plus,qp)&&projection_only.project(minus,qm),
                                 "finite-difference projection validity");
        Eigen::Vector2d delta=qp.uv-qm.uv;
        if(std::abs(delta.x())>projection_only.width()*.5)
          delta.x()-=std::copysign(projection_only.width(),delta.x());
        fd.col(axis)=delta/2e-6;
      }
      worst_projection=std::max(worst_projection,(fd-q.jacobian).cwiseAbs().maxCoeff());
    }
    require(worst_projection<2e-7,"spherical projection Jacobian error");
    Projection pole;require(!projection_only.project({0.,0.,1.},pole),"pole singularity rejection");

    SphericalImage image(settings,MeasurementChannel::IntensityGradientMagnitude);
    std::vector<ScanPoint> points;points.reserve(image.width()*(image.height()-8));
    for(int v=3;v<image.height()-3;++v)for(int u=0;u<image.width();++u){
      const double lon=-kPi+(u+.25)*(2.*kPi/image.width());
      const double lat=.5*kPi-(v+.25)*(kPi/image.height());
      const double phase=2.*kPi*u/image.width();
      points.push_back({10.*ray(lon,lat),100.+20.*std::sin(phase)+10.*std::cos(.12*v)+.03*u*v});
    }
    image.build(points);
    require(image.rawPixelCount()>10000,"dense synthetic spherical image construction");
    require(image.validFeaturePixelCount()>5000,"spherical IGM construction");

    // Longitude is periodic: bilinear samples on both sides of the chart seam
    // remain valid instead of falling off the image boundary.
    for(double u:{.2,image.width()-.2}){
      const double lon=-kPi+u*(2.*kPi/image.width());
      const double lat=.15;Projection q;Sample s;
      require(image.project(10.*ray(lon,lat),q)&&image.validityCheck(q,10.,.3,.02,s),
              "periodic seam sample validity");
    }

    const Mat3 R=(Eigen::AngleAxisd(.21,Vec3::UnitZ())*Eigen::AngleAxisd(-.13,Vec3::UnitY())).toRotationMatrix();
    const Vec3 t(.3,-.7,.2);const Mat3 extr=Eigen::AngleAxisd(.08,Vec3::UnitX()).toRotationMatrix();
    const Vec3 et(.04,-.03,.07);double worst_residual=0.;int residual_count=0;
    for(int i=0;i<300;++i){
      const double u=20.+(i*37)%std::max(1,image.width()-40)+.37;
      const double v=8.+(i*19)%std::max(1,image.height()-16)+.41;
      const double lon=-kPi+u*(2.*kPi/image.width());
      const double lat=.5*kPi-v*(kPi/image.height());
      const Vec3 p_L=10.*ray(lon,lat),world=R*(extr*p_L+et)+t;
      Projection q;Sample sample;require(image.project(p_L,q)&&image.validityCheck(q,p_L.norm(),.3,.02,sample),
                                           "spherical residual reference sample");
      const double reference=sample.value+.7;
      auto residual=[&](const Mat3& rotation,const Vec3& position){
        const Vec3 current=landmarkInLidar(world,rotation,position,extr,et);
        Projection qp;Sample sp;double value=0.;
        require(image.project(current,qp)&&image.validityCheck(qp,current.norm(),.3,.02,sp)&&
                image.computeResidual(qp,reference,value),"spherical residual perturbation sample");
        return value;
      };
      const Row6 analytic=residualJacobian(world,R,t,extr,et,q,sample);Row6 fd;
      const double h=1e-6;
      for(int axis=0;axis<6;++axis){
        Mat3 rp=R,rm=R;Vec3 tp=t,tm=t;
        if(axis<3){
          rp=R*Eigen::AngleAxisd(h,Vec3::Unit(axis)).toRotationMatrix();
          rm=R*Eigen::AngleAxisd(-h,Vec3::Unit(axis)).toRotationMatrix();
        }else{tp[axis-3]+=h;tm[axis-3]-=h;}
        fd[axis]=(residual(rp,tp)-residual(rm,tm))/(2.*h);
      }
      worst_residual=std::max(worst_residual,(fd-analytic).cwiseAbs().maxCoeff());
      ++residual_count;
    }
    require(residual_count==300&&worst_residual<2e-5,"spherical full-pose residual Jacobian error");

    Settings sparse_settings=settings;sparse_settings.idw_enable=true;
    SphericalImage sparse(sparse_settings,MeasurementChannel::RawIntensity);
    std::vector<ScanPoint> sparse_points;
    const int center_v=sparse.height()/2;
    for(const auto offset:std::vector<std::pair<int,int>>{{-1,0},{1,0},{0,-1},{0,1}}){
      const int u=(offset.first+sparse.width())%sparse.width(),v=center_v+offset.second;
      const double lon=-kPi+(u+.2)*(2.*kPi/sparse.width());
      const double lat=.5*kPi-(v+.2)*(kPi/sparse.height());
      sparse_points.push_back({10.*ray(lon,lat),double(sparse_points.size()+1)});
    }
    sparse.build(sparse_points);
    require(sparse.chart().mask.at<uint8_t>(center_v,0)!=0,
            "IDW support wraps horizontally across the equirectangular seam");
    std::cout<<"{\"spherical_projection_points\":2000,\"projection_max_abs_error\":"
      <<worst_projection<<",\"residual_points\":"<<residual_count
      <<",\"residual_max_abs_error\":"<<worst_residual
      <<",\"seam_sampling\":\"PASS\",\"idw_seam\":\"PASS\",\"poles\":\"PASS\"}\n";
    return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
