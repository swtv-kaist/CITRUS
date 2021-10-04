#include "program-context.hpp"

namespace cxxfoozz {

// ##########
// # ProgramContext
// #####

std::shared_ptr<ProgramContext> ProgramContext::kGlobProgramCtx = nullptr;
ProgramContext::ProgramContext(
  const clang::ASTContext &ast_context,
  const std::vector<std::shared_ptr<ClassTypeModel>> &class_type_models,
  const std::vector<std::shared_ptr<Executable>> &executables,
  const std::vector<std::shared_ptr<Creator>> &creators,
  const std::vector<std::shared_ptr<EnumTypeModel>> &enum_type_models,
  const std::shared_ptr<InheritanceTreeModel> &inheritance_model
)
  : class_type_models_(class_type_models),
    executables_(executables),
    creators_(creators),
    enum_type_models_(enum_type_models),
    inheritance_model_(inheritance_model),
    ast_context_(ast_context) {}
const std::vector<std::shared_ptr<ClassTypeModel>> &ProgramContext::GetClassTypeModels() const {
  return class_type_models_;
}
const std::vector<std::shared_ptr<Executable>> &ProgramContext::GetExecutables() const {
  return executables_;
}
const std::vector<std::shared_ptr<Creator>> &ProgramContext::GetCreators() const {
  return creators_;
}
const std::vector<std::shared_ptr<EnumTypeModel>> &ProgramContext::GetEnumTypeModels() const {
  return enum_type_models_;
}
const std::shared_ptr<InheritanceTreeModel> &ProgramContext::GetInheritanceModel() const {
  return inheritance_model_;
}
const std::shared_ptr<ProgramContext> &ProgramContext::GetKGlobProgramCtx() {
  assert(kGlobProgramCtx != nullptr);
  return kGlobProgramCtx;
}
void ProgramContext::SetKGlobProgramCtx(const std::shared_ptr<ProgramContext> &k_glob_program_ctx) {
  kGlobProgramCtx = k_glob_program_ctx;
}
const clang::ASTContext &ProgramContext::GetAstContext() const {
  return ast_context_;
}

} // namespace cxxfoozz