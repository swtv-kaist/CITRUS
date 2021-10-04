
#include "func/api.hpp"
#include "function-selector.hpp"
#include "random.hpp"
#include <utility>
#include <iostream>

namespace cxxfoozz {

FunctionSelector::FunctionSelector(
  std::vector<std::shared_ptr<Executable>> executables,
  FunctionSelectorMode mode
) : executables_(std::move(executables)), mode_(mode) {}
std::shared_ptr<Executable> FunctionSelector::NextExecutable() {
  if (mode_ == FunctionSelectorMode::kRandom || kGlobalSummary.empty()) {
    const std::shared_ptr<Random> &r = Random::GetInstance();
    int idx = r->NextInt((int) executables_.size());
    const std::shared_ptr<Executable> &selected_method = executables_[idx];
    return selected_method;

  } else {
    std::vector<std::pair<double, std::shared_ptr<Executable>>> complexity_score;
    for (const auto &item : executables_) {
      const std::string &mangled_name = item->GetMangledName();
      if (mangled_name.empty()) {
        complexity_score.emplace_back(1.0, item);
        continue;
      }
      const auto &find_it = kGlobalSummary.find(mangled_name);
      if (find_it == kGlobalSummary.end()) {
        complexity_score.emplace_back(1.0, item);
        continue;
      } else {
        const StatementVisitorResult &fc = find_it->second;
        int call_to_other = (int) fc.GetCalls().size();
        int controls = fc.GetControls();
        int sw_cases = fc.GetSwitchCases();
        int cond_expr = fc.GetCondExpr();
        int short_cirs = fc.GetShortCirs();
        int sum_score = 1 + call_to_other + controls + sw_cases + cond_expr + short_cirs;
        complexity_score.emplace_back((double) sum_score, item);
      }
    }
//    std::sort(complexity_score.begin(),  complexity_score.end());
    double cumm_score = 0.0;
    for (auto &score : complexity_score) {
      cumm_score += score.first;
      score.first = cumm_score;
    }
    for (auto &score : complexity_score) {
      score.first /= cumm_score;
    }

    const std::shared_ptr<Random> &r = Random::GetInstance();
    double target_prob = r->NextGaussian();
    int lo = 0, hi = (int) complexity_score.size();
    while (hi - lo > 1) {
      int mid = (hi + lo) / 2;
      double curr_score = complexity_score[mid].first;
      if (curr_score < target_prob) {
        lo = mid;
      } else {
        hi = mid;
      }
    }
    if (complexity_score[lo].first > target_prob)
      return complexity_score[lo].second;
    return complexity_score[hi].second;
  }
}

}
