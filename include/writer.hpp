#ifndef CXXFOOZZ_INCLUDE_WRITER_HPP_
#define CXXFOOZZ_INCLUDE_WRITER_HPP_

#include "sequencegen.hpp"
#include "fuzzer.hpp"
#include "program-context.hpp"

namespace cxxfoozz {

enum class SubdirLoc {
  kQueue = 0,
  kCrash,
  kTmp,
};

class ImportWriter {
 public:
  explicit ImportWriter(std::vector<std::string> header_files);
  const std::vector<std::string> &GetHeaderFiles() const;
  void WriteHeader(std::ofstream &out);
  static std::shared_ptr<ImportWriter> ExtractImportWriterFromSourceLoc(
    const clang::CompilerInstance &ci,
    const std::shared_ptr<ClassTypeModel> &target
  );
  int GetLineUsage() const;

 private:
  std::vector<std::string> header_files_;

};

class ScaffoldingHPPFileWriter {
 public:
  explicit ScaffoldingHPPFileWriter(std::shared_ptr<ProgramContext> program_ctx);
  const std::shared_ptr<ProgramContext> &GetProgramCtx() const;
  void WriteToFile(const std::string &location);
  static const std::string &kScaffoldingHPPFilename;
  std::string GetScaffoldingContent();
 private:
  std::string GetFuncSignature(const std::shared_ptr<cxxfoozz::Executable> &executable);
 private:
  std::shared_ptr<ProgramContext> program_ctx_;
  static bool kUseScaffoldingHPP;
};

class TestCaseWriter {
 public:
  TestCaseWriter(
    std::shared_ptr<ImportWriter> import_writer,
    const std::shared_ptr<ProgramContext> &context
  );
  const std::shared_ptr<ImportWriter> &GetImportWriter() const;
  void WriteToFile(const TestCase &tc, const std::string &filename);
  static int LineNumberToStmtIdx(int src_linenum, int import_line_count, bool has_exception); // stmtIdx: 0-based idx
  bpstd::optional<std::shared_ptr<Statement>> GetStatementByLineNumber(
    const TestCase &tc,
    int src_linenum,
    bool has_exception
  );

 private:
  std::shared_ptr<ImportWriter> import_writer_;
  const std::shared_ptr<ProgramContext> &context_;
};

class GoogleTestWriter {
 public:
  explicit GoogleTestWriter(
    std::shared_ptr<ImportWriter> import_writer,
    std::string target_dir,
    std::vector<std::string> compile_flags,
    std::vector<std::string> ld_flags,
    int max_depth,
    const std::shared_ptr<ProgramContext> &context
  );
  void WriteToFile(
    std::vector<FlushableTestCase> &flushable_tcs,
    const std::string &filename,
    const std::string &suite_name = "CxxFoozzTestSuite"
  );
 private:
  void AppendCompileInstruction(std::ofstream &target, const std::string &filename);
 private:
  std::shared_ptr<ImportWriter> import_writer_;
  std::string target_dir_;
  std::vector<std::string> compile_flags_;
  std::vector<std::string> ld_flags_;
  int max_depth_;
  const std::shared_ptr<ProgramContext> &context_;
};

enum class ReplayDriverPurpose {
  kNormalUse = 0,
  kLibFuzzer,
};

class ReplayDriverWriter {
 public:
  ReplayDriverWriter(
    std::shared_ptr<ImportWriter> import_writer,
    std::string target_dir,
    std::vector<std::string> compile_flags,
    std::vector<std::string> ld_flags,
    int max_depth,
    const std::shared_ptr<ProgramContext> &context,
    ReplayDriverPurpose purpose
  );
  void WriteToDirectory(
    std::vector<FlushableTestCase> &flushable_tcs,
    const std::string &dir_name
  );
 private:
  void AppendLibFuzzerHelperFunctions(std::ofstream &target);
  void AppendCompileInstruction(std::ofstream &target, const std::string &filename);
 private:
  std::shared_ptr<ImportWriter> import_writer_;
  std::string target_dir_;
  std::vector<std::string> compile_flags_;
  std::vector<std::string> ld_flags_;
  int max_depth_;
  const std::shared_ptr<ProgramContext> &context_;
  ReplayDriverPurpose purpose_;
};

} // namespace cxxfoozz



#endif //CXXFOOZZ_INCLUDE_WRITER_HPP_
