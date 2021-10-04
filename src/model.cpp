#include "model.hpp"
#include "util.hpp"
#include "func/api.hpp"

#include <iostream>
#include <sstream>
#include <utility>

namespace cxxfoozz {

// ##########
// # ClassTypeModel
// #####

ClassTypeModel::ClassTypeModel(
  std::string name,
  std::string qual_name,
  clang::CXXRecordDecl *clang_decl,
  ClassTypeModelVariant variant
)
  : ClassTypeModel(std::move(name), std::move(qual_name), clang_decl, bpstd::nullopt, variant) {}
ClassTypeModel::ClassTypeModel(
  std::string name,
  std::string qual_name,
  clang::CXXRecordDecl *clang_decl,
  const bpstd::optional<clang::ClassTemplateDecl *> &class_template_decl,
  ClassTypeModelVariant variant
)
  : name_(std::move(name)),
    qualified_name_(std::move(qual_name)),
    clang_decl_(clang_decl),
    class_template_decl_(class_template_decl),
    variant_(variant),
    has_public_cctor_(true) {}
const std::string &ClassTypeModel::GetQualifiedName() const {
  return qualified_name_;
}
clang::CXXRecordDecl *ClassTypeModel::GetClangDecl() const {
  return clang_decl_;
}
const bpstd::optional<clang::ClassTemplateDecl *> &ClassTypeModel::GetClassTemplateDecl() const {
  return class_template_decl_;
}
bool ClassTypeModel::IsTemplatedClass() const {
  return class_template_decl_.has_value() && !template_param_list_.IsEmpty();
}
const TemplateTypeParamList &ClassTypeModel::GetTemplateParamList() const {
  return template_param_list_;
}
void ClassTypeModel::SetTemplateParamList(const TemplateTypeParamList &template_param_list) {
  template_param_list_ = template_param_list;
}
ClassTypeModelVariant ClassTypeModel::GetVariant() const {
  return variant_;
}
void ClassTypeModel::AppendField(const FieldModel &field) {
  fields_.push_back(field);
}
const std::vector<FieldModel> &ClassTypeModel::GetFields() const {
  return fields_;
}
bool ClassTypeModel::IsAllPublicFields() const {
  return std::all_of(
    fields_.begin(), fields_.end(), [](const FieldModel &i) {
      return i.IsPublic();
    });
}
const std::string &ClassTypeModel::GetName() const {
  return name_;
}
bool ClassTypeModel::IsHasPublicCctor() const {
  return has_public_cctor_;
}
void ClassTypeModel::SetHasPublicCctor(bool has_public_cctor) {
  has_public_cctor_ = has_public_cctor;
}

// ##########
// # EnumTypeModel
// #####

EnumTypeModel::EnumTypeModel(
  std::string name,
  std::string qual_name,
  std::vector<std::string> variants,
  clang::EnumDecl *enum_decl
)
  : name_(std::move(name)),
    qualified_name_(std::move(qual_name)),
    variants_(std::move(variants)),
    enum_decl_(enum_decl) {}
const std::string &EnumTypeModel::GetQualifiedName() const {
  return qualified_name_;
}
const std::vector<std::string> &EnumTypeModel::GetVariants() const {
  return variants_;
}
clang::EnumDecl *EnumTypeModel::GetEnumDecl() const {
  return enum_decl_;
}
const std::string &EnumTypeModel::GetName() const {
  return name_;
}

// ##########
// # Executable
// #####

// not working because signature could have default values
//std::string ExtractSignature(clang::FunctionDecl *decl) {
//  clang::ASTContext &ctx = decl->getASTContext();
//  clang::SourceManager &mgr = ctx.getSourceManager();
//  clang::SourceRange decl_range = decl->getSourceRange();
//  bool has_body = decl->hasBody();
//  if (has_body) {
//    const clang::SourceRange &body_range = decl->getBody()->getSourceRange();
//    decl_range = clang::SourceRange(decl_range.getBegin(), body_range.getBegin());
//  }
//  const clang::CharSourceRange &token_range = clang::CharSourceRange::getTokenRange(decl_range);
//  const clang::LangOptions &lang_opts = ctx.getLangOpts();
//  const clang::StringRef &s = clang::Lexer::getSourceText(token_range, mgr, lang_opts);
//  std::string result = s.str();
//
//  int len = (int) result.length();
//  if (result[len - 1] == '{') result.pop_back();
//  return StringStrip(result);
//}
std::string ExtractSignature(clang::FunctionDecl *decl) {
  return "";
}

Executable::Executable(
  std::string name,
  std::string qualified_name,
  ExecutableVariant executable_variant,
  std::shared_ptr<ClassTypeModel> owner,
  const bpstd::optional<clang::QualType> &return_type,
  std::vector<clang::QualType> arguments,
  bool is_creator,
  bool is_not_require_invoking_obj,
  std::string mangled_name
)
  : name_(std::move(name)),
    qualified_name_(std::move(qualified_name)),
    executable_variant_(executable_variant),
    owner_(std::move(owner)),
    return_type_(return_type),
    arguments_(std::move(arguments)),
    is_creator_(is_creator),
    is_not_require_invoking_obj_(is_not_require_invoking_obj),
    excluded_(false),
    mangled_name_(std::move(mangled_name)) {}

std::shared_ptr<Executable> Executable::MakeImplicitExecutable(
  const std::string &name,
  const std::string &qual_name,
  ExecutableVariant executable_variant,
  std::shared_ptr<ClassTypeModel> owner,
  const bpstd::optional<clang::QualType> &return_type,
  std::vector<clang::QualType> arguments,
  bool is_creator,
  bool is_not_require_invoking_obj
) {
  const std::string &mangled_name = ""; // because implicit;
  return std::make_shared<Executable>(
    name,
    qual_name,
    executable_variant,
    std::move(owner),
    return_type,
    std::move(arguments),
    is_creator,
    is_not_require_invoking_obj,
    mangled_name
  );
}
std::shared_ptr<Executable> Executable::MakeMethodExecutable(
  const std::shared_ptr<ClassTypeModel> &class_type_model,
  const std::vector<clang::QualType> &arguments,
  clang::CXXMethodDecl *method,
  clang::MangleContext *mangle_ctx
) {
  const std::string &name = method->getNameAsString();
  const std::string &qual_name = method->getQualifiedNameAsString();
  const clang::QualType &return_type = method->getCallResultType();
  const bpstd::optional<clang::QualType> opt_return_type = bpstd::make_optional<clang::QualType>(return_type);
  const std::string &mangled_name = MangleFunctionDecl(method, mangle_ctx);

  const std::shared_ptr<Executable> &executable =
    std::make_shared<Executable>(
      name,
      qual_name,
      ExecutableVariant::kMethod,
      class_type_model,
      opt_return_type,
      arguments,
      false,
      method->isStatic(),
      mangled_name
    );

  bool is_conv_decl = llvm::isa<clang::CXXConversionDecl>(method);
  executable->SetIsConversionDecl(is_conv_decl);

  return executable;
}
std::shared_ptr<Executable> Executable::MakeExternalExecutable(
  const std::vector<clang::QualType> &arguments,
  clang::FunctionDecl *func_decl,
  clang::MangleContext *mangle_ctx
) {
  const std::string &name = func_decl->getNameAsString();
  const std::string &qual_name = func_decl->getQualifiedNameAsString();
  const clang::QualType &return_type = func_decl->getCallResultType();
  const bpstd::optional<clang::QualType> opt_return_type = bpstd::make_optional<clang::QualType>(return_type);
  const std::string &mangled_name = MangleFunctionDecl(func_decl, mangle_ctx);

  return std::make_shared<Executable>(
    name,
    qual_name,
    ExecutableVariant::kMethod,
    nullptr,
    opt_return_type,
    arguments,
    false,
    true,
    mangled_name
  );
}
ExecutableVariant Executable::GetExecutableVariant() const {
  return executable_variant_;
}
const std::shared_ptr<ClassTypeModel> &Executable::GetOwner() const {
  return owner_;
}
const bpstd::optional<clang::QualType> &Executable::GetReturnType() const {
  return return_type_;
}
const std::vector<clang::QualType> &Executable::GetArguments() const {
  return arguments_;
}
bool Executable::IsCreator() const {
  return is_creator_;
}
std::string Executable::DebugString() const {
  std::stringstream ss;
  if (executable_variant_ == ExecutableVariant::kConstructor)
    ss << owner_->GetQualifiedName() << "(ctor) -> ";
  else
    ss << return_type_.value().getAsString() << " " << qualified_name_ << " -> ";

  std::stringstream joined;
  auto it = arguments_.begin();
  for (int idx = 0; it != arguments_.end(); ++it, ++idx) {
    joined << (idx == 0 ? "" : ", ") << it->getAsString();
  }
  ss << "(" << joined.str() << ")";
  return ss.str();
}
bool Executable::IsNotRequireInvokingObj() const {
  return is_not_require_invoking_obj_;
}
const std::string &Executable::GetQualifiedName() const {
  return qualified_name_;
}
bool Executable::IsMember() const {
  return owner_ != nullptr;
}
const TemplateTypeParamList &Executable::GetTemplateParamList() const {
  return template_param_list_;
}
void Executable::SetTemplateParamList(const TemplateTypeParamList &template_param_list) {
  template_param_list_ = template_param_list;
}
bool Executable::IsTemplatedExecutable() const {
  return !template_param_list_.IsEmpty();
}
const std::string Executable::GetName() const {
  return name_;
}
bool Executable::IsConversionDecl() const {
  return is_conversion_decl_;
}
void Executable::SetIsConversionDecl(bool is_conversion_decl) {
  is_conversion_decl_ = is_conversion_decl;
}
bool Executable::IsExcluded() const {
  return excluded_;
}
void Executable::SetExcluded(bool excluded) {
  excluded_ = excluded;
}
std::string Executable::GetMangledName() const {
  return mangled_name_;
}
Executable::~Executable() = default;



// ##########
// # Creator
// #####

Creator::Creator(
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
) :
  Executable(
    std::move(name),
    std::move(qualified_name),
    executable_variant,
    owner,
    return_type,
    arguments,
    true,
    is_not_require_invoking_obj,
    mangled_name),
  creator_variant_(creator_variant),
  target_class_(std::move(target_class)) {}

CreatorVariant Creator::GetCreatorVariant() const {
  return creator_variant_;
}
const std::shared_ptr<ClassTypeModel> &Creator::GetTargetClass() const {
  return target_class_;
}

std::string Creator::DebugString() const {
  return "[CREATOR] " + Executable::DebugString();
}

std::shared_ptr<Creator> Creator::MakeConstructorCreator(
  const std::shared_ptr<ClassTypeModel> &class_type_model,
  const std::vector<clang::QualType> &arguments,
  clang::CXXMethodDecl *method
) {
  const std::string &name = class_type_model->GetName();
  const std::string &qual_name = class_type_model->GetQualifiedName();
  const std::string &mangled_name = "";
  return std::make_shared<Creator>(
    name,
    qual_name,
    ExecutableVariant::kConstructor,
    class_type_model,
    bpstd::nullopt,
    arguments,
    CreatorVariant::kConstructor,
    class_type_model,
    false,
    mangled_name
  );
}

std::shared_ptr<Creator> Creator::MakeExternalCreator(
  const std::shared_ptr<ClassTypeModel> &target_cls,
  const std::vector<clang::QualType> &arguments,
  clang::FunctionDecl *func_decl,
  clang::MangleContext *mangle_ctx
) {
  const std::string &name = func_decl->getNameAsString();
  const std::string &qual_name = func_decl->getQualifiedNameAsString();
  const clang::QualType &return_type = func_decl->getCallResultType();
  const bpstd::optional<clang::QualType> opt_return_type = bpstd::make_optional<clang::QualType>(return_type);
  const std::string &mangled_name = MangleFunctionDecl(func_decl, mangle_ctx);

  return std::make_shared<Creator>(
    name,
    qual_name,
    ExecutableVariant::kMethod,
    nullptr,
    opt_return_type,
    arguments,
    CreatorVariant::kStaticFactory,
    target_cls,
    true,
    mangled_name
  );
}

std::shared_ptr<Creator> Creator::MakeStaticFactoryCreator(
  const std::shared_ptr<ClassTypeModel> &owner,
  const std::shared_ptr<ClassTypeModel> &target_cls,
  const std::vector<clang::QualType> &arguments,
  clang::CXXMethodDecl *method,
  clang::MangleContext *mangle_ctx
) {
  const std::string &name = method->getNameAsString();
  const std::string &qual_name = method->getQualifiedNameAsString();
  const clang::QualType &return_type = method->getCallResultType();
  const bpstd::optional<clang::QualType> opt_return_type = bpstd::make_optional<clang::QualType>(return_type);
  assert(opt_return_type.has_value());
  const std::string &mangled_name = MangleFunctionDecl(method, mangle_ctx);

  return std::make_shared<Creator>(
    name,
    qual_name,
    ExecutableVariant::kMethod,
    owner,
    opt_return_type,
    arguments,
    CreatorVariant::kStaticFactory,
    target_cls,
    true,
    mangled_name
  );
}

std::shared_ptr<Creator> Creator::MakeImplicitDefaultCtor(const std::shared_ptr<ClassTypeModel> &owner) {
  const std::string &signature = "";
  return std::make_shared<Creator>(
    owner->GetName(),
    owner->GetQualifiedName(),
    ExecutableVariant::kConstructor,
    owner,
    bpstd::nullopt,
    std::vector<clang::QualType>(),
    CreatorVariant::kConstructor,
    owner,
    false,
    signature
  );
}
std::shared_ptr<Creator> Creator::MakeImplicitCtorByFields(const std::shared_ptr<ClassTypeModel> &owner) {
  const std::vector<FieldModel> &fields = owner->GetFields();
  std::vector<clang::QualType> arguments;
  std::transform(
    fields.begin(), fields.end(), std::back_inserter(arguments), [](const FieldModel &i) {
      assert(i.IsPublic());
      return i.GetType();
    });
  const std::string &signature = "";

  return std::make_shared<Creator>(
    owner->GetName(),
    owner->GetQualifiedName(),
    ExecutableVariant::kConstructor,
    owner,
    bpstd::nullopt,
    arguments,
    CreatorVariant::kConstructor,
    owner,
    false,
    signature
  );

}

Creator::~Creator() = default;

// ##########
// # TemplateTypeParam
// #####

TemplateTypeParam::TemplateTypeParam(
  std::string name,
  int pos,
  TemplateTypeParamVariant variant
) : name_(std::move(name)), pos_(pos), variant_(variant) {}
const std::string &TemplateTypeParam::GetName() const {
  return name_;
}
int TemplateTypeParam::GetPos() const {
  return pos_;
}
std::string TemplateTypeParam::DebugString() const {
  std::stringstream ss;
  ss << "<template " << pos_ << ' ' << name_ << '>';
  return ss.str();
}
TemplateTypeParamVariant TemplateTypeParam::GetVariant() const {
  return variant_;
}

// ##########
// # TemplateTypeParamList
// #####

TemplateTypeParamList::TemplateTypeParamList() = default;
TemplateTypeParamList::TemplateTypeParamList(std::vector<TemplateTypeParam> list) : list_(std::move(list)) {}
const std::vector<TemplateTypeParam> &TemplateTypeParamList::GetList() const {
  return list_;
}
bool TemplateTypeParamList::IsEmpty() const {
  return list_.empty();
}
std::string TemplateTypeParamList::DebugString() const {
  std::stringstream joined;
  auto it = list_.begin();
  for (int idx = 0; it != list_.end(); ++it, ++idx) {
    joined << (idx == 0 ? "" : ", ") << it->DebugString();
  }

  std::stringstream ss;
  ss << '[' << joined.str() << ']';
  return ss.str();
}

// ##########
// # FieldModel
// #####

FieldModel::FieldModel(
  std::string name,
  const clang::QualType &type,
  bool is_public
) : name_(std::move(name)), type_(type), is_public_(is_public) {}
const std::string &FieldModel::GetName() const {
  return name_;
}
const clang::QualType &FieldModel::GetType() const {
  return type_;
}
bool FieldModel::IsPublic() const {
  return is_public_;
}

// ##########
// # InheritanceTreeModel & Builder
// #####

void ITMBuilder::AddRelation(
  clang::CXXRecordDecl *clz,
  const std::vector<clang::CXXRecordDecl *> &parent_classes
) {
  bool has_nullptr = std::any_of(
    parent_classes.begin(), parent_classes.end(), [](const auto &item) {
      return item == nullptr;
    });
  assert(!has_nullptr);
  parent_classes_.emplace(clz, parent_classes);
}
std::shared_ptr<InheritanceTreeModel> ITMBuilder::Build(
  const std::vector<std::shared_ptr<ClassTypeModel>> &models
) {
  std::map<std::string, std::shared_ptr<ClassTypeModel>> trans_map;
  for (const auto &item : models) {
    clang::CXXRecordDecl *decl = item->GetClangDecl();
    const std::string &key = decl->getQualifiedNameAsString();
//    std::cout << "In map: " << key << std::endl;
    trans_map.emplace(key, item);
  }
  const auto &trans_func = [&trans_map](clang::CXXRecordDecl *item) {
    const std::string &key = item->getQualifiedNameAsString();
    const auto &find_it = trans_map.find(key);
    if (find_it == trans_map.end()) {
//      std::cout << "unfound " << item->getQualifiedNameAsString() << std::endl;
      return std::shared_ptr<ClassTypeModel>();
    }
    return find_it->second;
  };

  std::map<std::shared_ptr<ClassTypeModel>, std::set<std::shared_ptr<ClassTypeModel>>> result;
  for (const auto &kv_item : parent_classes_) {
    clang::CXXRecordDecl *child = kv_item.first;
    const std::vector<clang::CXXRecordDecl *> &parents = kv_item.second;

    const std::shared_ptr<ClassTypeModel> &child_tm = trans_func(child);
    assert(child_tm != nullptr);

    std::vector<std::shared_ptr<ClassTypeModel>> parents_tm;
    std::transform(parents.begin(), parents.end(), std::back_inserter(parents_tm), trans_func);

    std::set<std::shared_ptr<ClassTypeModel>> parents_set;
    for (const auto &item : parents_tm) {
      if (item == nullptr)
        continue;
      parents_set.insert(item);
    }

    result.emplace(child_tm, parents_set);
  }
  return std::make_shared<InheritanceTreeModel>(result);
}

std::set<std::shared_ptr<ClassTypeModel>> InheritanceTreeModel::LookupBaseClasses(const std::shared_ptr<ClassTypeModel> &tgt) const {
  const auto &find_it = parent_classes_.find(tgt);
  if (find_it == parent_classes_.end())
    return {};
  return find_it->second;
}
std::set<std::shared_ptr<ClassTypeModel>> InheritanceTreeModel::LookupSubClasses(const std::shared_ptr<ClassTypeModel> &tgt) {
  auto find_it = subclasses_.find(tgt);
  if (find_it == subclasses_.end()) {
    std::set<std::shared_ptr<ClassTypeModel>> subclasses;
    for (const auto &kv_item : parent_classes_) {
      const std::shared_ptr<ClassTypeModel> &child = kv_item.first;
      const std::set<std::shared_ptr<ClassTypeModel>> &parents = kv_item.second;
      if (parents.count(tgt) > 0) {
        subclasses.insert(child);
      }
    }
    subclasses_.emplace(tgt, subclasses);
    return subclasses;
  } else {
    return find_it->second;
  }

}
InheritanceTreeModel::InheritanceTreeModel(std::map<std::shared_ptr<ClassTypeModel>, std::set<std::shared_ptr<ClassTypeModel>>> inheritances)
  : parent_classes_(std::move(inheritances)) {}

} // namespace cxxfoozz

