#include "execution.hpp"
#include "logger.hpp"
#include "util.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <utility>
#include <experimental/filesystem>

namespace cxxfoozz {

std::pair<int, std::string> ExecuteCommand(const std::string &cmd) {
  std::string redirected_cerr = cmd + " 2>&1";
  FILE *pipe = popen(redirected_cerr.c_str(), "r");
  if (!pipe) {
    std::cerr << "[ExecuteCommand] Unable to execute: " << cmd << '\n';
    exit(1);
  }

  std::array<char, 128> buffer{};
  std::string result;
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    result += buffer.data();

  int ret_code = pclose(pipe);
  int return_code = WEXITSTATUS(ret_code);
//  if (return_code != EXIT_SUCCESS) {
//    std::cerr << "[ExecuteCommand] Warning! non-successful return code from: " << cmd << '\n';
//  }
  return std::make_pair(return_code, result);
}

// ##########
// # SourceCompiler
// #####
const std::string &SourceCompiler::kTmpDriverCppFilename = "tmp.cpp";
const std::string &SourceCompiler::kTmpDriverObjectFilename = "tmp.o";
const std::string &SourceCompiler::kTmpDriverExeFilename = "tmp";
SourceCompiler::SourceCompiler(
  std::string cxx_compiler,
  std::string object_files,
  const std::vector<std::string> &additional_compile_flags,
  const std::vector<std::string> &additional_ld_flags
)
  : object_files_(std::move(object_files)),
    cxx_compiler_(std::move(cxx_compiler)),
    additional_compile_flags_(additional_compile_flags),
    additional_ld_flags_(additional_ld_flags) {}
SysProcessReport SourceCompiler::Compile(const std::string &target_cpp, const std::string &target_o) {
  std::stringstream ss;
  ss << cxx_compiler_ << " -g -c -o " << target_o << " " << target_cpp;
  for (const auto &flag : additional_compile_flags_) {
    ss << ' ' << flag;
  }
  const std::string &cmd_to_exec = ss.str();
  const std::pair<int, std::string> &result = ExecuteCommand(cmd_to_exec);
  return {
    result.first == EXIT_SUCCESS,
    cmd_to_exec,
    result.second,
  };
}
SysProcessReport SourceCompiler::Link(const std::string &target_o, const std::string &target_exe) {
  std::stringstream ss;
  ss << cxx_compiler_ << " -g -o " << target_exe << " " << target_o << " " << object_files_
     << " --coverage -fsanitize=fuzzer-no-link";
  for (const auto &flag : additional_ld_flags_) {
    ss << ' ' << flag;
  }
  const std::string &cmd_to_exec = ss.str();
  const std::pair<int, std::string> &result = ExecuteCommand(cmd_to_exec);
  return {
    result.first == EXIT_SUCCESS,
    cmd_to_exec,
    result.second,
  };
}
std::pair<CompilationResult, std::string> SourceCompiler::CompileAndLink(
  const std::string &target_cpp,
  const std::string &target_o,
  const std::string &target_exe
) {
  const auto &compile_rc = Compile(target_cpp, target_o);
  if (!compile_rc.IsSuccess()) {
//    std::cerr << "[SourceCompiler::CompileAndLink] Compilation failed\n";
    const std::string &compile_error_msg = compile_rc.GetOutput();
    return {CompilationResult::kCompileFailed, compile_error_msg};
  }
  const auto &link_rc = Link(target_o, target_exe);
  if (!link_rc.IsSuccess()) {
    const std::string &linking_cmd = link_rc.GetCommand();
//    Logger::Error(
//      "SourceCompiler::CompileAndLink",
//      "Linking failed, check your linking command:\n (*)> " + linking_cmd, true);
    const std::string &link_error_msg = link_rc.GetOutput();
    return {CompilationResult::kLinkingFailed, link_error_msg};
  }
  return {CompilationResult::kSuccess, ""};
}

// ##########
// # CoverageReport
// #####

CoverageReport::CoverageReport() : CoverageReport(0, 0, 0, 0, 0, 0) {}
CoverageReport::CoverageReport(int line_cov, int branch_cov, int line_tot, int branch_tot, int func_cov, int func_tot)
  : line_cov_(line_cov),
    branch_cov_(branch_cov),
    line_tot_(line_tot),
    branch_tot_(branch_tot),
    func_cov_(func_cov),
    func_tot_(func_tot) {}
int CoverageReport::GetLineCov() const {
  return line_cov_;
}
int CoverageReport::GetBranchCov() const {
  return branch_cov_;
}
int CoverageReport::GetLineTot() const {
  return line_tot_;
}
int CoverageReport::GetBranchTot() const {
  return branch_tot_;
}
std::string CoverageReport::ToPrettyString() const {
  const std::tuple<double, double, double> &cov = GetCoverage();
  std::stringstream ss;
  ss << "L: " << line_cov_ << '/' << line_tot_ << ", "
     << "B: " << branch_cov_ << '/' << branch_tot_ << ", "
     << "F: " << func_cov_ << '/' << func_tot_ << ", "
     << "%: " << std::get<0>(cov) << ", " << std::get<1>(cov) << ", " << std::get<2>(cov);
  return ss.str();
}
std::ostream &operator<<(std::ostream &os, const CoverageReport &report) {
  os << report.ToPrettyString();
  return os;
}
std::tuple<double, double, double> CoverageReport::GetCoverage() const {
  double lcov = line_tot_ != 0 ? 100.0 * line_cov_ / line_tot_ : 0.0;
  double bcov = branch_tot_ != 0 ? 100.0 * branch_cov_ / branch_tot_ : 0.0;
  double fcov = func_tot_ != 0 ? 100.0 * func_cov_ / func_tot_ : 0.0;
  return std::make_tuple(lcov, bcov, fcov);
}
int CoverageReport::GetFuncCov() const {
  return func_cov_;
}
int CoverageReport::GetFuncTot() const {
  return func_tot_;
}

// ##########
// # ExecutionResult
// #####

ExecutionResult::ExecutionResult(
  int return_code,
  const bpstd::optional<CoverageReport> &cov_report,
  bool interesting
) : return_code_(return_code), cov_report_(cov_report), interesting_(interesting) {}
int ExecutionResult::GetReturnCode() const {
  return return_code_;
}
const bpstd::optional<CoverageReport> &ExecutionResult::GetCovReport() const {
  return cov_report_;
}
bool ExecutionResult::IsInteresting() const {
  return interesting_;
}
bool ExecutionResult::IsSuccessful() const {
  return return_code_ == EXIT_SUCCESS;
}
const int ExecutionResult::kExceptionReturnCode = 180;
bool ExecutionResult::HasCaughtException() const {
  return return_code_ == kExceptionReturnCode;
}

// ##########
// # CoverageObserver
// #####

// For alternatives:
//   -) lcov -c -d . -o lcov.info --rc lcov_branch_coverage=1 --no-external
//   -) genhtml -o html lcov.info --branch-coverage

std::pair<int, int> ParseFromGcovrSummary(const std::string &summary) {
  unsigned long start = summary.find_last_of('(');
  unsigned long end = summary.find_last_of(')');
  unsigned long s1 = summary.find_first_of(' ', start);
  unsigned long s2 = summary.find_last_of(' ', end);

  std::string cov = summary.substr(start + 1, s1 - start - 1);
  std::string tot = summary.substr(s2 + 1, end - s2 - 1);
  return std::make_pair(std::stoi(cov), std::stoi(tot));
}

CoverageReport ParseFromGcovrOutput(const std::string &output) {
  unsigned long lines_idx = output.rfind("\nlines: ");
  unsigned long branch_idx = output.rfind("\nbranches: ");
  auto branch_substr = output.substr(branch_idx + 1, output.size() - branch_idx);
  auto line_substr = output.substr(lines_idx + 1, branch_idx - lines_idx);

  const std::pair<int, int> &line_info = ParseFromGcovrSummary(line_substr);
  const std::pair<int, int> &branch_info = ParseFromGcovrSummary(branch_substr);

  return CoverageReport{line_info.first, branch_info.first, line_info.second, branch_info.second, 0, 0};
}

std::pair<int, int> ParseFromLCOVSummary(const std::string &line) {
  if (line.find("no data found") != std::string::npos)
    return {0, 0};
  unsigned long start = line.find_last_of('(');
  unsigned long end = line.find_last_of(')');
  unsigned long s2 = line.find_last_of(' ', end);
  unsigned long s3 = line.find_last_of(' ', s2 - 1);
  unsigned long s1 = line.find_last_of(' ', s3 - 1);

  std::string cov = line.substr(start + 1, s1 - start - 1);
  std::string tot = line.substr(s3 + 1, s2 - s3 - 1);
  return std::make_pair(std::stoi(cov), std::stoi(tot));
}

CoverageReport ParseFromLCOVOutput(const std::string &output) {
  std::vector<std::string> lines = SplitStringIntoVector(output, "\n");
  std::reverse(lines.begin(), lines.end());
  std::string line_cov, branch_cov, func_cov;
  int counter = 0;
  for (const auto &line : lines) {
    if (line.rfind("  lines......:", 0) == 0) {
      line_cov = line;
      ++counter;
    } else if (line.rfind("  functions..:", 0) == 0) {
      func_cov = line;
      ++counter;
    } else if (line.rfind("  branches...:", 0) == 0) {
      branch_cov = line;
      ++counter;
    }
    if (counter == 3)
      break;
  }
  assert(counter == 3);
  const auto &line_info = ParseFromLCOVSummary(line_cov);
  const auto &func_info = ParseFromLCOVSummary(func_cov);
  const auto &branch_info = ParseFromLCOVSummary(branch_cov);

  return CoverageReport{
    line_info.first,
    branch_info.first,
    line_info.second,
    branch_info.second,
    func_info.first,
    func_info.second
  };
}

int CoverageObserver::Execute(const std::string &target_exe) {
  std::experimental::filesystem::path as_fs_path(target_exe);
  int timeout_s = (int) exec_timeout_in_msec_ / 1000;
  const std::string &to_str = std::to_string(timeout_s);
  std::string timeout_cmd = "timeout " + to_str + "s ";

  bool is_absolute_path = as_fs_path.is_absolute();
  std::string final_cmd = timeout_cmd + (is_absolute_path ? "" : "./") + target_exe;
  return ExecuteCommand(final_cmd).first;
}
// https://stackoverflow.com/questions/38875615/gcovr-giving-empty-results-zero-percent-in-mac
// gcovr root directory (-r) must be the directory where .cpp files exist
CoverageReport CoverageObserver::MeasureCoverage() {
  switch (measurement_tool_) {
    case CoverageMeasurementTool::kGCOVR: {
      //  const std::string &additional_flags = "--exclude-throw-branches --exclude-unreachable-branches";
      const std::string &additional_flags = "";
      // https://github.com/gcovr/gcovr/issues/169 OUCH :(
      const std::string &command = "gcovr -r " + source_files_dir_ + " -f " + source_files_dir_
        + " --branch -s" + ' ' + additional_flags + ' ' + object_files_dir_ + " --gcov-executable gcov_for_clang.sh";
      const std::pair<int, std::string> &execution_res = ExecuteCommand(command);
      int rc = execution_res.first;
      if (rc != EXIT_SUCCESS) {
        std::cerr << "[CoverageObserver::MeasureCoverage] Coverage measurement failed. Return code = " << rc << '\n';
        exit(1);
      }
      const std::string &output = execution_res.second;
      return ParseFromGcovrOutput(output);
    }
    case CoverageMeasurementTool::kLCOV:
    case CoverageMeasurementTool::kLCOVFILT: {
      const std::string &tool = "lcov-filt";
      const std::string &additional_flags = "--filter branch,line";
      const std::string &filename1 = output_dir_ + "/lcov.info";
      const std::string &filename2 = output_dir_ + "/lcov2.info";
      const char *lcov_branch_cov = "--rc lcov_branch_coverage=1";
      const char *ignore_empty = " --ignore-errors empty ";
      const char *gcov_tool = " --gcov-tool gcov_for_clang.sh";
      const char *suppress_stderr = " 2> /dev/null";
      const std::string &cmd1 =
        tool + ignore_empty + " -c -d " + object_files_dir_ + " -o " + filename1 + ' ' + lcov_branch_cov + gcov_tool
          + suppress_stderr;
      const std::string &cmd2 =
        tool + ignore_empty + ' ' + additional_flags + ' ' + lcov_branch_cov + " -o " + filename2
          + " -r " + filename1 + " '/usr/include/*' '/usr/lib/*'" + suppress_stderr;
      const std::string &command = cmd1 + " && " + cmd2;
      const std::pair<int, std::string> &execution_res = ExecuteCommand(command);
      int rc = execution_res.first;
      if (rc != EXIT_SUCCESS) {
        std::cerr << "[CoverageObserver::MeasureCoverage] Coverage measurement failed. Return code = " << rc << '\n';
        exit(1);
      }
      const std::string &output = execution_res.second;
      return ParseFromLCOVOutput(output);
    }
  }
}

ExecutionResult CoverageObserver::ExecuteAndMeasureCov(const std::string &target_exe) {
  int rc = Execute(target_exe);
  if (rc != EXIT_SUCCESS && rc != ExecutionResult::kExceptionReturnCode)
    return ExecutionResult{rc, bpstd::nullopt, false};

  const CoverageReport &report = MeasureCoverage();
  int curr_line = report.GetLineCov(), pline_cov = prev_success_.GetLineCov();
  int curr_branch = report.GetBranchCov(), pbranch_cov = prev_success_.GetBranchCov();
  int curr_func = report.GetFuncCov(), pfunc_cov = prev_success_.GetFuncCov();
  bool is_interesting = curr_line > pline_cov || curr_branch > pbranch_cov || curr_func > pfunc_cov;
  if (is_interesting)
    prev_success_ = report; // Assume using gcovr

  return ExecutionResult{rc, bpstd::make_optional(report), is_interesting};
}
void CoverageObserver::CleanCovInfo() {
  const std::string &command = "find " + object_files_dir_ + R"( -name "*.gcda" -exec rm -f {} \;)";
  ExecuteCommand(command);
  prev_success_ = CoverageReport();
}
bool CoverageObserver::IsGCNOFileExisted() {
  const std::string &command = "find " + object_files_dir_ + R"( -name "*.gcno")";
  const std::pair<int, std::string> &execution_result = ExecuteCommand(command);
  if (execution_result.first != EXIT_SUCCESS)
    return false;
  if (execution_result.second.empty())
    return false;
  return true;
}
CoverageObserver::CoverageObserver(
  std::string output_dir,
  std::string object_files_dir,
  std::string source_files_dir,
  CoverageMeasurementTool measurement_tool,
  long long int exec_timeout_in_msec
)
  : output_dir_(std::move(output_dir)),
    object_files_dir_(std::move(object_files_dir)),
    source_files_dir_(std::move(source_files_dir)),
    measurement_tool_(measurement_tool),
    exec_timeout_in_msec_(exec_timeout_in_msec) {}

// ##########
// # CrashTCHandler
// #####

void CrashTCHandler::WriteGDBCommandFile() {
  if (std::ofstream target{"test.gdb"}) {
    target << "run\n"
              "\n"
              "bt";
  }
}

bool IsStartOfStackTrace(const std::string &line) {
  return !line.empty() && line[0] == '#' && line[1] == '0';
}

bool IsEndOfStackTrace(const std::string &line) {
  return line.empty();
}

std::string ParseLocation(const std::string &line) {
  const std::vector<std::string> &tokens = SplitStringIntoVector(line, " ");
//  const auto &find_it = std::find_if(
//    tokens.begin(), tokens.end(), [](const std::string &item) {
//      return !item.empty() && item.rfind("0x", 0) == 0;
//    });
//  if (find_it == tokens.end())
//    return std::make_pair("", "");
//  const std::string &stack_trace_id = *find_it;

  const std::vector<std::string>::const_reverse_iterator &at_it = std::find_if(
    tokens.rbegin(), tokens.rend(), [](const std::string &item) {
      return !item.empty() && item == "at";
    });
  if (at_it == tokens.rend()) {
    return "";
//    return std::make_pair(stack_trace_id, "");
  }
  const std::string &location = *(at_it - 1);
  return location;
}

bool InvokingOnNullptrCheck(const std::vector<std::string> &gdb_output_ss) {
  std::vector<std::string> temp_vc(gdb_output_ss);
  std::reverse(temp_vc.begin(), temp_vc.end());

  const std::vector<std::string>::iterator &main_it = std::find_if(
    temp_vc.begin(), temp_vc.end(), [](const std::string &item) {
      bool is_main = item.find("main") != std::string::npos;
      bool at_driver = item.find(SourceCompiler::kTmpDriverCppFilename) != std::string::npos;
      return is_main && at_driver;
    });
  if (main_it != temp_vc.end() && (main_it + 1 != temp_vc.end())) {
    const std::string &invoke = *(main_it + 1);
    bool is_nullptr = invoke.find("this=0x0") != std::string::npos;
    return is_nullptr;
  }
  return false;
}

TCMemo CrashTCHandler::ExecuteInGDBEnv(const std::string &target_exe, const std::string &src_dir) {
  std::stringstream ss;
  ss << "timeout 5 gdb --batch --command=test.gdb --args " << target_exe;
  const std::pair<int, std::string> &execution_result = ExecuteCommand(ss.str());

  const std::string &gdb_output = execution_result.second;
//  std::cout << "## YOUR GDB OUTPUT" << '\n';
//  std::cout << gdb_output << '\n';
//  std::cout << "## END OF GDB OUTPUT" << '\n' << '\n';

  const std::vector<std::string> &gdb_output_ss = SplitStringIntoVector(gdb_output, "\n");
  bool nullptr_crash = InvokingOnNullptrCheck(gdb_output_ss);
  if (nullptr_crash) {
    TCMemo memo;
    memo.SetValidCrash(false);
    memo.SetGdbOutput({gdb_output});
    return memo;
  }

  std::string first_location, main_location;
  std::vector<std::string> final_stack_trace;
  bool found_first_tgt_loc = false;
  bool is_stack_trace = false;
  for (const auto &gdb_line : gdb_output_ss) {
    const std::string &stripped = StringStrip(gdb_line);
    if (IsStartOfStackTrace(stripped))
      is_stack_trace = true;
    else if (is_stack_trace && IsEndOfStackTrace(stripped)) {
      is_stack_trace = false;
      break;
    }

    if (is_stack_trace) {
//      const std::string &st_loc = ParseLocation(stripped);
      const std::string &loc = ParseLocation(stripped);
      if (loc.rfind(src_dir, 0) == 0) {
        final_stack_trace.push_back(loc);
        if (!found_first_tgt_loc) {
          first_location = loc;
        }
        found_first_tgt_loc = true;
      }
      main_location = loc;
    }
  }
  TCMemo memo;
  const std::string &squashed_stack_trace = SquashStackTrace(final_stack_trace);
  memo.SetFingerprint({squashed_stack_trace});

  memo.SetGdbOutput({gdb_output});
  if (found_first_tgt_loc)
    memo.SetLocation({first_location});
  else
    memo.SetValidCrash(false); // crash did not occur in src directory

  if (!main_location.empty() && main_location.find(':') != std::string::npos) {
    const std::vector<std::string> &locs = SplitStringIntoVector(main_location, ":");
    int line_num = std::stoi(locs[1]);
    memo.SetCrashLineNum({line_num});
  }
  return memo;
}
std::string CrashTCHandler::SquashStackTrace(const std::vector<std::string> &stack_trace_ids) {
  std::stringstream joined;
  bool first_elmt = true;
  for (const auto &trace_id : stack_trace_ids) {
    joined << (first_elmt ? "" : " ") << trace_id;
    first_elmt = false;
  }
  return joined.str();
}
bool CrashTCHandler::IsNewCrash(const std::string &squashed_stack_trace) const {
  return unique_crashes_.count(squashed_stack_trace) == 0;
}
bool CrashTCHandler::RegisterIfNewCrash(const std::string &squashed_stack_trace) {
  if (IsNewCrash(squashed_stack_trace)) {
    unique_crashes_.insert(squashed_stack_trace);
    return true;
  }
  return false;
}
CrashTCHandler::CrashTCHandler() {
  WriteGDBCommandFile();
}
CrashTCHandler::~CrashTCHandler() {
  DeleteGDBCommandFile();
}
void CrashTCHandler::DeleteGDBCommandFile() {
  std::remove("test.gdb");
}

// ##########
// # ObjectFileLocator
// #####

// To exclude dir: add -path dirname -prune -false -o AFTER target_dir
// To prune depth:
std::string ObjectFileLocator::Lookup(const std::string &target_dir, int max_depth) {
  assert(max_depth > 0);
  const std::string &md_string = std::to_string(max_depth);
  const std::string &command = "echo -n $(find " + target_dir + " -maxdepth " + md_string + " -type f -name \"*.o\")";
  const std::pair<int, std::string> &execution_result = ExecuteCommand(command);
  return execution_result.second;
}

// ##########
// # TCMemo
// #####

const bpstd::optional<std::string> &TCMemo::GetFingerprint() const {
  return fingerprint_;
}
const bpstd::optional<std::string> &TCMemo::GetGdbOutput() const {
  return gdb_output_;
}
const bpstd::optional<std::string> &TCMemo::GetLocation() const {
  return location_;
}
void TCMemo::SetFingerprint(const bpstd::optional<std::string> &fingerprint) {
  fingerprint_ = fingerprint;
}
void TCMemo::SetGdbOutput(const bpstd::optional<std::string> &gdb_output) {
  gdb_output_ = gdb_output;
}
void TCMemo::SetLocation(const bpstd::optional<std::string> &location) {
  location_ = location;
}
const bpstd::optional<int> &TCMemo::GetCrashLineNum() const {
  return crash_line_num_;
}
void TCMemo::SetCrashLineNum(const bpstd::optional<int> &crash_line_num) {
  crash_line_num_ = crash_line_num;
}
bool TCMemo::IsValidCrash() const {
  return valid_crash_;
}
void TCMemo::SetValidCrash(bool valid_crash) {
  valid_crash_ = valid_crash;
}
void TCMemo::SetCompilationOutput(const bpstd::optional<std::string> &compilation_output) {
  compilation_output_ = compilation_output;
}
const bpstd::optional<std::string> &TCMemo::GetCompilationOutput() const {
  return compilation_output_;
}
TCMemo::TCMemo() = default;

// ##########
// # SysProcessReport
// #####

SysProcessReport::SysProcessReport(bool success, std::string command, std::string output)
  : success_(success), command_(std::move(command)), output_(std::move(output)) {}
bool SysProcessReport::IsSuccess() const {
  return success_;
}
const std::string &SysProcessReport::GetCommand() const {
  return command_;
}
const std::string &SysProcessReport::GetOutput() const {
  return output_;
}
}

