#ifndef CXXFOOZZ__TRAVERSAL_HPP_
#define CXXFOOZZ__TRAVERSAL_HPP_

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Builtins.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Frontend/Utils.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Sema/Sema.h"

#include "cli.hpp"

namespace cxxfoozz {

class ClassTraversingResult {
 public:
  ClassTraversingResult();
  ClassTraversingResult(
    std::vector<clang::CXXRecordDecl *> record_decls,
    std::vector<clang::EnumDecl *> enum_decls,
    std::vector<clang::ClassTemplateDecl *> class_template_decls,
    std::vector<clang::FunctionDecl *> func_decls,
    std::vector<clang::FunctionTemplateDecl *> func_template_decls
  );
  const std::vector<clang::CXXRecordDecl *> &GetRecordDecls() const;
  const std::vector<clang::EnumDecl *> &GetEnumDecls() const;
  const std::vector<clang::ClassTemplateDecl *> &GetClassTemplateDecls() const;
  const std::vector<clang::FunctionDecl *> &GetFuncDecls() const;
  const std::vector<clang::FunctionTemplateDecl *> &GetFuncTemplateDecls() const;

 private:
  std::vector<clang::CXXRecordDecl *> record_decls_;
  std::vector<clang::EnumDecl *> enum_decls_;
  std::vector<clang::ClassTemplateDecl *> class_template_decls_;
  std::vector<clang::FunctionDecl *> func_decls_;
  std::vector<clang::FunctionTemplateDecl *> func_template_decls_;
  friend class ClassTraversingVisitor;
};

class ClassTraversingVisitor : public clang::RecursiveASTVisitor<ClassTraversingVisitor> {
 public:
  explicit ClassTraversingVisitor(clang::ASTContext &context);

  bool VisitCXXRecordDecl(clang::CXXRecordDecl *d);
  bool VisitEnumDecl(clang::EnumDecl *d);
  bool VisitClassTemplateDecl(clang::ClassTemplateDecl *d);
  bool VisitFunctionDecl(clang::FunctionDecl *d);
  bool VisitFunctionTemplateDecl(clang::FunctionTemplateDecl *d);

  inline clang::ASTContext &GetAstContext() const;
  static const ClassTraversingResult &GetTraversalResult();

 private:
  clang::ASTContext &ast_context_;
  static ClassTraversingResult traversal_result;
};

class CxxfoozzASTConsumer : public clang::ASTConsumer {
 public:
  explicit CxxfoozzASTConsumer(clang::ASTContext &context);
  void HandleTranslationUnit(clang::ASTContext &context) override;
 private:
  ClassTraversingVisitor class_visitor_;
};

class MainFuzzingAction : public clang::ASTFrontendAction {
 public:
  static void SetCLIArgs(const std::shared_ptr<CLIParsedArgs> &parsed_args);
  static void SetCompileCmds(const std::vector<clang::tooling::CompileCommand> &compile_cmds);
 protected:
  void ExecuteAction() override;
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef InFile) override;
 private:
  static std::shared_ptr<CLIParsedArgs> cli_args;
  static std::vector<clang::tooling::CompileCommand> compile_cmds;
  static std::set<std::string> processed_files;
};

} // namespace cxxfoozz

#endif //CXXFOOZZ__TRAVERSAL_HPP_
