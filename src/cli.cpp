#include "cli.hpp"
#include "logger.hpp"

#include <utility>
#include <experimental/filesystem>

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Sema/Sema.h"

namespace cxxfoozz {

// ##########
// # CLIParsedArgs
// #####

CLIParsedArgs::CLIParsedArgs() = default;
const std::string &CLIParsedArgs::GetTargetClassName() const {
  return target_class_name_;
}
void CLIParsedArgs::SetTargetClassName(const std::string &target_class_name) {
  target_class_name_ = target_class_name;
}
const std::string &CLIParsedArgs::GetOutputPrefix() const {
  return output_prefix_;
}
void CLIParsedArgs::SetOutputPrefix(const std::string &output_prefix) {
  output_prefix_ = output_prefix;
}
const std::string &CLIParsedArgs::GetWorkingDir() const {
  return working_dir_;
}
void CLIParsedArgs::SetWorkingDir(const std::string &working_dir) {
  working_dir_ = working_dir;
}
int CLIParsedArgs::GetMaxDepth() const {
  return max_depth_;
}
void CLIParsedArgs::SetMaxDepth(int max_depth) {
  max_depth_ = max_depth;
}
const std::string &CLIParsedArgs::GetObjectFilesDir() const {
  return object_files_dir_;
}
void CLIParsedArgs::SetObjectFilesDir(const std::string &object_files_dir) {
  object_files_dir_ = object_files_dir;
}
const std::string &CLIParsedArgs::GetSourceFilesDir() const {
  return source_files_dir_;
}
void CLIParsedArgs::SetSourceFilesDir(const std::string &source_files_dir) {
  source_files_dir_ = source_files_dir;
}
const std::string &CLIParsedArgs::GetExtraLdFlags() const {
  return extra_ld_flags_;
}
void CLIParsedArgs::SetExtraLdFlags(const std::string &extra_ld_flags) {
  extra_ld_flags_ = extra_ld_flags;
}
const std::string &CLIParsedArgs::GetExtraCxxFlags() const {
  return extra_cxx_flags_;
}
void CLIParsedArgs::SetExtraCxxFlags(const std::string &extra_cxx_flags) {
  extra_cxx_flags_ = extra_cxx_flags;
}
int CLIParsedArgs::GetFuzzTimeoutInSeconds() const {
  return fuzz_timeout_in_seconds_;
}
void CLIParsedArgs::SetFuzzTimeoutInSeconds(int fuzz_timeout) {
  fuzz_timeout_in_seconds_ = fuzz_timeout;
}
const std::string &CLIParsedArgs::GetFuncComplexityExtFile() const {
  return func_complexity_ext_file_;
}
void CLIParsedArgs::SetFuncComplexityExtFile(const std::string &func_complexity_ext_file) {
  func_complexity_ext_file_ = func_complexity_ext_file;
}

// ##########
// # CLIArgumentParser
// #####

static llvm::cl::OptionCategory kCxxfoozzOptions("CXXFOOZZ options");

static llvm::cl::opt<std::string> kOptTargetClass(
  "cls",
  llvm::cl::desc(
    "Specify target class name in format '[namespace::]*classname'. "
    "If this option is unspecified, the tool will generate random method call sequences "
    "without targeting any particular class"),
  llvm::cl::value_desc("string"),
  llvm::cl::cat(kCxxfoozzOptions));

static llvm::cl::opt<std::string> kOptOutputPrefix(
  "out-prefix",
  llvm::cl::desc("Specify output prefix (directory) for test suite."),
  llvm::cl::value_desc("string"),
  llvm::cl::Required,
  llvm::cl::cat(kCxxfoozzOptions));

static llvm::cl::opt<std::string> kOptObjectFileDirectory(
  "obj-dir",
  llvm::cl::desc("Specify target project's object files directory"),
  llvm::cl::value_desc("string"),
  llvm::cl::Required,
  llvm::cl::cat(kCxxfoozzOptions));

static llvm::cl::opt<std::string> kOptSrcFileDirectory(
  "src-dir",
  llvm::cl::desc("Specify target project's source files directory"),
  llvm::cl::value_desc("string"),
  llvm::cl::Required,
  llvm::cl::cat(kCxxfoozzOptions));

static llvm::cl::opt<std::string> kOptExtraCXXFlags(
  "xtra-cxx",
  llvm::cl::desc("Additional compile flags for the target project"),
  llvm::cl::value_desc("string"),
  llvm::cl::cat(kCxxfoozzOptions));

static llvm::cl::opt<std::string> kOptExtraLDFlags(
  "xtra-ld",
  llvm::cl::desc("Additional linking flags for the target project"),
  llvm::cl::value_desc("string"),
  llvm::cl::cat(kCxxfoozzOptions));

static llvm::cl::opt<std::string> kOptFuncComplexityExtFile(
  "func-comp",
  llvm::cl::desc("Additional external file with specific format for function prioritization"),
  llvm::cl::value_desc("string"),
  llvm::cl::cat(kCxxfoozzOptions));

static llvm::cl::opt<int> kOptMaxTraversalDepth(
  "max-depth",
  llvm::cl::desc(
    "Specify maximum traversal depth for gathering object file in the target project directory. Default = 1"),
  llvm::cl::value_desc("int"),
  llvm::cl::init(1),
  llvm::cl::cat(kCxxfoozzOptions));

static llvm::cl::opt<int> kOptFuzzingTimeout(
  "fuzz-timeout",
  llvm::cl::desc(
    "Specify maximum timeout (in seconds) for fuzzing. Default = 30"),
  llvm::cl::value_desc("int"),
  llvm::cl::init(30),
  llvm::cl::cat(kCxxfoozzOptions));

CLIParsedArgs CLIArgumentParser::ParseProgramOpt() {
  const std::experimental::filesystem::path &working_dir = std::experimental::filesystem::current_path();
  const std::string &wd_str = working_dir.string();

  CLIParsedArgs result;
  result.SetTargetClassName(kOptTargetClass.c_str());
  result.SetOutputPrefix(kOptOutputPrefix.c_str());
  result.SetWorkingDir(wd_str);
  result.SetObjectFilesDir(kOptObjectFileDirectory.c_str());
  result.SetSourceFilesDir(kOptSrcFileDirectory.c_str());
  result.SetMaxDepth(kOptMaxTraversalDepth.getValue());
  result.SetFuzzTimeoutInSeconds(kOptFuzzingTimeout.getValue());

  if (!kOptExtraCXXFlags.empty())
    result.SetExtraCxxFlags(kOptExtraCXXFlags.c_str());
  if (!kOptExtraLDFlags.empty())
    result.SetExtraLdFlags(kOptExtraLDFlags.c_str());
  if (!kOptFuncComplexityExtFile.empty())
    result.SetFuncComplexityExtFile(kOptFuncComplexityExtFile.c_str());

  return result;
}
CLIArgumentParser::CLIArgumentParser(int argc, const char **argv) : parser_(argc, argv, kCxxfoozzOptions) {}
clang::tooling::CommonOptionsParser &CLIArgumentParser::GetClangToolingParser() {
  return parser_;
}
} // namespace cxxfoozz