#ifndef CXXFOOZZ_INCLUDE_FUZZER_HPP_
#define CXXFOOZZ_INCLUDE_FUZZER_HPP_

#include "cli.hpp"
#include "clock.hpp"
#include "sequencegen.hpp"
#include "type.hpp"
#include "program-context.hpp"
#include "execution.hpp"

namespace cxxfoozz {

class CompilationContext {
 public:
  CompilationContext(
    std::vector<std::string> extracted_include_paths,
    std::vector<std::string> extracted_cxx_flags
  );
  const std::vector<std::string> &GetExtractedIncludePaths() const;
  const std::vector<std::string> &GetExtractedCxxFlags() const;
 private:
  std::vector<std::string> extracted_include_paths_; // from PPCallbacks
  std::vector<std::string> extracted_cxx_flags_; // from compile_commands.json
};

class FuzzingMainLoopSpec {
 public:
  FuzzingMainLoopSpec(
    clang::CompilerInstance &compiler_instance,
    const CLIParsedArgs &cli_args,
    std::string target_filename,
    std::shared_ptr<ClassType> target_class,
    std::shared_ptr<ProgramContext> program_ctx,
    std::shared_ptr<CompilationContext> compilation_ctx
  );
  clang::CompilerInstance &GetCompilerInstance() const;
  const CLIParsedArgs &GetCliArgs() const;
  const std::string &GetTargetFilename() const;
  const std::shared_ptr<ClassType> &GetTargetClass() const;
  const std::shared_ptr<ProgramContext> &GetProgramCtx() const;
  const std::shared_ptr<CompilationContext> &GetCompilationCtx() const;
 private:
  clang::CompilerInstance &compiler_instance_;
  std::string target_filename_;
  const CLIParsedArgs &cli_args_;
  std::shared_ptr<ClassType> target_class_;
  std::shared_ptr<ProgramContext> program_ctx_;
  std::shared_ptr<CompilationContext> compilation_ctx_;
};

class FlushableTestCase {
 public:
  explicit FlushableTestCase(TestCase tc);
  FlushableTestCase(TestCase tc, const TCMemo &memo);
  int GetId() const;
  bool IsFlushed() const;
  const TestCase &GetTc() const;
  void SetFlushed(bool flushed);
  const TCMemo &GetMemo() const;
  int GetReturnCode() const;
  void SetReturnCode(int return_code);
  int GetTimestamp() const;
  void SetTimestamp(int timestamp);

 private:
  static int kGlobalTCId;
  int id_;
  int timestamp_;
  bool flushed_ = false; // allow destruction only if flushed_ = true :)
  TestCase tc_;
  TCMemo memo_;
  int return_code_;
};

class TestCaseQueue {
 public:
  std::vector<FlushableTestCase> &GetValid();
  std::vector<FlushableTestCase> GetValidByTimestamp(int last_ts_in_sec);
  std::vector<FlushableTestCase> &GetCrashes();
  std::vector<FlushableTestCase> &GetIncompilable();
  FlushableTestCase &AddValid(const TestCase &tc);
  FlushableTestCase &AddCrashes(const TestCase &tc, const TCMemo &memo);
  FlushableTestCase &AddIncompilable(const TestCase &tc, const TCMemo &memo);
  void PrintSummary();
 private:
  std::vector<FlushableTestCase> valid_;
  std::vector<FlushableTestCase> crashes_;
  std::vector<FlushableTestCase> incompilable_;
};

class MainFuzzer {
 public:
  MainFuzzer();
  const TestCaseQueue &GetQueue() const;
  void MainLoop(const FuzzingMainLoopSpec &spec);
  static void SignalHandling(int signum);
 private:
  TestCase LoadTestCase(const TestCaseGenerator &tcgen, const std::vector<std::shared_ptr<Executable>> &class_methods);
  bpstd::optional<TestCase> LoadTestCaseDeterministically(
    const TestCaseGenerator &tcgen,
    const std::vector<std::shared_ptr<Executable>> &executables
    );
 private:
  TestCaseQueue queue_;
  unsigned int seed_scheduling_counter_;
  static bool interrupt;
};

} // namespace cxxfoozz


#endif //CXXFOOZZ_INCLUDE_FUZZER_HPP_
