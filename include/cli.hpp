#ifndef CXXFOOZZ_INCLUDE_CLI_HPP_
#define CXXFOOZZ_INCLUDE_CLI_HPP_

#include <map>
#include "bpstd/optional.hpp"
#include <string>
#include <vector>

#include <clang/Tooling/CommonOptionsParser.h>

namespace cxxfoozz {
class CLIParsedArgs {
 public:
  CLIParsedArgs();
  const std::string &GetTargetClassName() const;
  void SetTargetClassName(const std::string &target_class_name);
  const std::string &GetOutputPrefix() const;
  void SetOutputPrefix(const std::string &output_prefix);
  const std::string &GetWorkingDir() const;
  void SetWorkingDir(const std::string &working_dir);
  const std::string &GetObjectFilesDir() const;
  void SetObjectFilesDir(const std::string &object_files_dir);
  const std::string &GetSourceFilesDir() const;
  void SetSourceFilesDir(const std::string &source_files_dir);
  const std::string &GetExtraCxxFlags() const;
  void SetExtraCxxFlags(const std::string &extra_cxx_flags);
  const std::string &GetExtraLdFlags() const;
  void SetExtraLdFlags(const std::string &extra_ld_flags);
  const std::string &GetFuncComplexityExtFile() const;
  void SetFuncComplexityExtFile(const std::string &func_complexity_ext_file);
  int GetMaxDepth() const;
  void SetMaxDepth(int max_depth);
  int GetFuzzTimeoutInSeconds() const;
  void SetFuzzTimeoutInSeconds(int fuzz_timeout);

 private:
  std::string target_class_name_;
  std::string output_prefix_;
  std::string working_dir_;
  std::string object_files_dir_;
  std::string source_files_dir_;
  std::string extra_cxx_flags_;
  std::string extra_ld_flags_;
  std::string func_complexity_ext_file_;
  int max_depth_;
  int fuzz_timeout_in_seconds_; // in seconds

};

class CLIArgumentParser {
 public:
  CLIArgumentParser(int argc, const char **argv);
  clang::tooling::CommonOptionsParser &GetClangToolingParser();
  CLIParsedArgs ParseProgramOpt();

 private:
  clang::tooling::CommonOptionsParser parser_;
};
} // namespace cxxfoozz


#endif //CXXFOOZZ_INCLUDE_CLI_HPP_
