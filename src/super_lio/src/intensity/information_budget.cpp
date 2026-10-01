// Original GPLv3 implementation. Budgets are reference-derived ablations,
// not a claim about unpublished CUBE weighting.
#include "intensity/information_budget.hpp"
#include <Eigen/Eigenvalues>
#include <Eigen/Cholesky>
#include <tbb/parallel_for.h>
#include <algorithm>
#include <numeric>
#include <set>
#include <stdexcept>
#include <array>

namespace cube {
InformationPolicy parsePolicy(const std::string& s){
  if(s=="C0")return InformationPolicy::C0;
  if(s=="C60")return InformationPolicy::C60;
  if(s=="C100")return InformationPolicy::C100;
  if(s=="K100")return InformationPolicy::K100;
  throw std::invalid_argument("unknown information policy: "+s);
}
std::string policyName(InformationPolicy p){
  switch(p){case InformationPolicy::C0:return "C0";case InformationPolicy::C60:return "C60";
    case InformationPolicy::C100:return "C100";default:return "K100";}
}
double budgetAlpha(size_t n,size_t budget){
  if(budget==0)throw std::invalid_argument("zero information budget");
  return n<=budget?1.:double(budget)/double(n);
}
BudgetTerms informationBudget(const std::vector<ResidualContribution>& rows,
                             InformationPolicy policy,int suppression_radius,bool parallel){
  if(suppression_radius<0)throw std::invalid_argument("negative suppression radius");
  BudgetTerms out;std::vector<size_t> indices(rows.size());std::iota(indices.begin(),indices.end(),0);
  std::set<size_t> ids;
  for(const auto& r:rows){
    if(!r.jacobian.allFinite()||!r.uv.allFinite()||!std::isfinite(r.weight)||r.weight<0||
       !std::isfinite(r.residual)||!std::isfinite(r.response)||r.face<0||r.face>5||!ids.insert(r.id).second)
      throw std::invalid_argument("invalid/duplicate photometric contribution");
  }
  if(policy==InformationPolicy::K100){
    std::sort(indices.begin(),indices.end(),[&](size_t a,size_t b){
      if(rows[a].response==rows[b].response)return rows[a].id<rows[b].id;
      return rows[a].response>rows[b].response;
    });
    std::vector<size_t> selected;
    for(size_t i:indices){
      bool suppressed=false;
      for(size_t j:selected){
        if(rows[i].face==rows[j].face&&
           std::abs(int(rows[i].uv.x())-int(rows[j].uv.x()))<=suppression_radius&&
           std::abs(int(rows[i].uv.y())-int(rows[j].uv.y()))<=suppression_radius){suppressed=true;break;}
      }
      if(!suppressed)selected.push_back(i);
      if(selected.size()==100)break;
    }
    indices=std::move(selected);
  }
  out.selected_ids.reserve(indices.size());for(size_t i:indices)out.selected_ids.push_back(rows[i].id);
  struct Block {Mat6 A=Mat6::Zero();Vec6 b=Vec6::Zero();};
  const int count=int((indices.size()+63)/64);std::vector<Block> blocks(count);
  auto compute=[&](int chunk){
    auto& b=blocks[chunk];
    for(size_t k=size_t(chunk)*64;k<std::min(indices.size(),size_t(chunk+1)*64);++k){
      const auto& r=rows[indices[k]];
      b.A.noalias()+=r.weight*r.jacobian.transpose()*r.jacobian;
      b.b.noalias()-=r.weight*r.jacobian.transpose()*r.residual;
    }
  };
  if(parallel)tbb::parallel_for(0,count,compute);else for(int i=0;i<count;++i)compute(i);
  for(const auto& b:blocks){out.A+=b.A;out.b+=b.b;}
  if(policy==InformationPolicy::C60)out.alpha=budgetAlpha(rows.size(),60);
  if(policy==InformationPolicy::C100)out.alpha=budgetAlpha(rows.size(),100);
  out.A*=out.alpha;out.b*=out.alpha;
  return out;
}
std::map<std::string,double> auditInformation(const Mat6& geometry,const Vec6& geometry_b,
    const Mat6& prior_covariance,const Vec6& corrected_prior_delta,
    const std::vector<ResidualContribution>& rows,const BudgetTerms& photo,int resolution,int coarse_grid){
  if(resolution<8||coarse_grid<1)throw std::invalid_argument("invalid audit grid");
  std::map<std::string,double> m;
  const Mat6 Ag=.5*(geometry+geometry.transpose()),Ap=.5*(photo.A+photo.A.transpose());
  Eigen::SelfAdjointEigenSolver<Mat6> eg6(Ag),ep6(Ap);
  Eigen::SelfAdjointEigenSolver<Mat3> eg3(Ag.bottomRightCorner<3,3>()),ep3(Ap.bottomRightCorner<3,3>());
  m["trace_Ag"]=Ag.trace();m["trace_Ap"]=Ap.trace();m["trace_ratio"]=Ap.trace()/std::max(Ag.trace(),1e-9);
  m["n_valid"]=double(rows.size());m["n_used"]=double(photo.selected_ids.size());m["alpha"]=photo.alpha;
  for(int k=0;k<6;++k){m["geo_eig"+std::to_string(k)]=eg6.eigenvalues()[k];m["photo_eig"+std::to_string(k)]=ep6.eigenvalues()[k];}
  for(int k=0;k<3;++k){
    const Vec3 v=eg3.eigenvectors().col(k);double qg=v.dot(Ag.bottomRightCorner<3,3>()*v),qp=v.dot(Ap.bottomRightCorner<3,3>()*v);
    const auto s=std::to_string(k);m["geo_t_eig"+s]=eg3.eigenvalues()[k];m["photo_t_eig"+s]=ep3.eigenvalues()[k];
    m["qg"+s]=qg;m["qp"+s]=qp;m["ratio"+s]=qp/std::max(qg,1e-9);
    m["sg"+s]=v.dot(geometry_b.tail<3>());m["sp"+s]=v.dot(photo.b.tail<3>());
    for(int j=0;j<3;++j)m["v"+s+"_"+std::to_string(j)]=v[j];
  }
  Mat6 covariance=.5*(prior_covariance+prior_covariance.transpose());
  Eigen::SelfAdjointEigenSolver<Mat6> pc(covariance);
  double jitter=std::max(0.,1e-12-pc.eigenvalues()[0]);covariance.diagonal().array()+=jitter;
  const Mat6 prior=covariance.ldlt().solve(Mat6::Identity());
  const Vec6 rhs=geometry_b-prior*corrected_prior_delta;
  const Vec6 d_geo=(prior+Ag).ldlt().solve(rhs);
  const Vec6 d_joint=(prior+Ag+Ap).ldlt().solve(rhs+photo.b);
  const Vec6 d_photo=d_joint-d_geo;
  m["prior_jitter"]=jitter;m["d_photo_translation"]=d_photo.tail<3>().norm();
  m["d_photo_rotation"]=d_photo.head<3>().norm();
  m["d_geo_translation"]=d_geo.tail<3>().norm();m["d_joint_translation"]=d_joint.tail<3>().norm();
  for(int k=0;k<6;++k){const auto s=std::to_string(k);m["d_geo"+s]=d_geo[k];m["d_joint"+s]=d_joint[k];m["d_photo"+s]=d_photo[k];}
  std::array<int,6> face_counts{};std::map<int,std::pair<int,double>> cells;
  std::set<size_t> selected(photo.selected_ids.begin(),photo.selected_ids.end());
  std::vector<double> residuals;double sum2=0;
  for(const auto& r:rows){
    residuals.push_back(r.residual);sum2+=r.residual*r.residual;
    if(!selected.count(r.id))continue;
    ++face_counts[r.face];
    int x=std::clamp(int(r.uv.x()*coarse_grid/resolution),0,coarse_grid-1);
    int y=std::clamp(int(r.uv.y()*coarse_grid/resolution),0,coarse_grid-1);
    auto& cell=cells[r.face*coarse_grid*coarse_grid+y*coarse_grid+x];
    ++cell.first;cell.second+=photo.alpha*r.weight*r.jacobian.squaredNorm();
  }
  auto median=[](std::vector<double> v){if(v.empty())return 0.;auto it=v.begin()+v.size()/2;std::nth_element(v.begin(),it,v.end());return *it;};
  const double center=median(residuals);std::vector<double> deviations;
  for(double r:residuals)deviations.push_back(std::abs(r-center));
  m["residual_median"]=center;m["residual_mad"]=median(deviations);
  m["residual_rms"]=rows.empty()?0.:std::sqrt(sum2/rows.size());
  int occupied_faces=0;for(int f=0;f<6;++f){m["face"+std::to_string(f)]=face_counts[f];occupied_faces+=face_counts[f]>0;}
  m["occupied_faces"]=occupied_faces;m["occupied_cells"]=double(cells.size());
  m["residuals_per_cell"]=cells.empty()?0.:double(photo.selected_ids.size())/cells.size();
  int max_cell_count=0;std::vector<double> traces;
  for(const auto& c:cells){max_cell_count=std::max(max_cell_count,c.second.first);traces.push_back(c.second.second);}
  std::sort(traces.begin(),traces.end(),std::greater<double>());
  const size_t top=(traces.size()+9)/10;double top_sum=std::accumulate(traces.begin(),traces.begin()+top,0.);
  const double all=std::accumulate(traces.begin(),traces.end(),0.);
  m["max_cell_residuals"]=max_cell_count;m["top10_cell_trace_fraction"]=all>0?top_sum/all:0.;
  m["psd_min"]=ep6.eigenvalues()[0];
  for(const auto& kv:m)if(!std::isfinite(kv.second))throw std::runtime_error("nonfinite photo audit metric: "+kv.first);
  if(ep6.eigenvalues()[0]<-1e-10*std::max(1.,Ap.trace()))throw std::runtime_error("negative photo information");
  return m;
}
} // namespace cube
