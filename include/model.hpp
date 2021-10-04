#ifndef CXXFOOZZ_INCLUDE_MODEL_HPP_
#define CXXFOOZZ_INCLUDE_MODEL_HPP_

#include <utility>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
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

#include "bpstd/optional.hpp"

/**
 * Classes in this file represent the "model" of the target program
 * Obtained from static analysis (i.e. clang AST traversal)
 */

namespace cxxfoozz {

enum class TemplateTypeParamVariant {
  kTypeParam = 0,
  kNonTypeParam,
};

class TemplateTypeParam {
 public:
  TemplateTypeParam(
    std::string name,
    int pos,
    TemplateTypeParamVariant variant
  );
  const std::string &GetName() const;
  int GetPos() const;
  TemplateTypeParamVariant GetVariant() const;
  std::string DebugString() const;

 private:
  std::string name_;
  int pos_;
  TemplateTypeParamVariant variant_;
};

class TemplateTypeParamList {
 public:
  TemplateTypeParamList();
  explicit TemplateTypeParamList(std::vector<TemplateTypeParam> list);
  const std::vector<TemplateTypeParam> &GetList() const;
  bool IsEmpty() const;
  std::string DebugString() const;
 private:
  std::vector<TemplateTypeParam> list_;
};

class FieldModel {
 public:
  FieldModel(
    std::string name,
    const clang::QualType &type,
    bool is_public
  );
  const std::string &GetName() const;
  const clang::QualType &GetType() const;
  bool IsPublic() const;

 private:
  std::string name_;
  clang::QualType type_;
  bool is_public_;
};

enum class ClassTypeModelVariant {
  kClass = 0,
  kStruct,
};

class ClassTypeModel;

class InheritanceTreeModel {
 public:
  explicit InheritanceTreeModel(
    std::map<std::shared_ptr<ClassTypeModel>, std::set<std::shared_ptr<ClassTypeModel>>> inheritances
  );
  std::set<std::shared_ptr<ClassTypeModel>> LookupBaseClasses(const std::shared_ptr<ClassTypeModel> &tgt) const;
  std::set<std::shared_ptr<ClassTypeModel>> LookupSubClasses(const std::shared_ptr<ClassTypeModel> &tgt);
 private:
  std::map<std::shared_ptr<ClassTypeModel>, std::set<std::shared_ptr<ClassTypeModel>>> parent_classes_;
  std::map<std::shared_ptr<ClassTypeModel>, std::set<std::shared_ptr<ClassTypeModel>>> subclasses_;
};

class ITMBuilder {
 public:
  void AddRelation(clang::CXXRecordDecl *clz, const std::vector<clang::CXXRecordDecl *> &parent_classes);
  std::shared_ptr<InheritanceTreeModel> Build(const std::vector<std::shared_ptr<ClassTypeModel>> &models);
 private:
  std::map<clang::CXXRecordDecl *, std::vector<clang::CXXRecordDecl *>> parent_classes_;
};

class ClassTypeModel {
 public:
  ClassTypeModel(
    std::string name,
    std::string qual_name,
    clang::CXXRecordDecl *clang_decl,
    ClassTypeModelVariant variant
  );
  ClassTypeModel(
    std::string name,
    std::string qual_name,
    clang::CXXRecordDecl *clang_decl,
    const bpstd::optional<clang::ClassTemplateDecl *> &class_template_decl,
    ClassTypeModelVariant variant
  );

  const std::string &GetName() const;
  const std::string &GetQualifiedName() const;
  clang::CXXRecordDecl *GetClangDecl() const;
  const bpstd::optional<clang::ClassTemplateDecl *> &GetClassTemplateDecl() const;
  const TemplateTypeParamList &GetTemplateParamList() const;
  ClassTypeModelVariant GetVariant() const;
  void SetTemplateParamList(const TemplateTypeParamList &template_param_list);
  bool IsTemplatedClass() const;
  void AppendField(const FieldModel &field);
  const std::vector<FieldModel> &GetFields() const;
  bool IsAllPublicFields() const;
  bool IsHasPublicCctor() const;
  void SetHasPublicCctor(bool has_public_cctor);

 private:
  std::string name_;
  std::string qualified_name_;
  clang::CXXRecordDecl *clang_decl_;
  bpstd::optional<clang::ClassTemplateDecl *> class_template_decl_;
  TemplateTypeParamList template_param_list_;
  ClassTypeModelVariant variant_;
  std::vector<FieldModel> fields_;
  bool has_public_cctor_;
};

class EnumTypeModel {
 public:
  EnumTypeModel(
    std::string name,
    std::string qual_name,
    std::vector<std::string> variants,
    clang::EnumDecl *enum_decl
  );
  const std::string &GetName() const;
  const std::string &GetQualifiedName() const;
  const std::vector<std::string> &GetVariants() const;
  clang::EnumDecl *GetEnumDecl() const;
 private:
  std::string name_;
  std::string qualified_name_;
  std::vector<std::string> variants_;
  clang::EnumDecl *enum_decl_;
};

enum class ExecutableVariant {
  kMethod = 0,
  kConstructor,
};

class Executable {
 public:
  Executable(
    std::string name,
    std::string qualified_name,
    ExecutableVariant executable_variant,
    std::shared_ptr<ClassTypeModel> owner,
    const bpstd::optional<clang::QualType> &return_type,
    std::vector<clang::QualType> arguments,
    bool is_creator,
    bool is_not_require_invoking_obj,
    std::string mangled_name
  );
  virtual ~Executable();
  static std::shared_ptr<Executable> MakeMethodExecutable(
    const std::shared_ptr<ClassTypeModel> &class_type_model,
    const std::vector<clang::QualType> &arguments,
    clang::CXXMethodDecl *method,
    clang::MangleContext *mangle_ctx
  );
  static std::shared_ptr<Executable> MakeImplicitExecutable(
    const std::string &name,
    const std::string &qual_name,
    ExecutableVariant executable_variant,
    std::shared_ptr<ClassTypeModel> owner,
    const bpstd::optional<clang::QualType> &return_type,
    std::vector<clang::QualType> arguments,
    bool is_creator,
    bool is_not_require_invoking_obj
  );
  static std::shared_ptr<Executable> MakeExternalExecutable(
    const std::vector<clang::QualType> &arguments,
    clang::FunctionDecl *func_decl,
    clang::MangleContext *mangle_ctx
  );

  const std::string GetName() const;
  const std::string &GetQualifiedName() const;
  ExecutableVariant GetExecutableVariant() const;
  const std::shared_ptr<ClassTypeModel> &GetOwner() const;
  const bpstd::optional<clang::QualType> &GetReturnType() const;
  const std::vector<clang::QualType> &GetArguments() const;
  bool IsCreator() const;
  bool IsNotRequireInvokingObj() const;
  bool IsTemplatedExecutable() const;
  const TemplateTypeParamList &GetTemplateParamList() const;
  void SetTemplateParamList(const TemplateTypeParamList &template_param_list);
  bool IsConversionDecl() const;
  void SetIsConversionDecl(bool is_conversion_decl);
  bool IsExcluded() const;
  void SetExcluded(bool excluded);
  std::string GetMangledName() const;

  bool IsMember() const;
  virtual std::string DebugString() const;

 private:
  std::string name_; // without qualifier
  std::string qualified_name_;
  ExecutableVariant executable_variant_;
  std::shared_ptr<ClassTypeModel> owner_;
  bpstd::optional<clang::QualType> return_type_;
  std::vector<clang::QualType> arguments_;
  bool is_creator_ = false;
  bool is_not_require_invoking_obj_ = false;
  TemplateTypeParamList template_param_list_;
  bool is_conversion_decl_ = false;
  bool excluded_ = false;
  std::string mangled_name_;
};

enum class CreatorVariant {
  kConstructor = 0,
  kStaticFactory,
  kMethodWithReferenceArg,
};

class Creator : public Executable {
 public:
  Creator(
    std::string name,
    std::string qualified_name,
    ExecutableVariant executable_variant,
    const std::shared_ptr<ClassTypeModel> &owner,
    const bpstd::optional<clang::QualType> &return_type,
    const std::vector<clang::QualType> &arguments,
    CreatorVariant creator_variant,
    std::shared_ptr<ClassTypeModel> target_class,
    bool is_not_require_invoking_obj,
    const std::string &mangled_name
  );
  virtual ~Creator();
  static std::shared_ptr<Creator> MakeConstructorCreator(
    const std::shared_ptr<ClassTypeModel> &class_type_model,
    const std::vector<clang::QualType> &arguments,
    clang::CXXMethodDecl *method
  );
  static std::shared_ptr<Creator> MakeStaticFactoryCreator(
    const std::shared_ptr<ClassTypeModel> &owner,
    const std::shared_ptr<ClassTypeModel> &target_cls,
    const std::vector<clang::QualType> &arguments,
    clang::CXXMethodDecl *method,
    clang::MangleContext *mangle_ctx
  );
  static std::shared_ptr<Creator> MakeExternalCreator(
    const std::shared_ptr<ClassTypeModel> &target_cls,
    const std::vector<clang::QualType> &arguments,
    clang::FunctionDecl *func_decl,
    clang::MangleContext *mangle_ctx
  );
  static std::shared_ptr<Creator> MakeImplicitDefaultCtor(
    const std::shared_ptr<ClassTypeModel> &owner
  );
  static std::shared_ptr<Creator> MakeImplicitCtorByFields(const std::shared_ptr<ClassTypeModel> &owner);

  CreatorVariant GetCreatorVariant() const;
  const std::shared_ptr<ClassTypeModel> &GetTargetClass() const;
  std::string DebugString() const override;

 private:
  CreatorVariant creator_variant_;
  std::shared_ptr<ClassTypeModel> target_class_; // Note: owner != target_class
};

} // namespace cxxfoozz
#endif //CXXFOOZZ_INCLUDE_MODEL_HPP_
