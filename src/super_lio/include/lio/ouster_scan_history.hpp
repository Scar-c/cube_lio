#pragma once

#include "common/ds.h"
#include <algorithm>
#include <cmath>

namespace LI2Sup {

// Keep the preceding Super propagation states when adjacent Ouster scans
// overlap in acquisition time. Align that prior history to the ESKF-corrected
// end state; no official COIN poses or transforms enter this operation.
inline void retainRebasedOusterHistory(std::vector<DynamicState>& history,
    const DynamicState& corrected_end,double next_scan_start){
  if(history.size()<2||std::abs(history.back().time-corrected_end.time)>1e-5){
    history.clear();
    history.push_back(corrected_end);
    return;
  }
  const auto& predicted_end=history.back();
  const BASIC::M3 rotation=corrected_end.R*predicted_end.R.transpose();
  const BASIC::V3 translation=corrected_end.p-rotation*predicted_end.p;
  const BASIC::V3 velocity_delta=corrected_end.v-rotation*predicted_end.v;
  auto first=std::upper_bound(history.begin(),history.end(),next_scan_start,
      [](double t,const DynamicState& s){return t<s.time;});
  if(first!=history.begin())--first;
  std::vector<DynamicState> kept;
  kept.reserve(static_cast<std::size_t>(std::distance(first,history.end()))+1);
  for(auto iter=first;iter!=history.end();++iter){
    if(iter->time>=corrected_end.time-1e-8)break;
    if(!kept.empty()&&iter->time<=kept.back().time)continue;
    DynamicState state=*iter;
    state.R=rotation*state.R;
    state.p=rotation*state.p+translation;
    state.v=rotation*state.v+velocity_delta;
    state.a=rotation*state.a;
    kept.push_back(state);
  }
  kept.push_back(corrected_end);
  history.swap(kept);
}

} // namespace LI2Sup
