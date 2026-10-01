// Determinism and information semantics fixture, GPL-3.0-or-later.
#include "intensity/information_budget.hpp"
#include <Eigen/Eigenvalues>
#include <tbb/task_scheduler_init.h>
#include <random>
#include <iostream>
#include <iomanip>
#include <limits>
using namespace cube;
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){
  try{
    tbb::task_scheduler_init threads(4);std::mt19937 rng(20261002);
    std::normal_distribution<double> normal;std::vector<ResidualContribution> fixture;
    for(size_t i=0;i<1200;++i){
      ResidualContribution r;r.id=1199-i;r.face=int(i%6);
      r.uv<<3+3*int((i/6)%25),3+3*int((i/150)%8);
      r.response=double(i%17);r.residual=normal(rng);r.weight=.2+std::abs(normal(rng));
      for(int k=0;k<6;++k)r.jacobian[k]=normal(rng);
      fixture.push_back(r);
    }
    double worst_A=0,worst_b=0;auto raw=informationBudget(fixture,InformationPolicy::C0,2,false);
    for(size_t budget:{60,100})for(size_t n:{0,1,9,32,60,100,701,1200}){
      double alpha=budgetAlpha(n,budget);require(alpha<=1&&alpha>0,"no sparse boost");
      require(alpha==(n<=budget?1.:double(budget)/n),"budget alpha formula");
    }
    for(auto p:{InformationPolicy::C0,InformationPolicy::C60,InformationPolicy::C100,InformationPolicy::K100}){
      auto serial=informationBudget(fixture,p,2,false);
      for(int repeat=0;repeat<20;++repeat){
        auto parallel=informationBudget(fixture,p,2,true);
        require(serial.selected_ids==parallel.selected_ids,"parallel selected ID determinism");
        double ae=(serial.A-parallel.A).norm()/std::max(1.,serial.A.norm());
        double be=(serial.b-parallel.b).norm()/std::max(1.,serial.b.norm());
        worst_A=std::max(worst_A,ae);worst_b=std::max(worst_b,be);
        require(ae==0&&be==0,"fixed-block serial/parallel arithmetic parity");
      }
      Eigen::SelfAdjointEigenSolver<Mat6> ep(serial.A),ed(raw.A-serial.A);
      require(ep.eigenvalues()[0]>=-1e-10*std::max(1.,serial.A.trace()),"photo PSD");
      require(ed.eigenvalues()[0]>=-1e-10*std::max(1.,raw.A.trace()),"authority cannot increase");
      require(serial.A.allFinite()&&serial.b.allFinite(),"finite information");
      if(p==InformationPolicy::C60||p==InformationPolicy::C100){
        require((serial.A-serial.alpha*raw.A).norm()==0,"A scaled correctly");
        require((serial.b-serial.alpha*raw.b).norm()==0,"b scaled identically");
        require(serial.selected_ids==raw.selected_ids,"normalization keeps all residuals");
      }
      if(p==InformationPolicy::K100){
        require(serial.selected_ids.size()==100,"K100 exact selected count");
        std::vector<size_t> expected;
        for(const auto& r:fixture)expected.push_back(r.id);
        std::sort(expected.begin(),expected.end(),[&](size_t a,size_t b){
          const auto& ra=fixture[1199-a];const auto& rb=fixture[1199-b];
          return ra.response==rb.response?a<b:ra.response>rb.response;
        });expected.resize(100);
        require(expected==serial.selected_ids,"K100 response order/tie IDs");
        // Coincident candidates really are suppressed; exact count remains <=100.
        auto overlapped=fixture;for(auto& r:overlapped){r.face=0;r.uv.setConstant(20);}
        auto selected=informationBudget(overlapped,p,2,true);
        require(selected.selected_ids.size()==1,"K100 current-image spatial suppression");
      }
    }
    for(size_t n:{0,1,9,32,60}){
      auto sparse=std::vector<ResidualContribution>(fixture.begin(),fixture.begin()+n);
      auto base=informationBudget(sparse,InformationPolicy::C0);
      for(auto p:{InformationPolicy::C60,InformationPolicy::C100}){
        auto scaled=informationBudget(sparse,p);require(scaled.alpha==1,"sparse alpha=1");
        require((base.A-scaled.A).norm()==0&&(base.b-scaled.b).norm()==0,"sparse information unchanged");
      }
    }
    for(double bad:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-1.}){
      auto invalid=fixture;invalid[3].weight=bad;bool rejected=false;
      try{informationBudget(invalid,InformationPolicy::C60);}catch(const std::invalid_argument&){rejected=true;}
      require(rejected,"NaN/Inf/negative weights rejected");
    }
    Mat6 covariance=.001*Mat6::Identity(),geometry=Mat6::Identity();Vec6 bg=Vec6::Ones(),delta=Vec6::Zero();
    auto stats=auditInformation(geometry,bg,covariance,delta,fixture,raw,96);
    require(stats["occupied_faces"]==6&&stats["occupied_cells"]>0,"support grid counts");
    require(stats["top10_cell_trace_fraction"]>0&&stats["top10_cell_trace_fraction"]<=1,"support trace concentration");
    for(const auto& kv:stats)require(std::isfinite(kv.second),"finite audit metrics");
    auto empty=informationBudget({},InformationPolicy::C0);
    auto zero=auditInformation(geometry,bg,covariance,delta,{},empty,96);
    require(zero["d_photo_translation"]==0&&zero["d_photo_rotation"]==0,"empty photo has zero predicted influence");
    std::cout<<std::setprecision(17)<<"{\"fixture_rows\":1200,\"repeats_per_policy\":20,\"policies\":[\"C0\",\"C60\",\"C100\",\"K100\"],\"max_relative_A_error\":"<<worst_A
      <<",\"max_relative_b_error\":"<<worst_b<<",\"selected_ids_exact\":true,\"budget_scaling\":\"PASS\",\"no_sparse_boost\":\"PASS\",\"k100_order_and_suppression\":\"PASS\",\"nan_inf_psd\":\"PASS\"}\n";
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
