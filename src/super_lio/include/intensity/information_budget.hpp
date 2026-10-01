// Original CUBE-LIO engineering ablations, GPL-3.0-or-later.
#pragma once
#include "cube_projector.hpp"
#include <vector>
#include <string>
#include <map>
#include <cstdint>

namespace cube {
enum class InformationPolicy { C0, C60, C100, K100 };
InformationPolicy parsePolicy(const std::string& name);
std::string policyName(InformationPolicy policy);
double budgetAlpha(size_t count, size_t budget);
struct ResidualContribution {
  size_t id=0;
  int face=0;
  Eigen::Vector2d uv=Eigen::Vector2d::Zero();
  double response=0,residual=0,weight=0;
  Row6 jacobian=Row6::Zero();
};
struct BudgetTerms {
  Mat6 A=Mat6::Zero();
  Vec6 b=Vec6::Zero();
  double alpha=1;
  std::vector<size_t> selected_ids;
};
// Fixed 64-row blocks, identical arithmetic order in serial and TBB modes.
BudgetTerms informationBudget(const std::vector<ResidualContribution>& rows,
                              InformationPolicy policy,int suppression_radius=2,
                              bool parallel=true);
// Pose prior uses the marginal pose covariance. Same information form as ESKF;
// double 6D solve, without modifying the production float 18D update.
std::map<std::string,double> auditInformation(const Mat6& geometry,const Vec6& geometry_b,
    const Mat6& prior_covariance,const Vec6& corrected_prior_delta,
    const std::vector<ResidualContribution>& rows,const BudgetTerms& photo,
    int resolution,int coarse_grid=8);
} // namespace cube
