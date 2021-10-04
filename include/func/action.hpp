#ifndef CXXFOOZZ__FUNC_ANALYSIS_ACTION_HPP_
#define CXXFOOZZ__FUNC_ANALYSIS_ACTION_HPP_

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/StmtVisitor.h"
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

#include <fstream>
#include <vector>
#include <map>
#include "func/api.hpp"

extern int kClassCount;
extern int kStructCount;

class FuncAnalysisASTVisitor : public clang::RecursiveASTVisitor<FuncAnalysisASTVisitor> {
 public:
  explicit FuncAnalysisASTVisitor(clang::ASTContext &context);
  virtual ~FuncAnalysisASTVisitor();
  bool VisitFunctionDecl(clang::FunctionDecl *d);
  bool VisitCXXRecordDecl(clang::CXXRecordDecl *d);
  clang::MangleContext *GetMangleContext() const;

 private:
  clang::ASTContext &ast_context_;
  clang::MangleContext *mangle_context_;
};

class FuncAnalysisASTConsumer : public clang::ASTConsumer {
 public:
  explicit FuncAnalysisASTConsumer(clang::ASTContext &context);
  void HandleTranslationUnit(clang::ASTContext &context) override;
 private:
  FuncAnalysisASTVisitor class_visitor_;
};

class FuncAnalysisAction : public clang::ASTFrontendAction {
 public:
 protected:
  void ExecuteAction() override;
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef InFile) override;
 private:
  static std::set<std::string> processed_files_;
};


#endif