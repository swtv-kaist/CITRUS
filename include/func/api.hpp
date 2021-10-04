#ifndef CXXFOOZZ__FUNC_ANALYSIS_ENCODE_HPP_
#define CXXFOOZZ__FUNC_ANALYSIS_ENCODE_HPP_

#include "clang/AST/Mangle.h"

#include <vector>
#include <string>
#include <map>

class StatementVisitorResult {
 public:
  StatementVisitorResult();
  int GetSwitchCases() const;
  int GetCondExpr() const;
  int GetControls() const;
  int GetShortCirs() const;
  void SetSwitchCases(int switch_cases);
  void SetCondExpr(int cond_expr);
  void SetControls(int controls);
  void SetShortCirs(int short_cirs);
  void SetCalls(const std::vector<std::string> &calls);
  const std::vector<std::string> &GetCalls() const;
  void Print() const;
  friend class StatementVisitor;
 private:
  int switch_cases_, cond_expr_, controls_, short_cirs_;
  std::vector<std::string> calls_;
};

extern std::map<std::string, StatementVisitorResult> kGlobalSummary;
extern std::map<std::string, int> kFunctionBodyLoC;
void ExportSummary(const std::string &filename);
bool ImportSummary(const std::string &filename);
void PrintFunctionSizeAverage();
std::string MangleFunctionDecl(clang::FunctionDecl *d, clang::MangleContext *mangle_ctx);

#endif