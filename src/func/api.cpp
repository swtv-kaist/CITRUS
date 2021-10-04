#include <fstream>
#include <cassert>
#include <iostream>
#include <experimental/filesystem>
#include "func/api.hpp"

std::map<std::string, StatementVisitorResult> kGlobalSummary;
std::map<std::string, int> kFunctionBodyLoC;

void StatementVisitorResult::Print() const {
  printf("Counts: %d %d %d %d\n", switch_cases_, cond_expr_, controls_, short_cirs_);
  printf("Call size: %d\n", (int) calls_.size());
}
StatementVisitorResult::StatementVisitorResult() : switch_cases_(0), cond_expr_(0), controls_(0), short_cirs_(0) {}
int StatementVisitorResult::GetSwitchCases() const {
  return switch_cases_;
}
int StatementVisitorResult::GetCondExpr() const {
  return cond_expr_;
}
int StatementVisitorResult::GetControls() const {
  return controls_;
}
int StatementVisitorResult::GetShortCirs() const {
  return short_cirs_;
}
const std::vector<std::string> &StatementVisitorResult::GetCalls() const {
  return calls_;
}
void StatementVisitorResult::SetCalls(const std::vector<std::string> &calls) {
  calls_ = calls;
}
void StatementVisitorResult::SetSwitchCases(int switch_cases) {
  switch_cases_ = switch_cases;
}
void StatementVisitorResult::SetCondExpr(int cond_expr) {
  cond_expr_ = cond_expr;
}
void StatementVisitorResult::SetControls(int controls) {
  controls_ = controls;
}
void StatementVisitorResult::SetShortCirs(int short_cirs) {
  short_cirs_ = short_cirs;
}


void ExportSummary(const std::string &filename) {
  std::map<std::string, int> indexes;
  int idx = 0;
  for (const auto &summary : kGlobalSummary) {
    const std::string &name = summary.first;
    indexes[name] = idx++;
  }
  std::vector<std::vector<int>> adj_list(idx);
  for (const auto &summary : kGlobalSummary) {
    const std::string &name = summary.first;
    const StatementVisitorResult &result = summary.second;
    const std::vector<std::string> &calls = result.GetCalls();

    int caller_idx = indexes[name];
    for (const auto &call : calls) {
      const std::map<std::string, int>::iterator &find_callee = indexes.find(call);
      if (find_callee != indexes.end()) {
        int callee_idx = find_callee->second;
        adj_list[caller_idx].push_back(callee_idx);
      }
    }
  }
  std::string out_dir = "func_comp";
  if (!std::experimental::filesystem::exists(out_dir)) {
    std::experimental::filesystem::create_directory(out_dir);
  }
  const auto &filepath = out_dir + '/' + filename;
  if (std::ofstream target{filepath}) {
    target << ">>>>>>>>>>>>>nameDict\n";
    int indexesLen = (int) indexes.size();
    target << indexesLen << std::endl;
    for (const auto &item : indexes) {
      target << item.second << ' ' << item.first << std::endl;
    }

    target << ">>>>>>>>>>>>>adjList\n";
    int adjListLen = (int) adj_list.size();
    target << adjListLen << std::endl;
    for (int i = 0; i < adjListLen; i++) {
      int curr_len = (int) adj_list[i].size();
      target << i << ' ' << curr_len;
      for (const auto &adj : adj_list[i]) {
        target << ' ' << adj;
      }
      target << std::endl;
    }

    target << ">>>>>>>>>>>>>complexity(controls,sw_cases,cond_exprs,short_cirs)\n";
    int globSummaryLen = (int) kGlobalSummary.size();
    target << globSummaryLen << std::endl;
    for (const auto &summary : kGlobalSummary) {
      const std::string &name = summary.first;
      int func_idx = indexes[name];
      const StatementVisitorResult &result = summary.second;

      int controls = result.GetControls();
      int sw_cases = result.GetSwitchCases();
      int cond_exprs = result.GetCondExpr();
      int short_cirs = result.GetShortCirs();
      target << func_idx
      << ' ' << controls << ' ' << sw_cases
      << ' ' << cond_exprs << ' ' << short_cirs << std::endl;
    }
  }
}

bool ImportSummary(const std::string &filename) {
  kGlobalSummary.clear();
  std::string dummy;
  if (std::ifstream target{filename}) {
    std::map<std::string, int> nameDict;
    std::map<int, std::string> revNameDict;
    target >> dummy;
    assert(dummy == ">>>>>>>>>>>>>nameDict");
    int indexesLen;
    target >> indexesLen;
    for (int i = 0; i < indexesLen; i++) {
      int idx; std::string name;
      target >> idx >> name;
      nameDict[name] = idx;
      revNameDict[idx] = name;
    }

    target >> dummy;
    assert(dummy == ">>>>>>>>>>>>>adjList");
    int adjListLen;
    target >> adjListLen;
    std::vector<std::vector<int>> adjList(adjListLen);
    for (int i = 0; i < adjListLen; i++) {
      int nodeIdx, nodeAdjLen;
      target >> nodeIdx >> nodeAdjLen;
      for (int j = 0; j < nodeAdjLen; j++) {
        int adjNode;
        target >> adjNode;
        adjList[nodeIdx].push_back(adjNode);
      }
    }

    target >> dummy;
    assert(dummy == ">>>>>>>>>>>>>complexity(controls,sw_cases,cond_exprs,short_cirs)");
    int globSummaryLen;
    target >> globSummaryLen;
    for (int i = 0; i < globSummaryLen; i++) {
      int func_idx, controls, sw_cases, cond_exprs, short_cirs;
      target >> func_idx >> controls >> sw_cases >> cond_exprs >> short_cirs;

      const std::string &func_name = revNameDict[func_idx];
      StatementVisitorResult result;
      result.SetControls(controls);
      result.SetSwitchCases(sw_cases);
      result.SetCondExpr(cond_exprs);
      result.SetShortCirs(short_cirs);

      const std::vector<int> &adjNodes = adjList[func_idx];
      std::vector<std::string> adjFuncs;
      for (const auto &nodeIdx : adjNodes) {
        const std::string &adjFuncName = revNameDict[nodeIdx];
        adjFuncs.push_back(adjFuncName);
      }
      result.SetCalls(adjFuncs);
      kGlobalSummary[func_name] = result;
    }
    return true;
  } else {
    return false;
  }
}
std::string MangleFunctionDecl(clang::FunctionDecl *d, clang::MangleContext *mangle_ctx) {
  bool should_mangle = mangle_ctx->shouldMangleDeclName(d);
  if (!should_mangle) {
    std::string name_info = d->getNameInfo().getName().getAsString();
    std::replace(name_info.begin(),  name_info.end(), ' ', '_');
    return name_info;
  }
  std::string mangled_name;
  llvm::raw_string_ostream ostream(mangled_name);

  if (const auto *ctor = llvm::dyn_cast_or_null<clang::CXXConstructorDecl>(d)) {
    std::string name_info = d->getNameInfo().getName().getAsString();
    std::replace(name_info.begin(),  name_info.end(), ' ', '_');
    return name_info;
  } else if (const auto *dtor = llvm::dyn_cast_or_null<clang::CXXDestructorDecl>(d)) {
    std::string name_info = d->getNameInfo().getName().getAsString();
    std::replace(name_info.begin(),  name_info.end(), ' ', '_');
    return name_info;
  }

  mangle_ctx->mangleName(d, ostream);
  ostream.flush();
  return mangled_name;
}

void PrintFunctionSizeAverage() {
  long long int total = 0;
  int divisor = 0;
  for (const auto &func_size : kFunctionBodyLoC) {
    total += func_size.second;
    ++divisor;
  }
  double avg_size = divisor == 0 ? 0 : (double) total / divisor;
  printf("Avg. Function Size (LoC): %.2lf (%d functions)\n", avg_size, divisor);
}