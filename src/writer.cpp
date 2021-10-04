#include "logger.hpp"
#include "writer.hpp"
#include "util.hpp"

#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <utility>

namespace cxxfoozz {
// ##########
// # ImportWriter
// #####
ImportWriter::ImportWriter(std::vector<std::string> header_files) : header_files_(std::move(header_files)) {}
const std::vector<std::string> &ImportWriter::GetHeaderFiles() const {
  return header_files_;
}
void ImportWriter::WriteHeader(std::ofstream &out) {
  for (const auto &header_file : header_files_) {
    out << "#include \"" << header_file << "\"\n";
  }
  out << '\n'; // Please remember to change GetLineUsage if you are about to remove this line
}
std::shared_ptr<ImportWriter> ImportWriter::ExtractImportWriterFromSourceLoc(
  const clang::CompilerInstance &ci,
  const std::shared_ptr<ClassTypeModel> &target
) {
  clang::SourceManager &src_manager = ci.getSourceManager();
  clang::CXXRecordDecl *target_class_decl = target->GetClangDecl();

  const clang::SourceLocation &src_loc = target_class_decl->getLocation();
  const std::string &decl_location_str = src_loc.printToString(src_manager);
  unsigned long colon_first = decl_location_str.find_first_of(':');
  const std::string &import = decl_location_str.substr(0, colon_first);

  const std::vector<std::string> &single_import = std::vector<std::string>{import};
  return std::make_shared<ImportWriter>(single_import);
}
int ImportWriter::GetLineUsage() const {
  return (int) header_files_.size() + 1; // (+1) from an extra end-of-line.
}

// ##########
// # TestCaseWriter
// #####
const int kIndentWidth = 2;
const std::string kIndentString(kIndentWidth, ' ');
void WriteStatementWithIndentation(
  std::ofstream &out,
  const std::string &stmt,
  bool no_semicolon = false,
  int indent_width = kIndentWidth
) {
  if (indent_width == kIndentWidth) {
    out << kIndentString;
  } else {
    const std::string indent_str(indent_width, ' ');
    out << indent_str;
  }
  out << stmt;
  if (!no_semicolon && stmt.back() != ';')
    out << ';';
  out << '\n';
}

void EnsureResultDirInitialized() {
  const std::experimental::filesystem::path &pwd = std::experimental::filesystem::current_path();
  const std::experimental::filesystem::path &result_dir = pwd / "result";
  if (!std::experimental::filesystem::exists(result_dir)) {
    if (!std::experimental::filesystem::create_directory(result_dir)) {
      std::cerr << "[TestCaseWriter::EnsureResultDirInitialized] Unable to create dir: " << result_dir << '\n';
      exit(1);
    }
    if (!std::experimental::filesystem::create_directory(result_dir / "queue")
      || !std::experimental::filesystem::create_directory(result_dir / "crashes")
      || !std::experimental::filesystem::create_directory(result_dir / "tmp")) {
      std::cerr << "[TestCaseWriter::EnsureResultDirInitialized] Unable to initialize sub-directories\n";
      exit(1);
    }
  }
}

TestCaseWriter::TestCaseWriter(
  std::shared_ptr<ImportWriter> import_writer,
  const std::shared_ptr<ProgramContext> &context
)
  : import_writer_(std::move(import_writer)), context_(context) {}
const std::shared_ptr<ImportWriter> &TestCaseWriter::GetImportWriter() const {
  return import_writer_;
}

enum class TryCatchVariant {
  kNoTryCatch = 0,
  kWithTryCatch,
  kWithTryCatchNoReturnValue,
};

void PrintStatements(
  std::ofstream &target,
  const TestCase &tc,
  const std::shared_ptr<ProgramContext> &prog_ctx,
  bpstd::optional<int> crash_tag_idx = bpstd::nullopt, // 0-based index
  TryCatchVariant try_catch_mode = TryCatchVariant::kNoTryCatch
) {
  const std::vector<std::shared_ptr<Statement>> &statements = tc.GetStatements();
  std::for_each(
    statements.begin(), statements.end(), [](auto &i) {
      i->ClearVarName();
    });

  StatementWriter stmt_writer{prog_ctx};
  if (try_catch_mode != TryCatchVariant::kNoTryCatch)
    WriteStatementWithIndentation(target, "try {", true, 1);
  int idx = 0;
  for (const auto &statement : statements) {
    if (crash_tag_idx.has_value() && crash_tag_idx.value() == idx) {
      WriteStatementWithIndentation(target, "/* PROGRAM CRASHED AT THE EXACT LINE BELOW */", true);
    }
    const std::string &stmt = stmt_writer.StmtAsString(statement, idx++);
    WriteStatementWithIndentation(target, stmt);
  }
  if (try_catch_mode != TryCatchVariant::kNoTryCatch) {
    bool no_ret = try_catch_mode == TryCatchVariant::kWithTryCatchNoReturnValue;
    const std::string &exception_ret_stmt = no_ret ? "" : ("return "
      + std::to_string(ExecutionResult::kExceptionReturnCode) + ';');
    WriteStatementWithIndentation(target, "} catch (...) { " + exception_ret_stmt + " }", true, 1);
  }
}

void TestCaseWriter::WriteToFile(const TestCase &tc, const std::string &filename) {
  if (std::ofstream target{filename}) {
    if (import_writer_ != nullptr)
      import_writer_->WriteHeader(target);

    target << "int main() {\n";
    PrintStatements(target, tc, context_, {}, TryCatchVariant::kWithTryCatch); // for temporary driver files
    WriteStatementWithIndentation(target, "return 0");
    target << "}\n";

  } else {
    Logger::Error("[TestCaseWriter::WriteToFile]", "Problematic output file: " + filename + '\n');
  }
}
int TestCaseWriter::LineNumberToStmtIdx(int src_linenum, int import_line_count, bool has_exception) {
  int tagged_line = src_linenum - import_line_count - 2; // 2: 1 for int main(), 1 to align with 0-based index
  if (!has_exception)
    tagged_line -= 1; // because the outputed TC will have no "try {" line.
  return tagged_line;
}
bpstd::optional<std::shared_ptr<Statement>> TestCaseWriter::GetStatementByLineNumber(
  const TestCase &tc,
  int src_linenum,
  bool has_exception
) {
  int import_line_count = import_writer_->GetLineUsage();
  int stmt_idx = LineNumberToStmtIdx(src_linenum, import_line_count, has_exception);
  const std::vector<std::shared_ptr<Statement>> &stmts = tc.GetStatements();
  if (stmt_idx < ((int) stmts.size()))
    return {stmts[stmt_idx]};
  else
    return {};
}

// ##########
// # GoogleTestWriter
// #####

inline bool SuffixCheck(const std::string &value, const std::string &ending) {
  if (ending.size() > value.size()) return false;
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

void GoogleTestWriter::AppendCompileInstruction(std::ofstream &target, const std::string &filename) {
  const std::string &object_files =
    "$(find " + target_dir_ + " -maxdepth " + std::to_string(max_depth_) + " -type f -name \"*.o\")";

  assert(SuffixCheck(filename, ".cpp"));
  std::string executable_name = filename;
  for (int i = 0; i < 4; i++) executable_name.pop_back(); // remove .cpp
  std::string o_filename = executable_name + ".o";

  std::stringstream cxx_flags;
  for (const auto &item : compile_flags_) {
    cxx_flags << ' ' << item;
  }
  const std::string &cxx_flags_str = cxx_flags.str();

  const char *gtest_ldflags = "--coverage -fsanitize=fuzzer-no-link -lgtest -lpthread";
  std::stringstream ld_flags;
  for (const auto &item : ld_flags_) {
    ld_flags << ' ' << item;
  }
  ld_flags << ' ' << gtest_ldflags;
  const std::string &ld_flags_str = ld_flags.str();

  std::stringstream compile_ss;
  compile_ss << "// clang++ -c -o " << o_filename << ' ' << filename << cxx_flags_str;
  std::stringstream link_ss;
  link_ss << "// clang++ -o " << executable_name << ' ' << o_filename << ' ' << object_files << ld_flags_str;

  target << "// Compile instruction:\n"
         << compile_ss.str() << '\n'
         << link_ss.str() << '\n';
}

std::string ReplaceString(std::string subject, const std::string &search, const std::string &replace) {
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
  }
  return subject;
}
std::string SanitizeCxxBlockComment(const std::string &to_sanitize) {
  const std::string &tmp = ReplaceString(to_sanitize, "/*", "/-*");
  const std::string &final = ReplaceString(tmp, "*/", "*-/");
  return final;
}

GoogleTestWriter::GoogleTestWriter(
  std::shared_ptr<ImportWriter> import_writer,
  std::string target_dir,
  std::vector<std::string> compile_flags,
  std::vector<std::string> ld_flags,
  int max_depth,
  const std::shared_ptr<ProgramContext> &context
)
  : import_writer_(std::move(import_writer)),
    target_dir_(std::move(target_dir)),
    compile_flags_(std::move(compile_flags)),
    ld_flags_(std::move(ld_flags)),
    max_depth_(max_depth), context_(context) {}

void GoogleTestWriter::WriteToFile(
  std::vector<FlushableTestCase> &flushable_tcs,
  const std::string &filename,
  const std::string &suite_name
) {
  if (std::ofstream target{filename}) {
    target << "#include <gtest/gtest.h>\n";

    if (import_writer_ != nullptr)
      import_writer_->WriteHeader(target);

    if (flushable_tcs.empty())
      target << "// CXXFOOZZ did not generate any test case here.\n\n";

    for (auto &ftc : flushable_tcs) {
      const TestCase &tc = ftc.GetTc();
      const TCMemo &memo = ftc.GetMemo();

      target << "TEST(" << suite_name << ", tc_id_" << ftc.GetId() << ") {\n";
      if (memo.GetLocation().has_value())
        WriteStatementWithIndentation(target, "// location: " + memo.GetLocation().value(), true);
      if (memo.GetFingerprint().has_value())
        WriteStatementWithIndentation(target, "// crash fp: " + memo.GetFingerprint().value(), true);
      if (memo.GetGdbOutput().has_value()) {
        const std::string &gdb_output = memo.GetGdbOutput().value();
        const std::string &gdb_san = SanitizeCxxBlockComment(gdb_output);
        WriteStatementWithIndentation(target, "/* gdb output:\n" + gdb_san + "*/", true);
      }
      if (memo.GetCompilationOutput().has_value()) {
        const std::string &compilation_output = memo.GetCompilationOutput().value();
        const std::string &cmp_san = SanitizeCxxBlockComment(compilation_output);
        WriteStatementWithIndentation(target, "/* compilation output:\n" + cmp_san + "*/", true);
      }
      bool has_exception = ftc.GetReturnCode() == ExecutionResult::kExceptionReturnCode;
      int crash_tag_idx = -1;
      if (memo.GetCrashLineNum().has_value()) {
        const int &crash_line_num = memo.GetCrashLineNum().value();
        int import_line_count = import_writer_->GetLineUsage();
        crash_tag_idx = TestCaseWriter::LineNumberToStmtIdx(crash_line_num, import_line_count, has_exception);
      }
      PrintStatements(
        target,
        tc,
        context_,
        crash_tag_idx >= 0 ? bpstd::make_optional(crash_tag_idx) : bpstd::nullopt,
        has_exception ? TryCatchVariant::kWithTryCatchNoReturnValue
                      : TryCatchVariant::kNoTryCatch); // for the final GoogleTest-formatted Test Suite
      ftc.SetFlushed(true);
      target << "}\n\n";
    }

    target << "int main(int argc, char **argv) {\n";
    WriteStatementWithIndentation(target, "testing::InitGoogleTest(&argc, argv)");
    WriteStatementWithIndentation(target, "return RUN_ALL_TESTS()");
    target << "}\n\n";

    AppendCompileInstruction(target, filename);

  } else {
    Logger::Error("[GoogleTestWriter::WriteToFile]", "Problematic output file: " + filename + '\n');
  }

}

// ##########
// # ScaffoldingHPPFileWriter
// #####

bool ScaffoldingHPPFileWriter::kUseScaffoldingHPP = false;
const std::string &ScaffoldingHPPFileWriter::kScaffoldingHPPFilename = "out_scaffolding.hpp";
ScaffoldingHPPFileWriter::ScaffoldingHPPFileWriter(std::shared_ptr<ProgramContext> program_ctx)
  : program_ctx_(std::move(program_ctx)) {}
const std::shared_ptr<ProgramContext> &ScaffoldingHPPFileWriter::GetProgramCtx() const {
  return program_ctx_;
}
void ScaffoldingHPPFileWriter::WriteToFile(const std::string &location) {
  if (std::ofstream target{location}) {
    target << "#ifndef CXXFOOZZ_SCAFFOLDING_HPP_FILE\n";
    target << "#define CXXFOOZZ_SCAFFOLDING_HPP_FILE\n\n";

    if (kUseScaffoldingHPP) {
      const std::string &content = GetScaffoldingContent();
      target << content;
    }

    target << "\n#endif\n";
  }
}

std::string GetNamespace(const std::string &qual_name, const std::string &name) {
  if (name == qual_name)
    return "";

  unsigned long length = name.size();
  std::string result = qual_name;
  for (int i = 0; i < length; i++) {
    result.pop_back();
  }
  for (int i = 0; i < 2; i++) {
    result.pop_back(); // ::
  }
  return result;
}

std::map<std::string, std::vector<std::shared_ptr<cxxfoozz::Executable>>> AggregateExecutablesByNamespace(
  const std::vector<std::shared_ptr<cxxfoozz::Executable>> &executables
) {
  std::map<std::string, std::vector<std::shared_ptr<cxxfoozz::Executable>>> result;
  for (const auto &item : executables) {
    bool is_global_func = item->GetOwner() != nullptr;
    if (is_global_func)
      continue;

    const std::string &qual_name = item->GetQualifiedName();
    const std::string &name = item->GetName();

    const std::string &the_namespace = GetNamespace(qual_name, name);
    result[the_namespace].push_back(item);
  }
  return result;
}

std::map<std::string, std::vector<std::string>> AggregateCTMsByNamespace(
  const std::vector<std::shared_ptr<cxxfoozz::ClassTypeModel>> &ctms
) {
  std::map<std::string, std::vector<std::string>> result;
  for (const auto &item : ctms) {
    const std::string &qual_name = item->GetQualifiedName();
    const std::string &name = item->GetName();
    const std::string &the_namespace = GetNamespace(qual_name, name);

    bool is_class = item->GetVariant() == ClassTypeModelVariant::kClass;
    const std::string &decl = (is_class ? "class " : "struct ") + name;
    result[the_namespace].push_back(decl);
  }
  return result;
}

void AppendTemplateTypingToSignature(std::stringstream &ss, const std::shared_ptr<cxxfoozz::Executable> &executable) {
  const std::string &qname = executable->GetQualifiedName();
  const TemplateTypeParamList &tt_param_list = executable->GetTemplateParamList();
  const std::vector<TemplateTypeParam> &tt_params = tt_param_list.GetList();
  if (tt_params.empty())
    return;

  std::vector<std::string> args;
  for (const auto &tt_param : tt_params) {
    const std::string &name = tt_param.GetName();
    bool is_type_param = tt_param.GetVariant() == TemplateTypeParamVariant::kTypeParam;
    if (is_type_param) {
      args.push_back("typename " + name);
    } else {
      args.push_back("int " + name);
    }
  }

  const std::string &joined_args = StringJoin(args);
  ss << "template <" << joined_args << ">\n";
}

std::string ScaffoldingHPPFileWriter::GetFuncSignature(const std::shared_ptr<cxxfoozz::Executable> &executable) {
  const clang::ASTContext &ast_context = program_ctx_->GetAstContext();
  const clang::PrintingPolicy &policy = ast_context.getPrintingPolicy();

  std::stringstream ss;
  AppendTemplateTypingToSignature(ss, executable);

  const std::string &return_type = executable->GetReturnType().value().getAsString(policy);
  const std::string &name_no_namespace = executable->GetName();
  ss << return_type << " " << name_no_namespace;

  std::stringstream joined;
  const std::vector<clang::QualType> &arguments = executable->GetArguments();
  auto it = arguments.begin();
  for (int idx = 0; it != arguments.end(); ++it, ++idx) {
    joined << (idx == 0 ? "" : ", ") << it->getAsString(policy);
  }
  ss << "(" << joined.str() << ")";

  const std::string &result = ss.str();
  return result;
}

std::string ScaffoldingHPPFileWriter::GetScaffoldingContent() {
  std::stringstream ss;

//  const std::vector<std::shared_ptr<cxxfoozz::ClassTypeModel>> &ctms = program_ctx_->GetClassTypeModels();
//  const std::map<std::string, std::vector<std::string>> &agg_ctms = AggregateCTMsByNamespace(ctms);
//
//  for (const auto &pair : agg_ctms) {
//    const std::string &the_namespace = pair.first;
//    const std::vector<std::string> &decls = pair.second;
//    ss << "namespace " + the_namespace + " {\n";
//    for (const auto &decl : decls) {
//      ss << decl << ";\n";
//    }
//    ss << "}\n";
//  }

  const std::vector<std::shared_ptr<cxxfoozz::Executable>> &executables = program_ctx_->GetExecutables();
  const std::map<std::string, std::vector<std::shared_ptr<cxxfoozz::Executable>>>
    agg_execs = AggregateExecutablesByNamespace(executables);

  for (const auto &pair : agg_execs) {
    const std::string &the_namespace = pair.first;
    const std::vector<std::shared_ptr<cxxfoozz::Executable>> &the_execs = pair.second;
    ss << "namespace " + the_namespace + " {\n";
    for (const auto &exec : the_execs) {
      const std::string &func_def = GetFuncSignature(exec);
      ss << func_def << ";\n";
    }
    ss << "}\n";
  }

  return ss.str();
}

// ##########
// # ReplayDriverWriter
// #####

ReplayDriverWriter::ReplayDriverWriter(
  std::shared_ptr<ImportWriter> import_writer,
  std::string target_dir,
  std::vector<std::string> compile_flags,
  std::vector<std::string> ld_flags,
  int max_depth,
  const std::shared_ptr<ProgramContext> &context,
  ReplayDriverPurpose purpose
)
  : import_writer_(std::move(import_writer)),
    target_dir_(std::move(target_dir)),
    compile_flags_(std::move(compile_flags)),
    ld_flags_(std::move(ld_flags)),
    max_depth_(max_depth),
    context_(context), purpose_(purpose) {}

void ReplayDriverWriter::WriteToDirectory(
  std::vector<FlushableTestCase> &flushable_tcs,
  const std::string &dir_name
) {
  if (std::experimental::filesystem::exists(dir_name)) {
    std::experimental::filesystem::remove_all(dir_name);
  }
  std::experimental::filesystem::create_directory(dir_name);

  bool for_libfuzzer = purpose_ == ReplayDriverPurpose::kLibFuzzer;
  for (const auto &ftc : flushable_tcs) {
    const std::string &filename = "tc_" + std::to_string(ftc.GetId()) + ".cpp";
    const std::string &fullpath = dir_name + '/' + filename;
    if (std::ofstream target{fullpath}) {
      if (import_writer_ != nullptr)
        import_writer_->WriteHeader(target);

      // Actually, clang++ does not require these lines
//      target << "#include <stdint.h>\n";
//      target << "#include <stddef.h>\n";

      const TestCase &tc = ftc.GetTc();
      const TCMemo &memo = ftc.GetMemo();

      if (for_libfuzzer) {
        AppendLibFuzzerHelperFunctions(target);
        target << "extern \"C\" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {\n";
        WriteStatementWithIndentation(target, "Init(Data, Size)");
      } else {
        target << "int main() {\n";
      }

      bool has_exception = ftc.GetReturnCode() == ExecutionResult::kExceptionReturnCode;
      PrintStatements(
        target,
        tc,
        context_,
        bpstd::nullopt,
        has_exception ? TryCatchVariant::kWithTryCatchNoReturnValue : TryCatchVariant::kNoTryCatch
      ); // for the final GoogleTest-formatted Test Suite

      WriteStatementWithIndentation(target, "return 0");
      target << "}\n\n";

      AppendCompileInstruction(target, filename);

    } else {
      Logger::Error("[ReplayDriverWriter::WriteToDirectory]", "Problematic output file: " + fullpath + '\n');
    }
  }
}
void ReplayDriverWriter::AppendCompileInstruction(std::ofstream &target, const std::string &filename) {
  bool for_libfuzzer = purpose_ == ReplayDriverPurpose::kLibFuzzer;
  const std::string &object_files =
    "$(find " + target_dir_ + " -maxdepth " + std::to_string(max_depth_) + " -type f -name \"*.o\")";

  assert(SuffixCheck(filename, ".cpp"));
  std::string executable_name = filename;
  for (int i = 0; i < 4; i++) executable_name.pop_back(); // remove .cpp
  std::string o_filename = executable_name + ".o";

  const std::string &coverage_flag = " --coverage";
  const std::string &fuzzer_no_link_flag = " -fsanitize=fuzzer-no-link";

  std::stringstream cxx_flags;
  for (const auto &item : compile_flags_) {
    bool is_glibcxx_use_cxx11_abi = item.rfind("-D_GLIBCXX_USE_CXX11_ABI=0", 0) == 0;
    if (is_glibcxx_use_cxx11_abi) {
      cxx_flags << ' ' << "-D_GLIBCXX_USE_CXX11_ABI=1";
    } else {
      cxx_flags << ' ' << item;
    }
  }
  cxx_flags << coverage_flag << fuzzer_no_link_flag;
  const std::string &cxx_flags_str = cxx_flags.str();

  std::stringstream ld_flags;
  for (const auto &item : ld_flags_) {
    ld_flags << ' ' << item;
  }
  if (for_libfuzzer) ld_flags << ' ' << "-fsanitize=fuzzer --coverage";
  else ld_flags << coverage_flag << fuzzer_no_link_flag;
  const std::string &ld_flags_str = ld_flags.str();

  std::stringstream compile_ss;
  compile_ss << "// clang++ -Wno-c++11-narrowing -c -o " << o_filename << ' ' << filename << cxx_flags_str;
  std::stringstream link_ss;
  link_ss << "// clang++ -Wno-c++11-narrowing -o " << executable_name << ' ' << o_filename << ' ' << object_files
          << ld_flags_str;

  std::string seed_dir = executable_name + "_seed";
  std::string artifact_dir = executable_name + "_art/";

  std::stringstream run_ss, repl_ss;
  if (for_libfuzzer) {
    run_ss << "// mkdir -p " << seed_dir
           << " && mkdir -p " << artifact_dir
           << " && truncate -s 1k " << seed_dir << "/init"
           << " && timeout 300s ./" << executable_name << " -max_total_time=300 -ignore_crashes=1 -fork=1"
                                                          << " -artifact_prefix=" << artifact_dir << ' ' << seed_dir;
    repl_ss << "// ./" << executable_name << " $(find ./" << seed_dir << " -type f -name \"*\")";
  } else {
    repl_ss << "// ./" << executable_name;
  }

  target << "// Run instruction: \n"
         << run_ss.str() << '\n';
  target << '\n';

  target << "// Replay instruction: \n"
         << repl_ss.str() << '\n';
  target << '\n';

  target << "// Compile instruction:\n"
         << compile_ss.str() << '\n'
         << link_ss.str() << '\n';

}
void ReplayDriverWriter::AppendLibFuzzerHelperFunctions(std::ofstream &target) {
  target << '\n';
  target << "unsigned long max_size; char *buff, *ptr; \n";
  target << "void Init(const uint8_t *Data, size_t Size) { max_size = Size; buff = ptr = (char*) Data; }\n";
  target << "template<typename T> T Get() { size_t sz = sizeof(T);\n";
  target << "  if (ptr + sz < buff + max_size) { T value = *(T *)((void*) ptr); ptr += sz; return value; }\n";
  target << "  else { return (T) 0; }\n";
  target << "}\n\n";
}
} // namespace cxxfoozz
