#ifndef CXXFOOZZ_INCLUDE_COMPILER_HPP_
#define CXXFOOZZ_INCLUDE_COMPILER_HPP_

#include "bpstd/optional.hpp"
#include <set>
#include <string>
#include <vector>
#include "clock.hpp"

namespace cxxfoozz {

class ObjectFileLocator {
 public:
  std::string Lookup(const std::string &target_dir, int max_depth = 1);
};

class SysProcessReport {
 public:
  SysProcessReport(bool success, std::string command, std::string output);
  bool IsSuccess() const;
  const std::string &GetCommand() const;
  const std::string &GetOutput() const;

 private:
  bool success_;
  std::string command_;
  std::string output_;
};

enum class CompilationResult {
  kSuccess = 0,
  kCompileFailed,
  kLinkingFailed,
};

class SourceCompiler {
 public:
  SourceCompiler(
    std::string cxx_compiler,
    std::string object_files,
    const std::vector<std::string> &additional_compile_flags,
    const std::vector<std::string> &additional_ld_flags
  );
  std::pair<CompilationResult, std::string> CompileAndLink(
    const std::string &target_cpp,
    const std::string &target_o,
    const std::string &target_exe
  );
  static const std::string &kTmpDriverCppFilename;
  static const std::string &kTmpDriverObjectFilename;
  static const std::string &kTmpDriverExeFilename;
 private:
  SysProcessReport Compile(const std::string &target_cpp, const std::string &target_o);
  SysProcessReport Link(const std::string &target_o, const std::string &target_exe);
  std::string cxx_compiler_;
  std::string object_files_;
  const std::vector<std::string> &additional_compile_flags_;
  const std::vector<std::string> &additional_ld_flags_;
};

class CoverageReport {
 public:
  CoverageReport();
  CoverageReport(int line_cov, int branch_cov, int line_tot, int branch_tot, int func_cov, int func_tot);
  int GetLineCov() const;
  int GetBranchCov() const;
  int GetLineTot() const;
  int GetBranchTot() const;
  int GetFuncCov() const;
  int GetFuncTot() const;
  std::tuple<double, double, double> GetCoverage() const;
  std::string ToPrettyString() const;
  friend std::ostream &operator<<(std::ostream &os, const CoverageReport &report);

 private:
  int line_cov_, branch_cov_;
  int line_tot_, branch_tot_;
  int func_cov_, func_tot_;
};

class ExecutionResult {
 public:
  ExecutionResult(
    int return_code,
    const bpstd::optional<CoverageReport> &cov_report,
    bool interesting
  );
  int GetReturnCode() const;
  const bpstd::optional<CoverageReport> &GetCovReport() const;
  bool IsInteresting() const;
  bool IsSuccessful() const;
  bool HasCaughtException() const;
 public:
  static const int kExceptionReturnCode;
 private:
  int return_code_;
  bpstd::optional<CoverageReport> cov_report_;
  bool interesting_;
};

enum class CoverageMeasurementTool {
  kGCOVR = 0,
  kLCOV,
  kLCOVFILT,
};

class CoverageObserver {
 public:
  CoverageObserver(
    std::string output_dir,
    std::string object_files_dir,
    std::string source_files_dir,
    CoverageMeasurementTool measurement_tool,
    long long int exec_timeout_in_msec = 5000ll
  );
  ExecutionResult ExecuteAndMeasureCov(const std::string &target_exe);
  CoverageReport MeasureCoverage();
  void CleanCovInfo();
  bool IsGCNOFileExisted();
 private:
  int Execute(const std::string &target_exe);
  CoverageReport prev_success_;
  std::string object_files_dir_;
  std::string source_files_dir_;
  long long int exec_timeout_in_msec_;
  std::string output_dir_;
  CoverageMeasurementTool measurement_tool_;
};

class TCMemo;

class CrashTCHandler {
 public:
  CrashTCHandler();
  ~CrashTCHandler();
  TCMemo ExecuteInGDBEnv(const std::string &target_exe, const std::string &src_dir);
  bool RegisterIfNewCrash(const std::string &squashed_stack_trace);
 private:
  void WriteGDBCommandFile();
  void DeleteGDBCommandFile();
  std::string SquashStackTrace(const std::vector<std::string> &stack_trace_ids);
  bool IsNewCrash(const std::string &squashed_stack_trace) const;
  std::set<std::string> unique_crashes_;
};

class TCMemo {
 public:
  TCMemo();
  bool IsValidCrash() const;
  const bpstd::optional<std::string> &GetFingerprint() const;
  const bpstd::optional<std::string> &GetGdbOutput() const;
  const bpstd::optional<std::string> &GetLocation() const;
  const bpstd::optional<int> &GetCrashLineNum() const;
  const bpstd::optional<std::string> &GetCompilationOutput() const;

  void SetValidCrash(bool valid_crash);
  void SetFingerprint(const bpstd::optional<std::string> &fingerprint);
  void SetGdbOutput(const bpstd::optional<std::string> &gdb_output);
  void SetLocation(const bpstd::optional<std::string> &location);
  void SetCrashLineNum(const bpstd::optional<int> &crash_line_num);
  void SetCompilationOutput(const bpstd::optional<std::string> &compilation_output);

 private:
  bool valid_crash_ = true;
  bpstd::optional<std::string> fingerprint_; // for identifying crash, taken src location from non-library call stack
  bpstd::optional<std::string> gdb_output_; // for gdb output
  bpstd::optional<std::string> location_; // for bottommost-level call stack
  bpstd::optional<int> crash_line_num_;
  bpstd::optional<std::string> compilation_output_; // for incompilable
};
}

#endif //CXXFOOZZ_INCLUDE_COMPILER_HPP_
