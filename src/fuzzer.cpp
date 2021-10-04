#include <csignal>
#include <string>
#include <iostream>
#include <queue>
#include <sstream>
#include <utility>
#include <experimental/filesystem>

#include "clock.hpp"
#include "execution.hpp"
#include "function-selector.hpp"
#include "fuzzer.hpp"
#include "logger.hpp"
#include "mutator.hpp"
#include "random.hpp"
#include "sequencegen.hpp"
#include "writer.hpp"
#include "type.hpp"
#include "util.hpp"

#include "func/api.hpp"

namespace cxxfoozz {

// ##########
// # FuzzingMainLoopSpec
// #####

clang::CompilerInstance &FuzzingMainLoopSpec::GetCompilerInstance() const {
  return compiler_instance_;
}
const std::shared_ptr<ClassType> &FuzzingMainLoopSpec::GetTargetClass() const {
  return target_class_;
}
const CLIParsedArgs &FuzzingMainLoopSpec::GetCliArgs() const {
  return cli_args_;
}
FuzzingMainLoopSpec::FuzzingMainLoopSpec(
  clang::CompilerInstance &compiler_instance,
  const CLIParsedArgs &cli_args,
  std::string target_filename,
  std::shared_ptr<ClassType> target_class,
  std::shared_ptr<ProgramContext> program_ctx,
  std::shared_ptr<CompilationContext> compilation_ctx
)
  : compiler_instance_(compiler_instance),
    cli_args_(cli_args),
    target_filename_(std::move(target_filename)),
    target_class_(std::move(target_class)),
    program_ctx_(std::move(program_ctx)),
    compilation_ctx_(std::move(compilation_ctx)) {}
const std::shared_ptr<ProgramContext> &FuzzingMainLoopSpec::GetProgramCtx() const {
  return program_ctx_;
}
const std::shared_ptr<CompilationContext> &FuzzingMainLoopSpec::GetCompilationCtx() const {
  return compilation_ctx_;
}
const std::string &FuzzingMainLoopSpec::GetTargetFilename() const {
  return target_filename_;
}

// ##########
// # MainFuzzer
// #####

std::string GetWDOutputFilename(const std::string &filename, const std::string &working_dir) {
  std::experimental::filesystem::path as_fs_path(working_dir);
  const std::experimental::filesystem::path &name = as_fs_path / filename;
  return name.string();
}

std::string HARDCODED_ReplaceBuildWithLibfuzzerDir(const std::string &target_dir) {
  return ReplaceFirstOccurrence(target_dir, "/build/", "/build_libfuzzer/");
}

void PartitionByTimestamp(
  TestCaseQueue &queue,
  const std::string &working_dir,
  const std::string &output_dir,
  ReplayDriverWriter &replay_writer,
  ScaffoldingHPPFileWriter &scaff_writer,
  const std::string &folder_name
) {
  /* BEGIN PARTITIONING BY TIMESTAMP */
  const auto &h_to_sec = [](int x) { return 3600 * x; };
  int configs[5] = {1, 3, 6, 12, 24};
  for (const auto conf : configs) {
    int last_timestamp = h_to_sec(conf);
    std::vector<FlushableTestCase> subqueue = queue.GetValidByTimestamp(last_timestamp);
    const std::string &subfolder_name = folder_name + '/' + std::to_string(conf);
    const std::string &wd_subfolder = GetWDOutputFilename(subfolder_name, output_dir);
    replay_writer.WriteToDirectory(subqueue, wd_subfolder);

    scaff_writer.WriteToFile(wd_subfolder + "/out_scaffolding.hpp");
    std::experimental::filesystem::copy(working_dir + "/scripts/batch_libfuzzer.py", wd_subfolder + "/batch_libfuzzer.py");
  }
  /* END */
}

void FlushQueue(
  TestCaseQueue &queue,
  const std::shared_ptr<ImportWriter> &import_writer,
  const std::string &working_dir,
  const std::string &output_dir,
  const std::string &target_dir,
  const std::vector<std::string> &cxx_flags,
  const std::vector<std::string> &ld_flags,
  int max_traversal_depth,
  const std::shared_ptr<ProgramContext> &prog_ctx,
  const std::string &target_filename
) {
  GoogleTestWriter gtest_writer{import_writer, target_dir, cxx_flags, ld_flags, max_traversal_depth, prog_ctx};

//  static int kFlushCounter = 0;
//  int idx = ++kFlushCounter;
  std::string ext = ".cpp";
//  std::string suffix = '_' + std::to_string(idx) + ext;
  const std::string &suffix = ext;

  std::string valid_tcs = "out_valid" + suffix;
  const std::string &wd_valid = GetWDOutputFilename(valid_tcs, output_dir);
  gtest_writer.WriteToFile(queue.GetValid(), wd_valid);

  std::string crash_tcs = "out_crash" + suffix;
  const std::string &wd_crash = GetWDOutputFilename(crash_tcs, output_dir);
  gtest_writer.WriteToFile(queue.GetCrashes(), wd_crash);

  std::string uncompilable_tcs = "out_uncompilable" + suffix;
  const std::string &wd_uncompilable = GetWDOutputFilename(uncompilable_tcs, output_dir);
  gtest_writer.WriteToFile(queue.GetIncompilable(), wd_uncompilable);

  const std::string &libfuzzer_target_dir = HARDCODED_ReplaceBuildWithLibfuzzerDir(target_dir);
  ReplayDriverWriter replay_writer{
    import_writer,
    libfuzzer_target_dir,
    cxx_flags,
    ld_flags,
    max_traversal_depth,
    prog_ctx,
    ReplayDriverPurpose::kNormalUse
  };
  const std::string &wd_replay = GetWDOutputFilename("out_replay", output_dir);
  replay_writer.WriteToDirectory(queue.GetValid(), wd_replay);

  ScaffoldingHPPFileWriter scaff_writer{prog_ctx};
  scaff_writer.WriteToFile(wd_replay + "/out_scaffolding.hpp");
  std::experimental::filesystem::copy(working_dir + "/scripts/batch_libfuzzer.py", wd_replay + "/batch_libfuzzer.py");
  PartitionByTimestamp(queue, working_dir, output_dir, replay_writer, scaff_writer, "out_replay");

  assert(!Operand::IsKLibFuzzerMode());
  {
    Operand::LibFuzzerModeHacker _hack;
    assert(Operand::IsKLibFuzzerMode());
    ReplayDriverWriter libfuzzer_writer{
      import_writer,
      libfuzzer_target_dir,
      cxx_flags,
      ld_flags,
      max_traversal_depth,
      prog_ctx,
      ReplayDriverPurpose::kLibFuzzer
    };
    const std::string &wd_libfuzzer = GetWDOutputFilename("out_libfuzzer", output_dir);
    libfuzzer_writer.WriteToDirectory(queue.GetValid(), wd_libfuzzer);

    scaff_writer.WriteToFile(wd_libfuzzer + "/out_scaffolding.hpp");
    std::experimental::filesystem::copy(working_dir + "/scripts/batch_libfuzzer.py", wd_libfuzzer + "/batch_libfuzzer.py");
    PartitionByTimestamp(queue, working_dir, output_dir, libfuzzer_writer, scaff_writer, "out_libfuzzer");
  }
  assert(!Operand::IsKLibFuzzerMode());
}

std::string AsAbsolutePath(const std::string &tgt_path, const std::string &working_dir) {
  std::experimental::filesystem::path as_fs_path(tgt_path);
  bool is_relative = as_fs_path.is_relative();
  return is_relative ? working_dir + "/" + tgt_path : tgt_path;
}

bool deterministic_mode = false;
int det_progress = 0;
std::queue<std::shared_ptr<Executable>> det_queue;

bpstd::optional<TestCase> MainFuzzer::LoadTestCaseDeterministically(
  const TestCaseGenerator &tcgen,
  const std::vector<std::shared_ptr<Executable>> &executables
) {
  if (deterministic_mode && det_queue.empty()) {
    Logger::Info("MainFuzzer", "Initializing Deterministic Mode");
    for (const auto &item : executables) {
      det_queue.push(item);
    }
  }
  if (!det_queue.empty()) {
    std::shared_ptr<Executable> curr_executable = det_queue.front();
    det_queue.pop();
    Logger::Info(
      "MainFuzzer", "Deterministic progress: " + std::to_string(++det_progress)
        + ", Remaining: " + std::to_string(det_queue.size()));

    const std::shared_ptr<TemplateTypeContext> &tt_ctx = TemplateTypeContext::New();
    const seqgen::GenTCForMethodSpec &method_spec = seqgen::GenTCForMethodSpec{curr_executable, tt_ctx};
    const TestCase &tc = tcgen.GenForMethod(method_spec);
    return tc;

  } else {
    Logger::Info("MainFuzzer", "Deterministic Mode Complete!");
    deterministic_mode = false;
    return {};
  }
}

TestCase MainFuzzer::LoadTestCase(
  const TestCaseGenerator &tcgen,
  const std::vector<std::shared_ptr<Executable>> &class_methods
) {
  if (deterministic_mode) {
    const bpstd::optional<TestCase> &opt_tc = LoadTestCaseDeterministically(tcgen, class_methods);
    if (opt_tc.has_value())
      return opt_tc.value();
  }

  std::vector<FlushableTestCase> &valid_seeds = queue_.GetValid();
  unsigned long valid_size = valid_seeds.size();
  const std::shared_ptr<Random> &r = Random::GetInstance();
  bool should_gen_from_scratch = valid_size == 0 || r->NextBoolean();
  if (should_gen_from_scratch) {
    const std::shared_ptr<TemplateTypeContext> &tt_ctx = TemplateTypeContext::New();
    bool should_force_reuse_op = r->NextBoolean();

    FunctionSelector function_selector{class_methods, FunctionSelectorMode::kComplexityBased};
    const std::shared_ptr<Executable> &selected_method = function_selector.NextExecutable();

    const seqgen::GenTCForMethodSpec &method_spec =
      seqgen::GenTCForMethodSpec{selected_method, tt_ctx, should_force_reuse_op};
    const TestCase &tc = tcgen.GenForMethod(method_spec);
    return tc;
  } else {
    seed_scheduling_counter_ %= valid_size;
    const FlushableTestCase &choosen = valid_seeds[seed_scheduling_counter_];
    ++seed_scheduling_counter_;
    return choosen.GetTc();
  }
}

void MainFuzzer::MainLoop(const FuzzingMainLoopSpec &spec) {
  Logger::InfoSection("Begin Fuzzing Loop");

  const CLIParsedArgs &parsed_args = spec.GetCliArgs();
  clang::CompilerInstance &compiler_instance = spec.GetCompilerInstance();
  const std::string &target_filename = spec.GetTargetFilename();
  const std::shared_ptr<ClassType> &target_class_type = spec.GetTargetClass();
  const std::shared_ptr<ProgramContext> &program_ctx = spec.GetProgramCtx();
  const std::shared_ptr<CompilationContext> &compilation_ctx = spec.GetCompilationCtx();

  const std::vector<std::shared_ptr<Executable>> &all_executables = program_ctx->GetExecutables();
  std::vector<std::shared_ptr<Executable>> base_executables;
  if (target_class_type == nullptr) {
    std::copy_if(
      all_executables.begin(), all_executables.end(), std::back_inserter(base_executables),
      [](const std::shared_ptr<Executable> &i) {
        return !i->IsCreator();
      });

  } else {
    std::copy_if(
      all_executables.begin(), all_executables.end(), std::back_inserter(base_executables),
      [target_class_type](const std::shared_ptr<Executable> &i) {
        bool is_owned = i->GetOwner() == target_class_type->GetModel();
        return is_owned && !i->IsCreator();
      });
  }

  const std::vector<std::string> &include_paths = compilation_ctx->GetExtractedIncludePaths();
  std::vector<std::string> include_paths_vc{include_paths.begin(), include_paths.end()};

  ScaffoldingHPPFileWriter scaff_writer{program_ctx};
  include_paths_vc.push_back(ScaffoldingHPPFileWriter::kScaffoldingHPPFilename);

  const std::shared_ptr<ImportWriter> &import_writer = std::make_shared<ImportWriter>(include_paths_vc);
//  const std::shared_ptr<ImportWriter> &import_writer =
//    ImportWriter::ExtractImportWriterFromSourceLoc(compiler_instance, target_class_type->GetModel());
  TestCaseWriter tc_writer{import_writer, program_ctx};

  const std::string &out_prefix = parsed_args.GetOutputPrefix();
  const std::string &working_dir = parsed_args.GetWorkingDir();
  const std::string &output_dir = working_dir + '/' + out_prefix;
  if (!std::experimental::filesystem::exists(output_dir)) {
    std::experimental::filesystem::create_directory(output_dir);
  }

  const std::string &func_comp_ext_filename = parsed_args.GetFuncComplexityExtFile();
  bool import_func_comp = false;
  if (!func_comp_ext_filename.empty()) {
    const std::string &func_comp_abs = AsAbsolutePath(func_comp_ext_filename, working_dir);
    import_func_comp = ImportSummary(func_comp_abs);
  }
  if (import_func_comp)
    Logger::Info("Using function complexity ext file: " + func_comp_ext_filename);
  else
    Logger::Info("Without using function complexity ext file.");

  const std::string &object_files_dir = parsed_args.GetObjectFilesDir();
  const std::string &source_files_dir = parsed_args.GetSourceFilesDir();
  const std::string &obj_dir_abs = AsAbsolutePath(object_files_dir, working_dir);
  const std::string &src_dir_abs = AsAbsolutePath(source_files_dir, working_dir);

  int max_depth = parsed_args.GetMaxDepth();
  ObjectFileLocator obj_file_locator;
  const std::string &object_files = obj_file_locator.Lookup(obj_dir_abs, max_depth);
  const std::string &temporary_cpp = output_dir + "/" + SourceCompiler::kTmpDriverCppFilename;
  const std::string &temporary_o = output_dir + "/" + SourceCompiler::kTmpDriverObjectFilename;
  const std::string &temporary_exe = output_dir + "/" + SourceCompiler::kTmpDriverExeFilename;
  const std::string &scaffolding_hpp = output_dir + "/" + ScaffoldingHPPFileWriter::kScaffoldingHPPFilename;
  scaff_writer.WriteToFile(scaffolding_hpp);

  const std::shared_ptr<Random> &r = Random::GetInstance();
  TestCaseGenerator tcgen{target_class_type, program_ctx};
  TestCaseMutator tcmut{target_class_type, program_ctx};

  const std::string &xtra_cxx_flags = parsed_args.GetExtraCxxFlags();
  std::vector<std::string> cxx_flags{compilation_ctx->GetExtractedCxxFlags()};
  if (!xtra_cxx_flags.empty())
    cxx_flags.push_back(xtra_cxx_flags);

  const std::string &xtra_ld_flags = parsed_args.GetExtraLdFlags();
  std::vector<std::string> ld_flags;
  if (!xtra_ld_flags.empty())
    ld_flags.push_back(xtra_ld_flags);

  SourceCompiler compiler{"clang++", object_files, cxx_flags, ld_flags};
  CoverageObserver observer{output_dir, obj_dir_abs, src_dir_abs, CoverageMeasurementTool::kLCOVFILT};

  if (!observer.IsGCNOFileExisted()) {
    Logger::Error("Cannot find GCNO files in the target directory: " + obj_dir_abs);
  }

  observer.CleanCovInfo();
  WallClock cov_clock;
  const CoverageReport &report = observer.MeasureCoverage();
  long long int cov_measure_time = cov_clock.MeasureElapsedInMsec();
  Logger::Info("Coverage measurement time = " + std::to_string(cov_measure_time) + "ms.");

  WallClock fuzzing_clock;
  signal(SIGINT, MainFuzzer::SignalHandling);
  signal(SIGTERM, MainFuzzer::SignalHandling);
  signal(SIGABRT, MainFuzzer::SignalHandling);
  CoverageLogger cov_logger;
  CrashTCHandler crash_tc_handler;

  int timeout_in_seconds = parsed_args.GetFuzzTimeoutInSeconds();
  long long int timeout_in_msec = timeout_in_seconds * 1000LL;
  long long int total_attempts = 0LL;

  while (!interrupt && fuzzing_clock.MeasureElapsedInMsec() < timeout_in_msec) {
    const TestCase &tc = LoadTestCase(tcgen, base_executables);
    const TestCase &mutation = tcmut.MutateTestCase(tc, 20); // TODO: Try with/without deterministic mode.
//    const TestCase &mutation = tc;
    tc_writer.WriteToFile(mutation, temporary_cpp);
//    Logger::Debug("Mutated TC has been written to: " + temporary_cpp);
    ++total_attempts;
    const auto &build_result = compiler.CompileAndLink(temporary_cpp, temporary_o, temporary_exe);
    CompilationResult compile_result = build_result.first;
    static long long int kDiscardUncompilableTCsAfter = 3600000LL;
    switch (compile_result) {
      case CompilationResult::kSuccess: {
        const ExecutionResult &exec_result = observer.ExecuteAndMeasureCov(temporary_exe);
        bool normal_execution = exec_result.IsSuccessful();
        bool has_exception = exec_result.HasCaughtException();
        if (normal_execution || has_exception) {
          if (exec_result.IsInteresting()) {
            const bpstd::optional<CoverageReport> &opt_cov_report = exec_result.GetCovReport();
            const CoverageReport &cov_report = opt_cov_report.value();

            FlushableTestCase &ftc = queue_.AddValid(mutation);
            Logger::Info("Found interesting test case with ID = " + std::to_string(ftc.GetId()));
            Logger::Info("Current coverage score: " + cov_report.ToPrettyString());

            int return_code = exec_result.GetReturnCode();
            ftc.SetReturnCode(return_code);

            long long int timestamp = fuzzing_clock.MeasureElapsedInMsec() / 1000ll;
            ftc.SetTimestamp((int) timestamp);
            cov_logger.AppendEntry(
              timestamp,
              cov_report.GetLineCov(),
              cov_report.GetBranchCov(),
              cov_report.GetLineTot(),
              cov_report.GetBranchTot(),
              cov_report.GetFuncCov(),
              cov_report.GetFuncTot());
          }
        } else { // !normal_execution && !has_exception
          const TCMemo &memo = crash_tc_handler.ExecuteInGDBEnv(temporary_exe, src_dir_abs);
          const std::string &fingerprint = *memo.GetFingerprint();

          bool crash_in_source = memo.IsValidCrash() && memo.GetLocation().has_value();
          bool is_new_unique_crash = crash_in_source && crash_tc_handler.RegisterIfNewCrash(fingerprint);
          if (crash_in_source && is_new_unique_crash) {
            FlushableTestCase &ftc = queue_.AddCrashes(mutation, memo);
            Logger::Info("Found new crashing test case with ID = " + std::to_string(ftc.GetId()));
//                + "\n Fingerprint: " + fingerprint);
          }
        }
        break;
      }
      case CompilationResult::kCompileFailed: {
        long long int curr_elapsed = fuzzing_clock.MeasureElapsedInMsec();
        if (curr_elapsed < kDiscardUncompilableTCsAfter) {
          const std::string &error_msg = build_result.second;
          TCMemo memo;
          memo.SetCompilationOutput({error_msg});
          FlushableTestCase &ftc = queue_.AddIncompilable(mutation, memo);
//          Logger::Warn("Found incompilable test case with ID = " + std::to_string(ftc.GetId()));
          break;
        }
      }
      case CompilationResult::kLinkingFailed: {
        long long int curr_elapsed = fuzzing_clock.MeasureElapsedInMsec();
        if (curr_elapsed < kDiscardUncompilableTCsAfter) {
          Logger::Warn("Found linking error");
        }
        break;
      }
    }
  }

  Logger::InfoSection("Ended Fuzzing Loop");
  Logger::Info("Total attempts = " + std::to_string(total_attempts));
  queue_.PrintSummary();
  cov_logger.PrintSummary();
  long long int timeout_in_sec = timeout_in_msec / 1000LL;
  cov_logger.PrintForPlotting(output_dir, timeout_in_sec, 5LL, queue_);

  if (import_func_comp)
    Logger::Info("Using function complexity ext file: " + func_comp_ext_filename);
  else
    Logger::Info("Without using function complexity ext file.");

  FlushQueue(
    queue_,
    import_writer,
    working_dir,
    output_dir,
    obj_dir_abs,
    cxx_flags,
    ld_flags,
    max_depth,
    program_ctx,
    target_filename
  );
}

const TestCaseQueue &MainFuzzer::GetQueue() const {
  return queue_;
}
bool MainFuzzer::interrupt = false;
void MainFuzzer::SignalHandling(int signum) {
  Logger::Info("[MainFuzzer]", "Performing cleanup due to signal: " + std::to_string(signum));
  interrupt = true;
}
MainFuzzer::MainFuzzer() : seed_scheduling_counter_(0) {}

// ##########
// # FlushableTestCase
// #####
int FlushableTestCase::kGlobalTCId = 0;
FlushableTestCase::FlushableTestCase(TestCase tc)
  : id_(++kGlobalTCId), flushed_(false), tc_(std::move(tc)), return_code_(0), timestamp_(0) {}
int FlushableTestCase::GetId() const {
  return id_;
}
bool FlushableTestCase::IsFlushed() const {
  return flushed_;
}
void FlushableTestCase::SetFlushed(bool flushed) {
  flushed_ = flushed;
}
const TestCase &FlushableTestCase::GetTc() const {
  return tc_;
}
FlushableTestCase::FlushableTestCase(TestCase tc, const TCMemo &memo)
  : FlushableTestCase(std::move(tc)) {
  memo_ = memo;
}
const TCMemo &FlushableTestCase::GetMemo() const {
  return memo_;
}
int FlushableTestCase::GetReturnCode() const {
  return return_code_;
}
void FlushableTestCase::SetReturnCode(int return_code) {
  return_code_ = return_code;
}
int FlushableTestCase::GetTimestamp() const {
  return timestamp_;
}
void FlushableTestCase::SetTimestamp(int timestamp) {
  timestamp_ = timestamp;
}

// ##########
// # TestCaseQueue
// #####

std::vector<FlushableTestCase> &TestCaseQueue::GetValid() {
  return valid_;
}
std::vector<FlushableTestCase> &TestCaseQueue::GetCrashes() {
  return crashes_;
}
std::vector<FlushableTestCase> &TestCaseQueue::GetIncompilable() {
  return incompilable_;
}
FlushableTestCase &TestCaseQueue::AddValid(const TestCase &tc) {
  assert(tc.Verify());
  valid_.emplace_back(tc);
  return valid_[valid_.size() - 1];
}
FlushableTestCase &TestCaseQueue::AddCrashes(const TestCase &tc, const TCMemo &memo) {
  crashes_.emplace_back(tc, memo);
  return crashes_[crashes_.size() - 1];
}
FlushableTestCase &TestCaseQueue::AddIncompilable(const TestCase &tc, const TCMemo &memo) {
  incompilable_.emplace_back(tc, memo);
  return incompilable_[incompilable_.size() - 1];
}
void TestCaseQueue::PrintSummary() {
  std::stringstream ss;
  ss << valid_.size() << '/' << crashes_.size() << '/' << incompilable_.size();
  Logger::Info("[Valid/Crash/Incompilable] = " + ss.str());
}
std::vector<FlushableTestCase> TestCaseQueue::GetValidByTimestamp(int last_ts_in_sec) {
  std::vector<FlushableTestCase> result;
  std::copy_if(
    valid_.begin(), valid_.end(), std::back_inserter(result), [&](const auto &item) {
      return item.GetTimestamp() <= last_ts_in_sec;
    });
  return result;
}

// ##########
// # CompilationContext
// #####

CompilationContext::CompilationContext(
  std::vector<std::string> extracted_include_paths,
  std::vector<std::string> extracted_cxx_flags
)
  : extracted_include_paths_(std::move(extracted_include_paths)),
    extracted_cxx_flags_(std::move(extracted_cxx_flags)) {}
const std::vector<std::string> &CompilationContext::GetExtractedIncludePaths() const {
  return extracted_include_paths_;
}
const std::vector<std::string> &CompilationContext::GetExtractedCxxFlags() const {
  return extracted_cxx_flags_;
}
} // namespace cxxfoozz