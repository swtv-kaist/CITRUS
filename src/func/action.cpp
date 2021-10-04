#include <iostream>
#include "func/action.hpp"

int kClassCount = 0;
int kStructCount = 0;

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

FuncAnalysisASTConsumer::FuncAnalysisASTConsumer(clang::ASTContext &context) : class_visitor_(context) {}
void FuncAnalysisASTConsumer::HandleTranslationUnit(clang::ASTContext &context) {
  clang::TranslationUnitDecl *translation_unit_decl = context.getTranslationUnitDecl();
  class_visitor_.TraverseDecl(translation_unit_decl);
//  printf("Current size: %d\n", (int) kGlobalSummary.size());
}

std::set<std::string> FuncAnalysisAction::processed_files_;
void FuncAnalysisAction::ExecuteAction() {
  const std::string &input_file = getCurrentInput().getFile().str();
  bool has_been_processed = processed_files_.count(input_file) > 0;
  if (has_been_processed)
    return;
  processed_files_.insert(input_file);

  clang::CompilerInstance &compiler_instance = getCompilerInstance();
  compiler_instance.getPreprocessor().createPreprocessingRecord();

  clang::ASTFrontendAction::ExecuteAction();
}

std::unique_ptr<clang::ASTConsumer> FuncAnalysisAction::CreateASTConsumer(
  clang::CompilerInstance &CI,
  llvm::StringRef InFile
) {
  CI.getPreprocessor().addPPCallbacks(std::make_unique<ImportPPCallbacks>());
  clang::ASTContext &ast_ctx = CI.getASTContext();
  return std::make_unique<FuncAnalysisASTConsumer>(ast_ctx);
}

//bool IsFromTargetProgram(clang::SourceManager &src_manager, const clang::SourceLocation &location) {
//  clang::SrcMgr::CharacteristicKind kind = src_manager.getFileCharacteristic(location);
//  bool is_tgt_src = kind == clang::SrcMgr::CharacteristicKind::C_User;
//  return is_tgt_src;
//}
//
//bool DeclaredInHeaderFiles(clang::SourceManager &src_manager, const clang::SourceLocation &location) {
//  bool is_tgt_src = IsFromTargetProgram(src_manager, location);
//  if (is_tgt_src) {
//    const std::set<std::string> &include_paths = ImportPPCallbacks::GetIncludePathsUnordered();
//    const std::string &dump_loc = location.printToString(src_manager);
//    unsigned long loc = dump_loc.find(':');
//    assert(loc != std::string::npos);
//    const std::string &filename = dump_loc.substr(0, loc);
//    bool is_in_header = include_paths.count(filename) > 0;
//    return is_in_header;
//  }
//  return false;
//}

class StatementVisitor : public clang::RecursiveASTVisitor<StatementVisitor> {
 public:
  StatementVisitor(const FuncAnalysisASTVisitor *visitor) : main_visitor_(visitor) {}
  bool VisitIfStmt(clang::IfStmt *d) {
    result_.controls_++;
    return true;
  }
  bool VisitWhileStmt(clang::WhileStmt *d) {
    result_.controls_++;
    return true;
  }
  bool VisitForStmt(clang::ForStmt *d) {
    result_.controls_++;
    return true;
  }
  bool VisitDoStmt(clang::DoStmt *d) {
    result_.controls_++;
    return true;
  }
  bool VisitCaseStmt(clang::CaseStmt *d) {
    result_.switch_cases_++;
    return true;
  }
  bool VisitConditionalOperator(clang::ConditionalOperator *d) {
    result_.cond_expr_++;
    return true;
  }
  bool VisitBinaryOperator(clang::BinaryOperator *d) {
    if (d->isLogicalOp())
      result_.short_cirs_++;
    return true;
  }
  bool VisitCallExpr(clang::CallExpr *e) {
    clang::FunctionDecl *call_func = e->getDirectCallee();
    if (call_func != nullptr) {
      clang::MangleContext *mangle_ctx = main_visitor_->GetMangleContext();
      const std::string &mangled_func = MangleFunctionDecl(call_func, mangle_ctx);
      result_.calls_.push_back(mangled_func);
    }
    return true;
  }

  const StatementVisitorResult &GetResult() const {
    return result_;
  }
 private:
  StatementVisitorResult result_;
  const FuncAnalysisASTVisitor *main_visitor_;
};

FuncAnalysisASTVisitor::FuncAnalysisASTVisitor(clang::ASTContext &context) : ast_context_(context) {
  mangle_context_ = context.createMangleContext();
}
bool FuncAnalysisASTVisitor::VisitFunctionDecl(clang::FunctionDecl *d) {
  clang::SourceManager &src_manager = ast_context_.getSourceManager();
  const clang::SourceLocation &location = d->getLocation();
  bool system_header = src_manager.isInSystemHeader(location);
  bool has_definition = d->isDefined();

  if (!system_header && has_definition) {
    clang::Stmt *body_stmt = d->getBody();
    if (auto *comp_stmt = llvm::dyn_cast_or_null<clang::CompoundStmt>(body_stmt)) {
      StatementVisitor stmt_visitor{this};
      stmt_visitor.TraverseStmt(body_stmt);
      const StatementVisitorResult &traversal_result = stmt_visitor.GetResult();

      const std::string &mangled_name = MangleFunctionDecl(d, mangle_context_);
      kGlobalSummary[mangled_name] = traversal_result;

      const clang::SourceLocation &src_loc_begin = comp_stmt->getLBracLoc();
      const clang::SourceLocation &src_loc_end = comp_stmt->getRBracLoc();
      unsigned int begin = src_manager.getExpansionLineNumber(src_loc_begin);
      unsigned int end = src_manager.getExpansionLineNumber(src_loc_end);
      unsigned int count_lines = end - begin + 1;
      kFunctionBodyLoC[mangled_name] = (int) count_lines;
    }
  }
  return true;
}

bool FuncAnalysisASTVisitor::VisitCXXRecordDecl(clang::CXXRecordDecl *d) {
  clang::SourceManager &src_manager = ast_context_.getSourceManager();
  const clang::SourceLocation &location = d->getLocation();
  bool is_class_or_struct = d->isClass() || d->isStruct();
  bool system_header = src_manager.isInSystemHeader(location);
  bool is_defined = d->hasDefinition();
  if (!system_header && is_class_or_struct && is_defined) {
    if (d->isClass()) ++kClassCount;
    else if (d->isStruct()) ++kStructCount;
  }
  return true;
}
FuncAnalysisASTVisitor::~FuncAnalysisASTVisitor() {
  delete mangle_context_;
}
clang::MangleContext *FuncAnalysisASTVisitor::GetMangleContext() const {
  return mangle_context_;
}
