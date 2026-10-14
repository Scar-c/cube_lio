#include "intensity/paper_cube.hpp"
#include <Eigen/Geometry>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <numeric>

using namespace cube;
namespace p4=cube::paper;
namespace {
void require(bool condition,const char* why) {if(!condition)throw std::runtime_error(why);}
Mat3 perturb(const Mat3& R,int axis,double h) {
  return R*Eigen::AngleAxisd(h,Vec3::Unit(axis)).toRotationMatrix();
}
std::vector<p4::Point> dense(const p4::Projector& p,bool linear=false) {
  std::vector<p4::Point> points;
  for(int f=0;f<6;++f)for(int v=0;v<p.resolution();++v)for(int u=0;u<p.resolution();++u) {
    const Vec3 direction=p.inverse(f,{u+.25,v+.25}).normalized();
    // Smooth spherical texture, shared across faces. This produces genuine
    // derivative-of-Gaussian IGM rather than an invented direct IGM fixture.
    const double intensity=linear?50+2*u+3*v:
      150+30*std::sin(8*direction.x())+20*std::cos(6*direction.y())+25*direction.z();
    points.push_back({10*direction,intensity});
  }
  return points;
}
double quantile(std::vector<double> x,double probability) {
  if(x.empty())return 0.;std::sort(x.begin(),x.end());
  return x[std::min(x.size()-1,size_t(probability*(x.size()-1)))];
}
void components() {
  p4::Projector projector(64);std::mt19937 rng(20261014);
  std::uniform_real_distribution<double> pixel(7,56),range(3,30);
  double projection_error=0,roundtrip_error=0,transform_error=0,seam_error=0;
  int seam_count=0,exact_boundary_open_interval_failures=0;
  for(int face=0;face<6;++face)for(int i=0;i<1000;++i) {
    Eigen::Vector2d uv(pixel(rng),pixel(rng));
    const Vec3 point=projector.reconstruct(face,uv,range(rng));const auto q=projector.project(point);
    require(q.face==face,"published face assignment");
    roundtrip_error=std::max(roundtrip_error,(projector.reconstruct(q.face,q.uv,point.norm())-point).norm());
    Eigen::Matrix<double,2,3> fd;
    for(int k=0;k<3;++k) {Vec3 a=point,b=point;a[k]+=1e-6;b[k]-=1e-6;
      fd.col(k)=(projector.project(a).uv-projector.project(b).uv)/2e-6;}
    projection_error=std::max(projection_error,(fd-q.jacobian).cwiseAbs().maxCoeff());
  }
  require(projection_error<2e-7,"Eq. (14) projection FD");
  require(roundtrip_error<1e-12,"Eq. (7)-(8) range reconstruction round trip");
  require(projector.project(Vec3::Zero()).face==-1,"zero projection rejection");
  for(int face=0;face<6;++face)for(int axis=0;axis<2;++axis)
    for(double edge:{-.2,64.2})for(double along:{.3,16.3,32.3,48.3,63.7}) {
      Eigen::Vector2d uv;uv[axis]=edge;uv[1-axis]=along;
      const auto q=projector.remap(face,uv);require(q.face>=0&&q.face!=face,"adjacent face remapping");
      const double err=(projector.inverse(face,uv).normalized()-projector.inverse(q.face,q.uv).normalized()).norm();
      seam_error=std::max(seam_error,err);++seam_count;
    }
  for(int face=0;face<6;++face)for(double u:{-.2,64.2})for(double v:{-.2,64.2}) {
    const auto q=projector.remap(face,{u,v});
    seam_error=std::max(seam_error,(projector.inverse(face,{u,v}).normalized()-projector.inverse(q.face,q.uv).normalized()).norm());
    ++seam_count;
  }
  require(seam_error<1e-12,"Eq. (15) preserves direction at every edge/corner");
  for(int face=0;face<6;++face)for(const Eigen::Vector2d uv:
      std::vector<Eigen::Vector2d>{{64,32},{32,64},{64,64}}) {
    const auto q=projector.remap(face,uv);
    if(q.uv.x()>=64||q.uv.y()>=64)++exact_boundary_open_interval_failures;
  }
  const Mat3 R=(Eigen::AngleAxisd(.31,Vec3::UnitZ())*Eigen::AngleAxisd(-.23,Vec3::UnitY())).toRotationMatrix();
  const Vec3 t(.4,-.7,.2),world(4,-3,7);const auto J=p4::inverseTransformJacobian(world,R,t);
  for(int k=0;k<6;++k) {
    Mat3 rp=R,rm=R;Vec3 tp=t,tm=t;
    if(k<3){rp=perturb(R,k,1e-6);rm=perturb(R,k,-1e-6);}else{tp[k-3]+=1e-6;tm[k-3]-=1e-6;}
    const Vec3 fd=(rp.transpose()*(world-tp)-rm.transpose()*(world-tm))/2e-6;
    transform_error=std::max(transform_error,(fd-J.col(k)).cwiseAbs().maxCoeff());
  }
  require(transform_error<2e-8,"Eq. (13) right rotation/global translation FD");
  p4::Config config;config.resolution=64;config.idw=false;p4::Frame linear(config);
  linear.build(dense(linear.projector,true));
  const auto& f=linear.face(0);double derivative_error=0;
  for(int v=10;v<54;++v)for(int u=10;u<54;++u) {
    require(f.igm_mask.at<uint8_t>(v,u),"full Gaussian derivative support");
    derivative_error=std::max(derivative_error,std::abs(f.gx.at<double>(v,u)-2));
    derivative_error=std::max(derivative_error,std::abs(f.gy.at<double>(v,u)-3));
    derivative_error=std::max(derivative_error,std::abs(f.igm.at<double>(v,u)-std::sqrt(13.)));
  }
  require(derivative_error<1e-11,"Eq. (6) sampled Gaussian derivative ramp response");
  config.idw=true;p4::Frame sparse(config);std::vector<p4::Point> points;
  for(int u:{30,32})for(int v:{30,32})points.push_back({projector.reconstruct(0,{u+.25,v+.25},5),20});
  sparse.build(points);require(sparse.face(0).mask.at<uint8_t>(31,31),"IDW hole fill");
  require(!sparse.face(0).raw_mask.at<uint8_t>(31,31),"IDW feature needs no owner");
  require(sparse.face(0).support_count.at<int>(31,31)==4,"IDW original support count");
  require(std::abs(sparse.face(0).fill_distance.at<double>(31,31)-std::sqrt(2.))<1e-12,"IDW fill distance");
  require(std::abs(sparse.face(0).range.at<double>(31,31)-5)<1e-12,"IDW plausible range");
  for(size_t i=0;i<points.size();++i)if(i%2==0)points[i].body*=3;
  sparse.build(points);require(!sparse.face(0).mask.at<uint8_t>(31,31),"two depth layers rejected");
  sparse.build({});require(sparse.statistics().valid_range_pixels==0,"empty support cannot fill");
  points={{projector.reconstruct(0,{31.25,31.25},5),20}};sparse.build(points);
  require(sparse.statistics().valid_range_pixels==1,"no recursive support growth");
  points.clear();
  for(int v:{0,2})for(int u:{0,2}) {
    const Vec3 direction=projector.inverse(0,{u+.25,v+.25}).normalized();
    points.push_back({direction*(5/direction.x()),20}); // actual x=5 plane
  }
  sparse.build(points);require(sparse.face(0).mask.at<uint8_t>(1,1),"plane boundary hole fill");
  double low=1e9,high=0;for(const auto& point:points){low=std::min(low,point.body.norm());high=std::max(high,point.body.norm());}
  require(sparse.face(0).range.at<double>(1,1)>=low&&sparse.face(0).range.at<double>(1,1)<=high,"plane IDW in support range hull");
  p4::Frame texture(config);texture.build(dense(projector));int seam_valid=0;
  for(int face=0;face<6;++face)for(const Eigen::Vector2d uv:
      std::vector<Eigen::Vector2d>{{.2,32.37},{63.8,32.37},{32.37,.2},{32.37,63.8}}) {
    Eigen::RowVector2d gradient;require(texture.gradient(face,uv,p4::Channel::Igm,gradient),"valid seam gradient not rejected");++seam_valid;
  }
  require(texture.statistics().seam_remapped_queries>0&&texture.statistics().seam_failed_queries==0,"seam samples are supported");
  std::cout<<std::setprecision(17)<<"{\"status\":\"PASS\",\"projection_points\":6000,\"projection_max_abs_error\":"<<projection_error
    <<",\"roundtrip_max_error\":"<<roundtrip_error<<",\"transform_max_abs_error\":"<<transform_error
    <<",\"remap_direction_max_error\":"<<seam_error<<",\"seam_edge_corner_cases\":"<<seam_count
    <<",\"valid_igm_seam_gradients\":"<<seam_valid<<",\"dog_ramp_max_error\":"<<derivative_error
    <<",\"idw_cases\":\"PASS\",\"literal_W_exact_boundary_halfopen_failures\":"<<exact_boundary_open_interval_failures
    <<",\"boundary_sampling_extension\":\"nearest boundary bin\"}\n";
}
int jacobians(const std::string& csv_path) {
  p4::Config config;config.resolution=64;config.idw=false;p4::Frame frame(config);
  frame.build(dense(frame.projector));
  const Mat3 R=(Eigen::AngleAxisd(.31,Vec3::UnitZ())*Eigen::AngleAxisd(-.23,Vec3::UnitY())).toRotationMatrix();
  const Vec3 t(.4,-.7,.2);std::ofstream csv(csv_path);require(bool(csv),"cannot write FD diagnostic CSV");
  csv<<std::setprecision(17)<<"face,u,v,seam_stencil,h,dof,paper_H,residual_FD,local_interpolant_H,abs_error,relative_error,accepted\n";
  std::vector<double> errors,relative;int failed=0,sign_disagreements=0,seam_cases=0,interior_cases=0;
  double exact_max=0;int samples=0;
  for(int face=0;face<6;++face) {
    std::vector<Eigen::Vector2d> pixels;
    for(double u:{10.37,20.37,30.37,40.37,50.37})for(double v:{12.41,25.41,38.41,51.41})pixels.push_back({u,v});
    for(const Eigen::Vector2d uv:std::vector<Eigen::Vector2d>{{.2,32.37},{63.8,32.37},{32.37,.2},{32.37,63.8}})pixels.push_back(uv);
    for(const auto& uv:pixels) {
      const bool seam=uv.minCoeff()<1||uv.maxCoeff()>63;seam?++seam_cases:++interior_cases;
      const Vec3 world=R*frame.projector.reconstruct(face,uv,10)+t;
      const auto row=p4::linearize(frame,world,2,R,t);require(row.valid,"reference IGM linearization");
      // Independent local image derivative diagnostic at the SAME point; this
      // is never used by the production Eq. (16) linearization.
      const Vec3 body=R.transpose()*(world-t);const auto q=frame.projector.project(body);
      Eigen::RowVector2d local;
      for(int axis=0;axis<2;++axis) {
        auto qp=q.uv,qm=q.uv;qp[axis]+=1e-5;qm[axis]-=1e-5;double vp,vm;
        require(frame.sample(q.face,qp,p4::Channel::Igm,vp)&&frame.sample(q.face,qm,p4::Channel::Igm,vm),"local image FD");
        local[axis]=(vp-vm)/2e-5;
      }
      const Row6 local_H=local*q.jacobian*p4::inverseTransformJacobian(world,R,t);
      for(double h:{1e-4,1e-5,1e-6,1e-7})for(int k=0;k<6;++k) {
        Mat3 rp=R,rm=R;Vec3 tp=t,tm=t;
        if(k<3){rp=perturb(R,k,h);rm=perturb(R,k,-h);}else{tp[k-3]+=h;tm[k-3]-=h;}
        auto residual=[&](const Mat3& rotation,const Vec3& translation) {
          const auto point=frame.projector.project(rotation.transpose()*(world-translation));double value;
          require(frame.sample(point.face,point.uv,p4::Channel::Igm,value),"perturbed scalar residual sample");return value-2;
        };
        const double fd=(residual(rp,tp)-residual(rm,tm))/(2*h),err=std::abs(fd-row.H[k]);
        const double rel=err/std::max(1e-8,std::abs(fd));const bool accepted=err<=1e-4+1e-4*std::abs(fd);
        if(h==1e-6){errors.push_back(err);relative.push_back(rel);failed+=!accepted;
          if(std::abs(fd)>1e-6&&std::abs(row.H[k])>1e-6&&fd*row.H[k]<0)++sign_disagreements;
          exact_max=std::max(exact_max,std::abs(fd-local_H[k]));++samples;}
        csv<<face<<','<<uv.x()<<','<<uv.y()<<','<<seam<<','<<h<<','<<k<<','<<row.H[k]<<','<<fd<<','<<local_H[k]<<','<<err<<','<<rel<<','<<accepted<<'\n';
      }
    }
  }
  std::cout<<std::setprecision(17)<<"{\"status\":\""<<(failed?"FAIL":"PASS")<<"\",\"components_at_h_1e6\":"<<samples
    <<",\"failed_components\":"<<failed<<",\"interior_points\":"<<interior_cases<<",\"seam_stencil_points\":"<<seam_cases
    <<",\"max_abs_error\":"<<*std::max_element(errors.begin(),errors.end())
    <<",\"relative_error_p50\":"<<quantile(relative,.5)<<",\"relative_error_p95\":"<<quantile(relative,.95)
    <<",\"sign_disagreements\":"<<sign_disagreements<<",\"local_interpolant_chain_max_abs_error\":"<<exact_max
    <<",\"acceptance_abs\":0.0001,\"acceptance_relative\":0.0001,\"h_probes\":[0.0001,0.00001,0.000001,0.0000001]}\n";
  return failed?2:0;
}
}
int main(int argc,char** argv) {
  try {
    if(argc>1&&std::string(argv[1])=="--jacobian")return jacobians(argc>2?argv[2]:"p4_jacobian.csv");
    components();return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
