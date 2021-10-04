#ifndef CXXFOOZZ_INCLUDE_ANALYZER_HPP_
#define CXXFOOZZ_INCLUDE_ANALYZER_HPP_

#include "model.hpp"
#include "traversal.hpp"

namespace cxxfoozz {

namespace analysis {

class AnalysisSpec {
 public:
  static AnalysisSpec FromTraversalResult(
    const ClassTraversingResult &traversal_result,
    clang::MangleContext *mangle_ctx
  );
  const std::vector<clang::CXXRecordDecl *> &GetRecordDecls() const;
  const std::vector<clang::EnumDecl *> &GetEnumDecls() const;
  const std::vector<clang::ClassTemplateDecl *> &GetClassTemplateDecls() const;
  const std::vector<clang::FunctionDecl *> &GetFuncDecls() const;
  const std::vector<clang::FunctionTemplateDecl *> &GetFuncTemplateDecls() const;
  clang::MangleContext *GetMangleCtx() const;

 private:
  std::vector<clang::CXXRecordDecl *> record_decls_;
  std::vector<clang::EnumDecl *> enum_decls_;
  std::vector<clang::ClassTemplateDecl *> class_template_decls_;
  std::vector<clang::FunctionDecl *> func_decls_;
  std::vector<clang::FunctionTemplateDecl *> func_template_decls_;
  clang::MangleContext *mangle_ctx_;
};

class AnalysisResult {
 public:
  AnalysisResult() = default;
  AnalysisResult(
    std::vector<std::shared_ptr<ClassTypeModel>> class_type_models,
    std::vector<std::shared_ptr<Executable>> executables,
    std::vector<std::shared_ptr<Creator>> creators,
    std::vector<std::shared_ptr<EnumTypeModel>> enum_type_models,
    std::shared_ptr<InheritanceTreeModel> inheritance_model
  );

  const std::vector<std::shared_ptr<ClassTypeModel>> &GetClassTypeModels() const;
  const std::vector<std::shared_ptr<Executable>> &GetExecutables() const;
  const std::vector<std::shared_ptr<Creator>> &GetCreators() const;
  const std::vector<std::shared_ptr<EnumTypeModel>> &GetEnumTypeModels() const;
  const std::shared_ptr<InheritanceTreeModel> &GetInheritanceModel() const;

 private:
  std::vector<std::shared_ptr<ClassTypeModel>> class_type_models_;
  std::vector<std::shared_ptr<Executable>> executables_;
  std::vector<std::shared_ptr<Creator>> creators_;
  std::vector<std::shared_ptr<EnumTypeModel>> enum_type_models_;
  std::shared_ptr<InheritanceTreeModel> inheritance_model_;
};

} // namespace analysis


class ProgramAnalyzer {
 public:
  analysis::AnalysisResult Analyze(const analysis::AnalysisSpec &spec);
 private:

};

} // namespace cxxfoozz

#endif //CXXFOOZZ_INCLUDE_ANALYZER_HPP_
