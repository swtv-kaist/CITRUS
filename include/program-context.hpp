#ifndef CXXFOOZZ_SRC_ANALYZER_CPP_PROGRAM_CONTEXT_HPP_
#define CXXFOOZZ_SRC_ANALYZER_CPP_PROGRAM_CONTEXT_HPP_

#include "model.hpp"

namespace cxxfoozz {

class ProgramContext {
 public:
  ProgramContext(
    const clang::ASTContext &ast_context,
    const std::vector<std::shared_ptr<ClassTypeModel>> &class_type_models,
    const std::vector<std::shared_ptr<Executable>> &executables,
    const std::vector<std::shared_ptr<Creator>> &creators,
    const std::vector<std::shared_ptr<EnumTypeModel>> &enum_type_models,
    const std::shared_ptr<InheritanceTreeModel> &inheritance_model
  );
  const clang::ASTContext &GetAstContext() const;
  const std::vector<std::shared_ptr<cxxfoozz::ClassTypeModel>> &GetClassTypeModels() const;
  const std::vector<std::shared_ptr<cxxfoozz::Executable>> &GetExecutables() const;
  const std::vector<std::shared_ptr<cxxfoozz::Creator>> &GetCreators() const;
  const std::vector<std::shared_ptr<cxxfoozz::EnumTypeModel>> &GetEnumTypeModels() const;
  const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &GetInheritanceModel() const;
  static const std::shared_ptr<ProgramContext> &GetKGlobProgramCtx();
  static void SetKGlobProgramCtx(const std::shared_ptr<ProgramContext> &k_glob_program_ctx);

 private:
  const clang::ASTContext &ast_context_;
  const std::vector<std::shared_ptr<cxxfoozz::ClassTypeModel>> &class_type_models_;
  const std::vector<std::shared_ptr<cxxfoozz::Executable>> &executables_;
  const std::vector<std::shared_ptr<cxxfoozz::Creator>> &creators_;
  const std::vector<std::shared_ptr<cxxfoozz::EnumTypeModel>> &enum_type_models_;
  const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &inheritance_model_;
  static std::shared_ptr<ProgramContext> kGlobProgramCtx;
};

} // namespace cxxfoozz

#endif //CXXFOOZZ_SRC_ANALYZER_CPP_PROGRAM_CONTEXT_HPP_
