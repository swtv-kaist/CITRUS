#include <iostream>
#include <utility>

#include "analyzer.hpp"
#include "clock.hpp"
#include "fuzzer.hpp"
#include "logger.hpp"
#include "model.hpp"
#include "traversal.hpp"
#include "type.hpp"
#include "util.hpp"

namespace cxxfoozz {

class ImportPPCallbacks : public clang::PPCallbacks {
 public:
  void InclusionDirective(
    clang::SourceLocation HashLoc,
    const clang::Token &IncludeTok,
    llvm::StringRef FileName,
    bool IsAngled,
    clang::CharSourceRange FilenameRange,
    const clang::FileEntry *File,
    llvm::StringRef SearchPath,
    llvm::StringRef RelativePath,
    const clang::Module *Imported,
    clang::SrcMgr::CharacteristicKind FileType
  ) override {
    switch (FileType) {
      case clang::SrcMgr::C_User: {
        assert(File != nullptr);
        const std::string &include_path = File->getName().str();
        const auto &ins_res = include_paths_.insert(include_path);
        if (ins_res.second) {
          include_paths_ordered_.push_back(include_path);
        }
        break;
      }
      case clang::SrcMgr::C_System:
      case clang::SrcMgr::C_ExternCSystem:
      case clang::SrcMgr::C_User_ModuleMap:
      case clang::SrcMgr::C_System_ModuleMap: {
        break;
      }
    }
    PPCallbacks::InclusionDirective(
      HashLoc,
      IncludeTok,
      FileName,
      IsAngled,
      FilenameRange,
      File,
      SearchPath,
      RelativePath,
      Imported,
      FileType);
  }
  static const std::set<std::string> &GetIncludePathsUnordered() {
    return include_paths_;
  }
  static const std::vector<std::string> &GetIncludePathsOrdered() {
    return include_paths_ordered_;
  }
 private:
  static std::set<std::string> include_paths_;
  static std::vector<std::string> include_paths_ordered_;
};
std::set<std::string> ImportPPCallbacks::include_paths_;
std::vector<std::string> ImportPPCallbacks::include_paths_ordered_;

// ##########
// # ClassTraversingResult
// #####

ClassTraversingResult::ClassTraversingResult() = default;
ClassTraversingResult::ClassTraversingResult(
  std::vector<clang::CXXRecordDecl *> record_decls,
  std::vector<clang::EnumDecl *> enum_decls,
  std::vector<clang::ClassTemplateDecl *> class_template_decls,
  std::vector<clang::FunctionDecl *> func_decls,
  std::vector<clang::FunctionTemplateDecl *> func_template_decls
)
  : record_decls_(std::move(record_decls)),
    enum_decls_(std::move(enum_decls)),
    class_template_decls_(std::move(class_template_decls)),
    func_decls_(std::move(func_decls)),
    func_template_decls_(std::move(func_template_decls)) {}
const std::vector<clang::CXXRecordDecl *> &ClassTraversingResult::GetRecordDecls() const {
  return record_decls_;
}
const std::vector<clang::EnumDecl *> &ClassTraversingResult::GetEnumDecls() const {
  return enum_decls_;
}
const std::vector<clang::ClassTemplateDecl *> &ClassTraversingResult::GetClassTemplateDecls() const {
  return class_template_decls_;
}
const std::vector<clang::FunctionDecl *> &ClassTraversingResult::GetFuncDecls() const {
  return func_decls_;
}
const std::vector<clang::FunctionTemplateDecl *> &ClassTraversingResult::GetFuncTemplateDecls() const {
  return func_template_decls_;
}

// ##########
// # ClassTraversingVisitor
// #####

bool IsFromTargetProgram(clang::SourceManager &src_manager, const clang::SourceLocation &location) {
  clang::SrcMgr::CharacteristicKind kind = src_manager.getFileCharacteristic(location);
  bool is_tgt_src = kind == clang::SrcMgr::CharacteristicKind::C_User;
  return is_tgt_src;
}

bool DeclaredInHeaderFiles(clang::SourceManager &src_manager, const clang::SourceLocation &location) {
  bool is_tgt_src = IsFromTargetProgram(src_manager, location);
  if (is_tgt_src) {
    const std::set<std::string> &include_paths = ImportPPCallbacks::GetIncludePathsUnordered();
    const std::string &dump_loc = location.printToString(src_manager);
    unsigned long loc = dump_loc.find(':');
    assert(loc != std::string::npos);
    const std::string &filename = dump_loc.substr(0, loc);
    bool is_in_header = include_paths.count(filename) > 0;
    return is_in_header;
  }
  return false;
}

ClassTraversingResult ClassTraversingVisitor::traversal_result;
bool ClassTraversingVisitor::VisitCXXRecordDecl(clang::CXXRecordDecl *d) {
  clang::SourceManager &src_manager = ast_context_.getSourceManager();
  const clang::SourceLocation &location = d->getLocation();
  bool is_class_or_struct = d->isClass() || d->isStruct();
  bool system_header = src_manager.isInSystemHeader(location);
  bool in_header = DeclaredInHeaderFiles(src_manager, location);
  if (!system_header && is_class_or_struct && in_header) {
    traversal_result.record_decls_.push_back(d);
  }
  return true;
}
bool ClassTraversingVisitor::VisitEnumDecl(clang::EnumDecl *d) {
  clang::SourceManager &src_manager = ast_context_.getSourceManager();
  const clang::SourceLocation &location = d->getLocation();
  bool system_header = src_manager.isInSystemHeader(location);
  bool in_header = DeclaredInHeaderFiles(src_manager, location);
  if (!system_header && in_header) {
    traversal_result.enum_decls_.push_back(d);
  }
  return true;
}
bool ClassTraversingVisitor::VisitClassTemplateDecl(clang::ClassTemplateDecl *d) {
  clang::SourceManager &src_manager = ast_context_.getSourceManager();
  const clang::SourceLocation &location = d->getLocation();
  bool system_header = src_manager.isInSystemHeader(location);
  bool in_header = DeclaredInHeaderFiles(src_manager, location);
  if (!system_header && in_header) {
    traversal_result.class_template_decls_.push_back(d);
  }
  return true;
}
bool ClassTraversingVisitor::VisitFunctionDecl(clang::FunctionDecl *d) {
  clang::SourceManager &src_manager = ast_context_.getSourceManager();
  const clang::SourceLocation &location = d->getLocation();
  bool system_header = src_manager.isInSystemHeader(location);
  bool is_from_tgt_prog = IsFromTargetProgram(src_manager, location);
  bool in_header = DeclaredInHeaderFiles(src_manager, location);
  bool in_impl = is_from_tgt_prog && !in_header;
  bool is_static = d->isStatic(); // static in impl cannot be invoked.

  bool is_cxx_method_decl = llvm::isa<clang::CXXMethodDecl>(d);
  if (!system_header && !is_cxx_method_decl) {
//    if (in_header || (in_impl && !is_static))
    if (in_header)
      traversal_result.func_decls_.push_back(d);
  }
  return true;
}
bool ClassTraversingVisitor::VisitFunctionTemplateDecl(clang::FunctionTemplateDecl *d) {
  clang::SourceManager &src_manager = ast_context_.getSourceManager();
  const clang::SourceLocation &location = d->getLocation();
  bool system_header = src_manager.isInSystemHeader(location);
  bool is_from_tgt_prog = IsFromTargetProgram(src_manager, location);
  bool in_header = DeclaredInHeaderFiles(src_manager, location);
  bool in_impl = is_from_tgt_prog && !in_header;

  const clang::FunctionDecl *inner_decl = d->getTemplatedDecl();
  bool is_static = inner_decl->isStatic(); // static in impl cannot be invoked.

  bool is_cxx_method_decl = llvm::isa<clang::CXXMethodDecl>(inner_decl);
  if (!system_header && !is_cxx_method_decl) {
//    if (in_header || (in_impl && !is_static)) {
    if (in_header) {
      traversal_result.func_template_decls_.push_back(d);
    }
  }
  return true;
}
ClassTraversingVisitor::ClassTraversingVisitor(clang::ASTContext &context) : ast_context_{context} {}
clang::ASTContext &ClassTraversingVisitor::GetAstContext() const {
  return ast_context_;
}
const ClassTraversingResult &ClassTraversingVisitor::GetTraversalResult() {
  return traversal_result;
}

void ASANSafeASTTraversal(clang::CompilerInstance &ci, const std::string &filename) {
  ci.createDiagnostics(nullptr, false);
  const std::shared_ptr<clang::TargetOptions> &to = std::make_shared<clang::TargetOptions>();
  to->Triple = llvm::sys::getDefaultTargetTriple();
  clang::TargetInfo *ti = clang::TargetInfo::CreateTargetInfo(ci.getDiagnostics(), to);
  ci.setTarget(ti);

  ci.createFileManager();
  clang::FileManager &fm = ci.getFileManager();
  ci.createSourceManager(fm);
  clang::SourceManager &sm = ci.getSourceManager();

  clang::LangOptions &lo = ci.getLangOpts();
  lo.GNUMode = 1;
  lo.CXXExceptions = 1;
  lo.RTTI = 1;
  lo.Bool = 1;
  lo.CPlusPlus = 1;
  lo.CPlusPlus11 = 1;
  lo.WChar = 1;

  ci.createPreprocessor(clang::TU_Module);
  clang::Preprocessor &pp = ci.getPreprocessor();
  pp.addPPCallbacks(std::make_unique<ImportPPCallbacks>());
  const llvm::Triple &triple = pp.getTargetInfo().getTriple();

  clang::HeaderSearchOptions &hso = ci.getHeaderSearchOpts();
  std::vector<std::string> include_paths{
//    "/local_home/robert/llvm-11.0.1/lib/clang/11.0.1/include",
//    "/local_home/robert/llvm-11.0.1/include/c++/v1",
    "/usr/lib/gcc/x86_64-linux-gnu/10/include",
    "/usr/local/include",
    "/usr/lib/gcc/x86_64-linux-gnu/10/include-fixed",
    "/usr/include/x86_64-linux-gnu",
    "/usr/include",
    "/usr/include/c++/10",
    "/usr/include/x86_64-linux-gnu/c++/10",
  };
  for (const auto &i : include_paths) {
    hso.AddPath(i, clang::frontend::Angled, false, false);
  }
  ApplyHeaderSearchOptions(pp.getHeaderSearchInfo(), hso, pp.getLangOpts(), triple);

  // Preprocessing error fixes
  pp.getBuiltinInfo().initializeBuiltins(pp.getIdentifierTable(), pp.getLangOpts());
  const clang::InputKind &ik_cxx = clang::InputKind{clang::Language::CXX,};
  clang::LangStandard::Kind cxx17 = clang::LangStandard::lang_cxx17;
  clang::CompilerInvocation::setLangDefaults(lo, ik_cxx, triple, pp.getPreprocessorOpts(), cxx17);
  ci.createASTContext();

  const clang::FileEntry *FileIn = fm.getFile(filename).get();
  sm.setMainFileID(sm.createFileID(FileIn, clang::SourceLocation(), clang::SrcMgr::C_User));
  ci.getDiagnosticClient().BeginSourceFile(lo, &ci.getPreprocessor());

  CxxfoozzASTConsumer consumer(ci.getASTContext());
  ParseAST(ci.getPreprocessor(), &consumer, ci.getASTContext());
}

// ##########
// # CxxfoozzASTConsumer
// #####

CxxfoozzASTConsumer::CxxfoozzASTConsumer(clang::ASTContext &context) : class_visitor_(context) {}
void CxxfoozzASTConsumer::HandleTranslationUnit(clang::ASTContext &context) {
  class_visitor_.TraverseDecl(context.getTranslationUnitDecl());
}

// ##########
// # MainFuzzingAction
// #####

std::string GetFilename(const std::string &path) {
  const std::vector<std::string> &tokens = SplitStringIntoVector(path, "/");
  int len = (int) tokens.size();
  return tokens[len - 1];
}

std::string StripExtension(const std::string &filename) {
  const std::vector<std::string> &tokens = SplitStringIntoVector(filename, ".");
  return tokens[0];
}

std::vector<std::string> ReduceIncludePaths(
  const std::vector<std::string> &include_paths,
  const std::string &current_file
) {
//  const std::string &current_filename = GetFilename(current_file);
//  const std::string &name2match = StripExtension(current_filename);
//  for (const auto &path : include_paths) {
//    const std::string &filename = GetFilename(path);
//    const std::string &name = StripExtension(filename);
//    if (name == name2match) {
//      return { path };
//    }
//  }
//  return include_paths;
  std::vector<std::string> result;
  for (const auto &path : include_paths) {
    bool is_hardcoded_lib = path.rfind("/usr/lib/gcc/x86_64-linux-gnu/", 0) == 0;
    if (!is_hardcoded_lib)
      result.push_back(path);
  }
  return result;
}

void MainFuzzingAction::ExecuteAction() {
  const std::string &input_file = getCurrentInput().getFile().str();
  bool has_been_processed = processed_files.count(input_file) > 0;
  if (has_been_processed)
    return;

  processed_files.insert(input_file);
  clang::CompilerInstance &compiler_instance = getCompilerInstance();
  compiler_instance.getPreprocessor().createPreprocessingRecord();
  clang::MangleContext *mangle_ctx = compiler_instance.getASTContext().createMangleContext();

  std::cout << "[INFO] Executing action from MainFuzzingAction\n";
//  ASANSafeASTTraversal(compiler_instance, filename.str());
  clang::ASTFrontendAction::ExecuteAction();

  const ClassTraversingResult &traversal_result = ClassTraversingVisitor::GetTraversalResult();
  const analysis::AnalysisSpec
    &analysis_spec = analysis::AnalysisSpec::FromTraversalResult(traversal_result, mangle_ctx);

  ProgramAnalyzer analyzer;
  const analysis::AnalysisResult &analysis_result = analyzer.Analyze(analysis_spec);
  const std::vector<std::shared_ptr<ClassTypeModel>> &class_tms = analysis_result.GetClassTypeModels();

  // ##########
  // # Find target class & target method
  // #####
  std::string target_class_name = cli_args->GetTargetClassName();
  std::shared_ptr<ClassType> target_class_type;
  if (target_class_name.empty()) {
    target_class_type = nullptr;
    Logger::Warn(
      "MainFuzzingAction",
      "Target class unspecified. Generating method sequence without particular target class."
    );
  } else {
    const auto &target_class_it = std::find_if(
      class_tms.begin(), class_tms.end(), [target_class_name](auto &item) {
        return item->GetQualifiedName() == target_class_name;
      });
    if (target_class_it == class_tms.end()) {
      Logger::Warn(
        "MainFuzzingAction",
        "Target class: " + target_class_name + " not found. Exiting.");
      std::cerr << "[Main] Target class: " << target_class_name << " not found!\n";
      return;
    }
    const std::shared_ptr<ClassTypeModel> &target_class_model = *target_class_it;
    target_class_type = ClassType::GetTypeByQualName(target_class_name);
  }

  const clang::ASTContext &ast_context = compiler_instance.getASTContext();
  const std::shared_ptr<ProgramContext> &program_ctx = std::make_shared<ProgramContext>(
    ast_context,
    analysis_result.GetClassTypeModels(),
    analysis_result.GetExecutables(),
    analysis_result.GetCreators(),
    analysis_result.GetEnumTypeModels(),
    analysis_result.GetInheritanceModel()
  );
  ProgramContext::SetKGlobProgramCtx(program_ctx);

  // ##########
  // # Parse compilation command
  // #####
  std::vector<std::string> extracted_cxx_flags;
  if (!compile_cmds.empty()) {
    const clang::tooling::CompileCommand &command = compile_cmds[0];
    const std::vector<std::string> &command_line = command.CommandLine;
    for (const auto &item : command_line) {
      bool is_driver_mode_flag = item.rfind("--driver-mode=", 0) == 0;
      bool is_without_link_flag = item.rfind("-c", 0) == 0;
      bool is_warning_constraint = item.rfind("-W", 0) == 0;
      bool is_output_flag = item.rfind("-o", 0) == 0;
      bool is_no_exception = item.rfind("-fno-exceptions", 0) == 0;
      if (is_driver_mode_flag || is_without_link_flag || is_warning_constraint || is_output_flag || is_no_exception)
        continue;

//      bool is_include_flag = item.rfind("-I", 0) == 0;
//      bool is_cxx_version_flag = item.rfind("-std=", 0) == 0;
      bool is_flag = item.rfind('-', 0) == 0;
      if (is_flag)
        extracted_cxx_flags.push_back(item);
    }
  }
  extracted_cxx_flags.emplace_back("-w"); // suppress all warnings

  const std::vector<std::string> &include_paths = ImportPPCallbacks::GetIncludePathsOrdered();
  const std::string &current_file = getCurrentFile().str();
//  const std::set<std::string> &reduced_includes = include_paths;
  const std::vector<std::string> &reduced_includes = ReduceIncludePaths(include_paths, current_file);

  const std::shared_ptr<CompilationContext> &compilation_ctx =
    std::make_shared<CompilationContext>(reduced_includes, extracted_cxx_flags);

  const clang::FrontendInputFile &file = getCurrentInput();
  const std::string &filename = file.getFile().str();
  FuzzingMainLoopSpec main_loop_spec{
    compiler_instance,
    *cli_args,
    filename,
    target_class_type,
    program_ctx,
    compilation_ctx,
  };

  const WallClock &main_clock = WallClock::ForLogging("Elapsed time");
  MainFuzzer main_fuzzer;
  main_fuzzer.MainLoop(main_loop_spec);
  long long int u = main_clock.MeasureElapsedInMsec();
  return;
}

std::unique_ptr<clang::ASTConsumer> MainFuzzingAction::CreateASTConsumer(
  clang::CompilerInstance &CI,
  llvm::StringRef InFile
) {
  CI.getPreprocessor().addPPCallbacks(std::make_unique<ImportPPCallbacks>());
  return std::make_unique<CxxfoozzASTConsumer>(CI.getASTContext());
}
std::shared_ptr<CLIParsedArgs> MainFuzzingAction::cli_args;
void MainFuzzingAction::SetCLIArgs(const std::shared_ptr<CLIParsedArgs> &parsed_args) {
  cli_args = parsed_args;
}
std::vector<clang::tooling::CompileCommand> MainFuzzingAction::compile_cmds;
void MainFuzzingAction::SetCompileCmds(const std::vector<clang::tooling::CompileCommand> &file_compile_cmds) {
  compile_cmds = file_compile_cmds;
}
std::set<std::string> MainFuzzingAction::processed_files;

} // namespace cxxfoozz
