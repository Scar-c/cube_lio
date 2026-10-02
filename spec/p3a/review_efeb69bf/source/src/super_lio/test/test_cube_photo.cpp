// Independent GPLv3 validation: actual projector/image interpolation/pose chain.
#include "intensity/cube_image.hpp"
#include <Eigen/Geometry>
#include <random>
#include <iostream>
#include <stdexcept>
#include <iomanip>
using namespace cube;
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
Vec3 ray(int face,double a,double b){
  switch(face){case 0:return {1,a,b};case 1:return {-1,-a,b};
    case 2:return {-a,1,b};case 3:return {a,-1,b};
    case 4:return {a,b,1};default:return {a,-b,-1};}
}
int main(){
  try {
    CubeProjector projector(64);std::mt19937 rng(20261001);
    std::uniform_real_distribution<double> uniform(-.7,.7),depth(3,30);
    double worst_projection=0,worst_residual=0;
    for(int face=0;face<6;++face){
      require(projector.project(ray(face,0,0)).face==face,"six-face center selection");
      for(int n=0;n<1000;++n){
        Vec3 p=depth(rng)*ray(face,uniform(rng),uniform(rng));auto q=projector.project(p);
        require(q.face==face&&!q.seam,"random interior face");
        Eigen::Matrix<double,2,3> fd;
        for(int k=0;k<3;++k){Vec3 plus=p,minus=p;plus[k]+=1e-6;minus[k]-=1e-6;
          fd.col(k)=(projector.project(plus).uv-projector.project(minus).uv)/(2e-6);}
        worst_projection=std::max(worst_projection,(fd-q.jacobian).cwiseAbs().maxCoeff());
      }
    }
    require(worst_projection<2e-7,"projection Jacobian error");
    require(projector.project(Vec3::Zero()).face==-1,"zero point invalid");
    require(projector.project({NAN,1,1}).face==-1,"NaN point invalid");
    // Exact ties and tiny one-sided perturbations around every signed seam.
    for(int a=0;a<3;++a)for(int b=a+1;b<3;++b)for(int sa:{-1,1})for(int sb:{-1,1}){
      Vec3 p=Vec3::Zero();p[a]=sa;p[b]=sb;
      auto q=projector.project(p);require(q.face==2*a+(sa<0)&&q.seam,"deterministic seam tie");
      Vec3 left=p,right=p;left[a]*=1.+1e-5;right[b]*=1.+1e-5;
      require(projector.project(left).face==2*a+(sa<0)&&projector.project(right).face==2*b+(sb<0),"seam one-sided faces");
    }
    Settings cfg;cfg.resolution=64;cfg.idw_enable=false;CubeImage image(cfg);
    std::vector<ScanPoint> points;
    for(int face=0;face<6;++face)for(int v=0;v<64;++v)for(int u=0;u<64;++u){
      Vec3 p=ray(face,u/31.5-1.,v/31.5-1.);p=10.*p.normalized();
      points.push_back({p,250+20*std::sin(.14*u)+15*std::cos(.2*v)+.05*u*v});
    }
    image.build(points);
    Mat3 R=(Eigen::AngleAxisd(.3,Vec3::UnitZ())*Eigen::AngleAxisd(-.2,Vec3::UnitY())).toRotationMatrix();
    Vec3 t(1,-2,.5);Mat3 extr=Eigen::AngleAxisd(.1,Vec3::UnitX()).toRotationMatrix();Vec3 et(.04,-.03,.07);
    auto residual=[&](const Vec3& world,const Mat3& rotation,const Vec3& position,double ref){
      Vec3 p=landmarkInLidar(world,rotation,position,extr,et);Sample s;
      require(image.sample(image.projector.project(p),s),"residual sample validity");return s.value-ref;
    };
    int count=0;
    for(int face=0;face<6;++face)for(int j=0;j<200;++j){
      double a=uniform(rng),b=uniform(rng);
      double u=31.5*(a+1),v=31.5*(b+1);
      if(std::abs(u-std::round(u))<.02||std::abs(v-std::round(v))<.02)continue;
      Vec3 p=10.*ray(face,a,b).normalized(),world=R*(extr*p+et)+t;
      auto q=image.projector.project(p);Sample s;require(image.sample(q,s),"interior sample");
      Row6 analytic=residualJacobian(world,R,t,extr,et,q,s),fd;
      const double h=1e-6,reference=s.value+3.;
      for(int k=0;k<6;++k){
        Mat3 rp=R,rm=R;Vec3 tp=t,tm=t;
        if(k<3){rp=R*Eigen::AngleAxisd(h,Vec3::Unit(k)).toRotationMatrix();rm=R*Eigen::AngleAxisd(-h,Vec3::Unit(k)).toRotationMatrix();}
        else {tp[k-3]+=h;tm[k-3]-=h;}
        fd[k]=(residual(world,rp,tp,reference)-residual(world,rm,tm,reference))/(2*h);
      }
      worst_residual=std::max(worst_residual,(fd-analytic).cwiseAbs().maxCoeff());++count;
      // Information sign must be a descent step for this residual.
      Vec6 step=-analytic.transpose()*(s.value-reference)*1e-6;
      Mat3 moved=R;
      if(step.head<3>().norm()>0)moved=R*Eigen::AngleAxisd(step.head<3>().norm(),step.head<3>().normalized()).toRotationMatrix();
      double after=residual(world,moved,t+step.tail<3>(),reference);
      require(std::abs(after)<std::abs(s.value-reference),"information-form sign must descend");
    }
    require(worst_residual<2e-6,"full 6DoF residual Jacobian error");
    Sample sample;require(!image.sample(projector.project({1,1,0}),sample),"seam residual is explicitly rejected");
    // Empty masks never create intensity constraints.
    image.build({});require(!image.sample(projector.project({1,0,0}),sample),"invalid mask rejection");
    // IDW cannot fill a competing foreground/background layer.
    cfg.idw_enable=true;cfg.idw_radius=3;cfg.idw_k=6;cfg.idw_min_support=3;
    CubeImage sparse(cfg);std::vector<ScanPoint> sparse_points;
    for(int u:{30,32})for(int v:{30,32}){
      Vec3 p=ray(0,u/31.5-1.,v/31.5-1.).normalized();
      sparse_points.push_back({p*(u==30?5.:15.),double(u)});
    }
    sparse.build(sparse_points);require(!sparse.face(0).mask.at<uint8_t>(31,31),"IDW depth discontinuity rejection");
    for(auto& p:sparse_points)p.p=5.*p.p.normalized();
    sparse.build(sparse_points);require(sparse.face(0).mask.at<uint8_t>(31,31),"IDW same-surface interpolation");
    std::cout<<std::setprecision(12)<<"{\"projection_points\":6000,\"projection_max_abs_error\":"<<worst_projection
      <<",\"residual_points\":"<<count<<",\"residual_max_abs_error\":"<<worst_residual
      <<",\"all_six_dof_and_faces\":true,\"boundary_tests\":\"PASS\",\"idw_visibility\":\"PASS\",\"information_sign\":\"PASS\"}\n";
    return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
