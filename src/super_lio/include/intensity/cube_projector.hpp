// CUBE-LIO independent implementation, GPL-3.0-or-later.
#pragma once
#include <Eigen/Core>
#include <array>
#include <cmath>
#include <stdexcept>

namespace cube {
using Vec3 = Eigen::Vector3d;
using Mat3 = Eigen::Matrix3d;
using Row6 = Eigen::Matrix<double,1,6>;
using Mat6 = Eigen::Matrix<double,6,6>;
using Vec6 = Eigen::Matrix<double,6,1>;
inline Mat3 hat(const Vec3& v) {
  Mat3 a; a << 0,-v.z(),v.y(),v.z(),0,-v.x(),-v.y(),v.x(),0; return a;
}
struct Projection {
  int face = -1;
  Eigen::Vector2d uv = Eigen::Vector2d::Zero();
  Eigen::Matrix<double,2,3> jacobian = Eigen::Matrix<double,2,3>::Zero();
  bool seam = false;
};
class CubeProjector {
 public:
  explicit CubeProjector(int n) : n_(n), f_((n-1)*.5) {
    if(n<8) throw std::invalid_argument("cubemap resolution <8");
  }
  int resolution() const { return n_; }
  // Face id: +X,-X,+Y,-Y,+Z,-Z. Ties choose X before Y before Z.
  Projection project(const Vec3& p) const {
    Projection out;
    if(!p.allFinite() || p.squaredNorm()<1e-16) return out;
    const Vec3 abs=p.cwiseAbs();
    int axis=0;
    if(abs.y()>abs.x()) axis=1;
    if(abs.z()>abs[axis]) axis=2;
    out.face=2*axis+(p[axis]<0);
    Vec3 d=Vec3::Zero(),a=Vec3::Zero(),b=Vec3::Zero();
    d[axis]=p[axis]<0?-1.:1.;
    // Local image axes form a consistent right-handed face frame.
    if(axis==0) {a.y()=d[axis];b.z()=1.;}
    if(axis==1) {a.x()=-d[axis];b.z()=1.;}
    if(axis==2) {a.x()=1.;b.y()=d[axis];}
    double depth=d.dot(p),x=a.dot(p),y=b.dot(p);
    out.uv << f_*(x/depth+1),f_*(y/depth+1);
    out.jacobian.row(0)=f_*(a/depth-x*d/(depth*depth)).transpose();
    out.jacobian.row(1)=f_*(b/depth-y*d/(depth*depth)).transpose();
    double second=0.;for(int k=0;k<3;++k)if(k!=axis)second=std::max(second,abs[k]);
    out.seam=(abs[axis]-second)<=1e-8*abs[axis];
    return out;
  }
 private:
  int n_;
  double f_;
};
} // namespace cube
