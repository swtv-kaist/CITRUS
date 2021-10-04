#include <iostream>
#include <sstream>
#include <utility>

#include "logger.hpp"
#include "random.hpp"
#include "type.hpp"

namespace cxxfoozz {

// ##########
// # Type
// #####

Type::Type(std::string name, TypeVariant variant) : name_(std::move(name)), variant_(variant) {}
const std::string &Type::GetName() const {
  return name_;
}
TypeVariant Type::GetVariant() const {
  return variant_;
}

// ##########
// # PrimitiveType
// #####

PrimitiveType::PrimitiveType(const std::string &name, PrimitiveTypeVariant primitive_type_variant)
  : Type(name, TypeVariant::kPrimitive), primitive_type_variant_(primitive_type_variant) {}
const std::shared_ptr<PrimitiveType> PrimitiveType::kVoid =
  std::make_shared<PrimitiveType>("void", PrimitiveTypeVariant::kVoid);
const std::shared_ptr<PrimitiveType> PrimitiveType::kBoolean =
  std::make_shared<PrimitiveType>("bool", PrimitiveTypeVariant::kBoolean);
const std::shared_ptr<PrimitiveType> PrimitiveType::kShort =
  std::make_shared<PrimitiveType>("short", PrimitiveTypeVariant::kShort);
const std::shared_ptr<PrimitiveType> PrimitiveType::kCharacter =
  std::make_shared<PrimitiveType>("char", PrimitiveTypeVariant::kCharacter);
const std::shared_ptr<PrimitiveType> PrimitiveType::kWideCharacter =
  std::make_shared<PrimitiveType>("wchar_t", PrimitiveTypeVariant::kWideCharacter);
const std::shared_ptr<PrimitiveType> PrimitiveType::kInteger =
  std::make_shared<PrimitiveType>("int", PrimitiveTypeVariant::kInteger);
const std::shared_ptr<PrimitiveType> PrimitiveType::kLong =
  std::make_shared<PrimitiveType>("long", PrimitiveTypeVariant::kLong);
const std::shared_ptr<PrimitiveType> PrimitiveType::kLongLong =
  std::make_shared<PrimitiveType>("long long", PrimitiveTypeVariant::kLongLong);
const std::shared_ptr<PrimitiveType> PrimitiveType::kFloat =
  std::make_shared<PrimitiveType>("float", PrimitiveTypeVariant::kFloat);
const std::shared_ptr<PrimitiveType> PrimitiveType::kDouble =
  std::make_shared<PrimitiveType>("double", PrimitiveTypeVariant::kDouble);
const std::shared_ptr<PrimitiveType> PrimitiveType::kNullptrType =
  std::make_shared<PrimitiveType>("std::nullptr_t", PrimitiveTypeVariant::kNullptrType);
int PrimitiveType::SizeOf(const std::shared_ptr<PrimitiveType> &t) {
  if (t == kVoid)
    return 0;
  else if (t == kBoolean || t == kCharacter)
    return 1;
  else if (t == kShort)
    return 2;
  else if (t == kInteger || t == kLong || t == kFloat || t == kWideCharacter)
    return 4;
  else
    return 8;
}
PrimitiveTypeVariant PrimitiveType::GetPrimitiveTypeVariant() const {
  return primitive_type_variant_;
}

// ##########
// # ClassType
// #####

std::map<std::string, std::shared_ptr<ClassType>> ClassType::kGlobalClassTypes;
ClassType::ClassType(std::shared_ptr<ClassTypeModel> model)
  : Type(model->GetQualifiedName(), TypeVariant::kClass), model_(std::move(model)) {}
const std::shared_ptr<ClassTypeModel> &ClassType::GetModel() const {
  return model_;
}
const std::map<std::string, std::shared_ptr<ClassType>> &ClassType::GetKGlobalClassTypes() {
  return kGlobalClassTypes;
}
std::shared_ptr<ClassType> ClassType::GetTypeByQualName(const std::string &qual_name) {
  const auto &find_it = kGlobalClassTypes.find(qual_name);
  assert(find_it != kGlobalClassTypes.end());
  return find_it->second;
}
std::shared_ptr<ClassType> ClassType::GetTypeByQualNameLifted(const std::string &qual_name) {
  const auto &find_it = kGlobalClassTypes.find(qual_name);
  if (find_it == kGlobalClassTypes.end())
    return nullptr;
  return find_it->second;
}
void ClassType::Install(const std::vector<std::shared_ptr<ClassTypeModel>> &models) {
  for (const auto &m : models) {
    const std::string &qual_name = m->GetQualifiedName();
    kGlobalClassTypes.emplace(qual_name, std::make_shared<ClassType>(m));
  }
}

// ##########
// # EnumType
// #####

std::map<std::string, std::shared_ptr<EnumType>> EnumType::kGlobalEnumTypes;
EnumType::EnumType(std::shared_ptr<EnumTypeModel> model)
  : Type(model->GetQualifiedName(), TypeVariant::kEnum), model_(std::move(model)) {}
const std::map<std::string, std::shared_ptr<EnumType>> &EnumType::GetKGlobalEnumTypes() {
  return kGlobalEnumTypes;
}
const std::shared_ptr<EnumTypeModel> &EnumType::GetModel() const {
  return model_;
}
std::shared_ptr<EnumType> EnumType::GetTypeByQualName(const std::string &qual_name) {
  const auto &find_it = kGlobalEnumTypes.find(qual_name);
  if (find_it == kGlobalEnumTypes.end()) {
    return nullptr;
  }
  return find_it->second;
}
void EnumType::Install(const std::vector<std::shared_ptr<EnumTypeModel>> &models) {
  for (const auto &m : models) {
    const std::string &qual_name = m->GetQualifiedName();
    kGlobalEnumTypes.emplace(qual_name, std::make_shared<EnumType>(m));
  }
}

// ##########
// # TypeWithModifier
// #####

//enum class Modifier {
//  kConst = 0,
//  kUnsigned,
//  kPointer,
//  kRegContainerWithSize,
//  kReference,
//};

const clang::Type *DesugarType(const clang::Type *type, int &ptr_count) {
  while (true) {
    if (const auto pointer_type = llvm::dyn_cast<clang::PointerType>(type)) {
      const clang::QualType &pointee = pointer_type->getPointeeType();
      const clang::Type *pointee_stripped = pointee.getTypePtrOrNull();
      type = pointee_stripped;
      ++ptr_count;
    } else if (const auto array_type = llvm::dyn_cast<clang::ArrayType>(type)) {
      const clang::QualType &elmt_type = array_type->getElementType();
      type = elmt_type.getTypePtrOrNull();
    } else if (const auto ref_type = llvm::dyn_cast<clang::ReferenceType>(type)) {
      const clang::QualType &pointee = ref_type->getPointeeType();
      type = pointee.getTypePtrOrNull();
    } else if (const auto pack_exp_type = llvm::dyn_cast<clang::PackExpansionType>(type)) {
      const clang::QualType &pattern = pack_exp_type->getPattern();
      type = pattern.getTypePtrOrNull();
    } else if (const auto elaborated_type = llvm::dyn_cast<clang::ElaboratedType>(type)) {
      const clang::QualType &named_type = elaborated_type->getNamedType();
      type = named_type.getTypePtrOrNull();
    } else if (const auto *typedef_type = llvm::dyn_cast<clang::TypedefType>(type)) {
      if (typedef_type->isSugared()) {
        const clang::QualType &desugared_type = typedef_type->desugar();
        type = desugared_type.getTypePtrOrNull();
      }
    } else if (const auto *decltype_type = llvm::dyn_cast<clang::DecltypeType>(type)) {
      const clang::QualType &underlying_type = decltype_type->getUnderlyingType();
      type = underlying_type.getTypePtrOrNull();
    } else if (const auto *paren_type = llvm::dyn_cast<clang::ParenType>(type)) {
      const clang::QualType &qual_type = paren_type->getInnerType();
      type = qual_type.getTypePtrOrNull();
    } else {
      break;
    }
  }
  return type;
}

const clang::Type *DesugarType(const clang::Type *type) {
  int unused;
  return DesugarType(type, unused);
}

std::multiset<Modifier> ExtractModifiers(const clang::QualType &type) {
  const clang::Qualifiers &qualifiers = type.getQualifiers();
  const clang::Type *strip_type = type.getTypePtrOrNull();
  int ptr_count = 0;
  const clang::Type *desugared = DesugarType(strip_type, ptr_count);
//  strip_type->dump();
  if (desugared->isEnumeralType())
    return std::multiset<Modifier>();

  bool is_const = qualifiers.hasConst();
  bool is_unsigned = desugared->isIntegerType() && desugared->isUnsignedIntegerType() && !desugared->isBooleanType();
  bool is_pointer = strip_type->isPointerType();
  bool is_array = strip_type->isArrayType();
  bool is_reference = strip_type->isLValueReferenceType();
  bool is_rvalue_ref = strip_type->isRValueReferenceType();

  bool is_const_inner = false;
  if (is_pointer) {
    const clang::QualType &inner = strip_type->getPointeeType();
    const clang::Qualifiers &inner_qual = inner.getQualifiers();
    if (inner_qual.hasConst())
      is_const_inner = true;
    is_unsigned |= inner->isIntegerType() && inner->isUnsignedIntegerType() && !inner->isBooleanType();
  }

  std::multiset<Modifier> result;
  if (is_pointer) {
    if (is_const) result.insert(Modifier::kConstOnPointer);
    if (is_const_inner) result.insert(Modifier::kConst);
  } else {
    if (is_const) result.insert(Modifier::kConst);
  }
  if (is_unsigned) result.insert(Modifier::kUnsigned);
  for (int i = 0; i < ptr_count; i++)
    result.insert(Modifier::kPointer);
  if (is_array) result.insert(Modifier::kArray);
  if (is_reference) result.insert(Modifier::kReference);
  if (is_rvalue_ref) result.insert(Modifier::kRValueReference);

  return result;
}

// ##########
// # TWMSpec
// #####

TWMSpec::TWMSpec() = default;
const std::shared_ptr<Type> &TWMSpec::GetByType() const {
  return by_type_;
}
void TWMSpec::SetByType(const std::shared_ptr<Type> &by_type) {
  by_type_ = by_type;
}
const bpstd::optional<clang::QualType> &TWMSpec::GetByClangType() const {
  return by_clang_type_;
}
void TWMSpec::SetByClangType(const bpstd::optional<clang::QualType> &by_clang_type) {
  by_clang_type_ = by_clang_type;
}
const std::multiset<Modifier> &TWMSpec::GetAdditionalMods() const {
  return additional_mods_;
}
void TWMSpec::SetAdditionalMods(const std::multiset<Modifier> &additional_mods) {
  additional_mods_ = additional_mods;
}
const std::shared_ptr<TemplateTypeContext> &TWMSpec::GetTemplateTypeContext() const {
  return template_type_context_;
}
void TWMSpec::SetTemplateTypeContext(const std::shared_ptr<TemplateTypeContext> &template_type_context) {
  template_type_context_ = template_type_context;
}
TWMSpec TWMSpec::ByType(const std::shared_ptr<Type> &by_type, const std::shared_ptr<TemplateTypeContext> &tt_ctx) {
  TWMSpec twm_spec;
  twm_spec.SetByType(by_type);
  twm_spec.SetTemplateTypeContext(tt_ctx);
  return twm_spec;
}
TWMSpec TWMSpec::ByClangType(const clang::QualType &by_clang_type, const std::shared_ptr<TemplateTypeContext> &tt_ctx) {
  TWMSpec twm_spec;
  twm_spec.SetByClangType(bpstd::make_optional(by_clang_type));
  twm_spec.SetTemplateTypeContext(tt_ctx);
  return twm_spec;
}

// ##########
// # TypeWithModifier
// #####

TypeWithModifier::TypeWithModifier(std::shared_ptr<Type> type, std::multiset<Modifier> modifiers)
  : type_(std::move(type)), modifiers_(std::move(modifiers)), bottom_type_(false) {}

TypeWithModifier TypeWithModifier::FromSpec(const TWMSpec &spec) {
  const std::multiset<Modifier> &additional_mods = spec.GetAdditionalMods();
  if (spec.GetByType() != nullptr) {
    const std::shared_ptr<Type> &type_ptr = spec.GetByType();
    return TypeWithModifier{type_ptr, additional_mods};

  } else if (spec.GetByClangType().has_value()) {
    const clang::QualType &type = spec.GetByClangType().value();
    const std::string &u = type.getAsString();
    std::multiset<Modifier> modifiers = ExtractModifiers(type);
    modifiers.insert(additional_mods.begin(), additional_mods.end());

    const clang::Type *strip_type = type.getTypePtrOrNull();
    const auto *deref_type = DesugarType(strip_type);

    const clang::CXXRecordDecl *cxx_decl = deref_type->getAsCXXRecordDecl();
    if (auto cls_template_spc = llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(cxx_decl)) {
      const std::string &type_name = cls_template_spc->getQualifiedNameAsString();
      const clang::TemplateArgumentList &clang_inst_types = cls_template_spc->getTemplateArgs();
      std::shared_ptr<Type> template_type_ptr;

      unsigned int arg_size;
      const std::shared_ptr<STLType> &stl_type = STLType::IsInstalledSTLType(type_name);
      if (stl_type != nullptr) {
        template_type_ptr = stl_type;
        if (stl_type == STLType::kTuple)
          arg_size = clang_inst_types.size();
        else
          arg_size = stl_type->GetTemplateArgumentLength();

      } else if (STLType::IsUnhandledSTLType(type_name)) {
        Logger::Warn("Unhandled STL type: " + type_name);
        return TypeWithModifier::Bottom();

      } else {
        const std::shared_ptr<ClassType> &type_ptr = ClassType::GetTypeByQualNameLifted(type_name);
        if (type_ptr == nullptr) {
          clang::SourceManager &src_mgr = cls_template_spc->getASTContext().getSourceManager();
          const std::string &loc_str = cls_template_spc->getLocation().printToString(src_mgr);
          Logger::Warn("Unrecognized class type: " + type_name + " located in: " + loc_str);
          return TypeWithModifier::Bottom();
        }
        const std::shared_ptr<ClassTypeModel> &class_model = type_ptr->GetModel();
        assert(class_model->IsTemplatedClass());

        const TemplateTypeParamList &tt_param_list = class_model->GetTemplateParamList();
        const std::vector<TemplateTypeParam> &tt_params = tt_param_list.GetList();
        assert(tt_params.size() == clang_inst_types.size());

        template_type_ptr = type_ptr;
        arg_size = clang_inst_types.size();
      }

      std::vector<TemplateTypeInstantiation> tt_insts;
      for (int i = 0; i < arg_size; i++) {
        const clang::TemplateArgument &inst_arg = clang_inst_types[i];
        clang::TemplateArgument::ArgKind arg_kind = inst_arg.getKind();
        switch (arg_kind) {
          case clang::TemplateArgument::Type: {
            const clang::QualType &qual_type = inst_arg.getAsType();
            const TWMSpec &twm_spec = TWMSpec::ByClangType(qual_type, nullptr);
            const TypeWithModifier &twm = TypeWithModifier::FromSpec(twm_spec);
            if (twm.IsBottomType()) {
              return TypeWithModifier::Bottom();
            }
            const TemplateTypeInstantiation &tt_inst = TemplateTypeInstantiation::ForType(twm);
            tt_insts.push_back(tt_inst);
            break;
          }
          case clang::TemplateArgument::Integral: {
            const llvm::APSInt &integral = inst_arg.getAsIntegral();
            int64_t value = integral.getExtValue();
            const TemplateTypeInstantiation &tt_inst = TemplateTypeInstantiation::ForIntegral((int) value);
            tt_insts.push_back(tt_inst);
            break;
          }
          case clang::TemplateArgument::Pack: {
            const llvm::ArrayRef<clang::TemplateArgument> &pack_elmts = inst_arg.pack_elements();
            for (const auto &elem : pack_elmts) {
              assert(elem.getKind() == clang::TemplateArgument::Type);
              const clang::QualType &qual_type = elem.getAsType();
              const TWMSpec &twm_spec = TWMSpec::ByClangType(qual_type, nullptr);
              const TypeWithModifier &twm = TypeWithModifier::FromSpec(twm_spec);
              const TemplateTypeInstantiation &tt_inst = TemplateTypeInstantiation::ForType(twm);
              tt_insts.push_back(tt_inst);
            }
            break;
          }
          case clang::TemplateArgument::NullPtr: {
            const TemplateTypeInstantiation &null_inst = TemplateTypeInstantiation::ForNullptr();
            tt_insts.push_back(null_inst);
            break;
          }
          case clang::TemplateArgument::Null:
          case clang::TemplateArgument::Declaration:
          case clang::TemplateArgument::Template:
          case clang::TemplateArgument::TemplateExpansion:
          case clang::TemplateArgument::Expression:
            assert(false);
        }

      }

      TemplateTypeInstList tt_inst_list{tt_insts};
      const std::shared_ptr<TemplateTypenameSpcType> &tt_spc_type =
        TemplateTypenameSpcType::From(template_type_ptr, tt_inst_list);
      return TypeWithModifier{tt_spc_type, modifiers};

    } else if (cxx_decl != nullptr) {
      const std::string &class_name = cxx_decl->getQualifiedNameAsString();
      if (STLType::IsUnhandledSTLType(class_name)) {
        Logger::Warn("Unhandled STL type: " + class_name);
        return TypeWithModifier::Bottom();
      }

      const std::shared_ptr<ClassType> &type_ptr = ClassType::GetTypeByQualNameLifted(class_name);
      if (type_ptr == nullptr) {
        clang::SourceManager &src_mgr = cxx_decl->getASTContext().getSourceManager();
        const std::string &loc_str = cxx_decl->getLocation().printToString(src_mgr);
        Logger::Warn("Unrecognized class type: " + class_name + " located in: " + loc_str);
        return TypeWithModifier::Bottom();
      }
      return TypeWithModifier{type_ptr, modifiers};
    }

    const clang::Type *desugared = deref_type->getUnqualifiedDesugaredType();
    if (desugared->isBuiltinType()) {
      const auto *builtin_type = llvm::dyn_cast<clang::BuiltinType>(desugared);
      clang::BuiltinType::Kind builtin_kind = builtin_type->getKind();
      switch (builtin_kind) {
        using namespace clang;
        case BuiltinType::Void:
          return TypeWithModifier{PrimitiveType::kVoid, modifiers};
        case BuiltinType::Bool:
          return TypeWithModifier{PrimitiveType::kBoolean, modifiers};
        case BuiltinType::Char_U:
        case BuiltinType::UChar:
        case BuiltinType::Char_S:
        case BuiltinType::SChar:
          return TypeWithModifier{PrimitiveType::kCharacter, modifiers};
        case BuiltinType::WChar_U:
        case BuiltinType::WChar_S:
          return TypeWithModifier{PrimitiveType::kWideCharacter, modifiers};
        case BuiltinType::UShort:
        case BuiltinType::Short:
        case BuiltinType::Char16:
          return TypeWithModifier{PrimitiveType::kShort, modifiers};
        case BuiltinType::UInt:
        case BuiltinType::Int:
        case BuiltinType::Char32:
          return TypeWithModifier{PrimitiveType::kInteger, modifiers};
        case BuiltinType::ULong:
        case BuiltinType::Long:
          return TypeWithModifier{PrimitiveType::kLong, modifiers};
        case BuiltinType::ULongLong:
        case BuiltinType::LongLong:
          return TypeWithModifier{PrimitiveType::kLongLong, modifiers};
        case BuiltinType::Float:
          return TypeWithModifier{PrimitiveType::kFloat, modifiers};
        case BuiltinType::Double:
        case BuiltinType::LongDouble:
        case BuiltinType::Float16:
        case BuiltinType::BFloat16:
          return TypeWithModifier{PrimitiveType::kDouble, modifiers};
        case BuiltinType::NullPtr:
          return TypeWithModifier{PrimitiveType::kNullptrType, modifiers};
        default:
          Logger::Error("[TypeWithModifier::FromSpec]", "Unhandled BuiltinType: " + type.getAsString(), true);
          return TypeWithModifier::Bottom();
          break;
      }
    } else if (desugared->isEnumeralType()) {
      const auto *casted_enum_type = llvm::dyn_cast_or_null<clang::EnumType>(desugared);
      const std::string &name = casted_enum_type->getDecl()->getQualifiedNameAsString();
      if (name.find("(anonymous)") != std::string::npos) {
        return TypeWithModifier::Bottom();
      }
      const std::shared_ptr<EnumType> &type_ptr = EnumType::GetTypeByQualName(name);
      if (type_ptr == nullptr) {
        return TypeWithModifier::Bottom();
      }
      return TypeWithModifier{type_ptr, modifiers};

    } else if (desugared->isTemplateTypeParmType()) {
      const auto *template_type_parm_type = llvm::dyn_cast_or_null<clang::TemplateTypeParmType>(desugared);
      bool is_sugared = template_type_parm_type->isSugared();
      assert(!is_sugared);
      clang::IdentifierInfo *identifier = template_type_parm_type->getIdentifier();
      if (identifier != nullptr) {
        const std::string &typename_ = identifier->getName().str();
        const std::shared_ptr<TemplateTypenameType> &typename_ptr = std::make_shared<TemplateTypenameType>(typename_);
        return TypeWithModifier{typename_ptr, modifiers};
      } else {
        return TypeWithModifier::Bottom();
      }

    } else if (desugared->isFunctionType()) {
      const auto *func_type = llvm::dyn_cast<clang::FunctionType>(desugared);
      Logger::Warn("Encounter function argument type");
      return TypeWithModifier::Bottom();
    } else {
//      deref_type->dump();
      const std::string &name = type.getAsString();
      Logger::Warn("[TypeWithModifier::FromSpec]", "Non-processable QualType: " + name);
      return TypeWithModifier::Bottom();
    }

    return TypeWithModifier{nullptr, modifiers};

  } else {
    Logger::Error("TypeWithModifier::FromSpec", "Must supply by_type or by_clang_type", true);
    return TypeWithModifier::Bottom();
  }
}

const std::shared_ptr<Type> &TypeWithModifier::GetType() const {
  return type_;
}
const std::multiset<Modifier> &TypeWithModifier::GetModifiers() const {
  return modifiers_;
}
bool TypeWithModifier::IsPrimitiveType() const {
  return type_->GetVariant() == TypeVariant::kPrimitive;
}
bool TypeWithModifier::IsClassType() const {
  return type_->GetVariant() == TypeVariant::kClass;
}
bool TypeWithModifier::IsTemplateTypenameType() const {
  return type_->GetVariant() == TypeVariant::kTemplateTypename;
}
bool TypeWithModifier::IsTemplateTypenameSpcType() const {
  return type_->GetVariant() == TypeVariant::kTemplateTypenameSpc;
}
bool TypeWithModifier::IsEnumType() const {
  return type_->GetVariant() == TypeVariant::kEnum;
}
std::string TypeWithModifier::ToString() const {
  return ToString(nullptr);
}
std::string TypeWithModifier::ToString(const std::shared_ptr<TemplateTypeContext> &template_type_context) const {
  assert(!IsTemplateTypenameType());
  bool is_const_on_ptr = IsConstOnPointer();
  bool is_const = IsConst();
  bool is_unsigned = IsUnsigned();
  bool is_pointer = IsPointer();
  bool is_array = IsArray();
  bool is_reference = IsReference();

  unsigned long ptr_count = modifiers_.count(Modifier::kPointer);
  unsigned long arr_count = modifiers_.count(Modifier::kArray);
  bool is_multiptr = ptr_count + arr_count > 1;

  bool is_struct = false;
  std::string template_instantiation;
  if (IsClassType()) {
    const std::shared_ptr<ClassType> &class_type = std::static_pointer_cast<ClassType>(type_);
    const std::shared_ptr<ClassTypeModel> &class_type_model = class_type->GetModel();
    bool is_templated_class = class_type_model->IsTemplatedClass();
    if (is_templated_class) {
      TemplateTypeInstMapping &tt_inst_mapping = template_type_context->GetMapping();
      const TemplateTypeInstList &tt_inst_list = tt_inst_mapping.LookupForClass(class_type_model);
      template_instantiation = tt_inst_list.ToString();
    }
    is_struct = class_type_model->GetVariant() == ClassTypeModelVariant::kStruct;

  } else if (IsTemplateTypenameSpcType()) {
    const std::shared_ptr<TemplateTypenameSpcType> &tt_spc_type =
      std::static_pointer_cast<TemplateTypenameSpcType>(type_);
    const TemplateTypeInstList &tt_inst_list = tt_spc_type->GetInstList();
    template_instantiation = tt_inst_list.ToString();
  }

  std::stringstream ss;
  const std::string &type_name = type_->GetName() + template_instantiation;
  if (is_const) ss << "const ";
  if (is_unsigned) ss << "unsigned ";

  if (is_struct) ss << "struct ";
  ss << type_name;

  if (is_multiptr) {
    for (int i = 0; i < (int) ptr_count + arr_count; i++)
      ss << "*";
  } else if (is_pointer || is_array) ss << "*"; // not multiptr
  else if (is_reference) ss << "&";
  if (is_pointer && is_const_on_ptr) ss << " const";
//  else if (is_array) ss << "*";
  return ss.str();
}

void ToLower(std::string &str) {
  std::transform(str.begin(), str.end(), str.begin(), [](auto c) { return std::tolower(c); });
}

void Sanitize(std::string &str) {
  auto erase_it = std::remove_if(
    str.begin(), str.end(), [](auto &i) {
      bool is_alphabet = (i >= 'a' && i <= 'z') || (i >= 'A' && i <= 'Z');
      return !is_alphabet;
    });
  str.erase(erase_it, str.end());
}

std::string TypeWithModifier::GetDefaultVarName() const {
  assert(type_ != nullptr);
  const std::string &type_name = type_->GetName();
  std::string result;

  unsigned long last_colon_pos = type_name.find_last_of(':');
  if (last_colon_pos == std::string::npos) {
    result = type_name;
  } else {
    result = type_name.substr(last_colon_pos + 1);
  }

  ToLower(result);
  Sanitize(result);
  return result;
}

bool IsSubclassOf(
  const std::shared_ptr<ClassType> &parent_cls,
  const std::shared_ptr<ClassType> &candidate_subclass_cls,
  const std::shared_ptr<InheritanceTreeModel> &itm
) {
  const std::shared_ptr<ClassTypeModel> &parent_class_model = parent_cls->GetModel();
  if (parent_cls == candidate_subclass_cls) {
    return true;
  } else if (itm != nullptr) {
    const std::set<std::shared_ptr<ClassTypeModel>> &subclasses = itm->LookupSubClasses(parent_class_model);
    const std::shared_ptr<ClassTypeModel> &candidate_ctm = candidate_subclass_cls->GetModel();
    if (subclasses.count(candidate_ctm) > 0)
      return true;
  }
  return false;
}

bool TypeWithModifier::IsAssignableFrom(
  const TypeWithModifier &other,
  const std::shared_ptr<TemplateTypeContext> &tt_ctx,
  const std::shared_ptr<InheritanceTreeModel> &itm
) const {
  const TypeWithModifier &sink_twm = ResolveTemplateType(tt_ctx);
  const std::shared_ptr<Type> &sink_type = sink_twm.type_;

  const TypeWithModifier &src_twm = other.ResolveTemplateType(tt_ctx);
  const std::shared_ptr<Type> &src_type = src_twm.type_;

  bool is_both_primitives = sink_twm.IsPrimitiveType() && src_twm.IsPrimitiveType();
  bool is_copy_value = !sink_twm.IsReference() && !sink_twm.IsPointerOrArray();
  if (!is_both_primitives && (src_type == sink_type)) {
    bool sink_const = sink_twm.IsConst();
    bool src_const = src_twm.IsConst();
    if (sink_const && src_const)
      return true;
    if (!sink_const && src_const && !is_copy_value)
      return false;
    return true;
  }

  bool sink_is_class = IsClassType();
  bool src_is_class = other.IsClassType();
  if (sink_is_class && src_is_class) {
    const std::shared_ptr<ClassType> &sink_cls = std::static_pointer_cast<ClassType>(type_);
    const std::shared_ptr<ClassType> &src_cls = std::static_pointer_cast<ClassType>(other.type_);
    if (IsSubclassOf(sink_cls, src_cls, itm)) {
      return true;
    }
  }

  // Oh no! What was this code for?
  bool sink_is_tt_spc = IsTemplateTypenameSpcType();
  bool op_is_tt_spc = other.IsTemplateTypenameSpcType();
  if (sink_is_class && op_is_tt_spc) {
    const std::shared_ptr<ClassType> &sink_cls = std::static_pointer_cast<ClassType>(type_);
    const std::shared_ptr<TemplateTypenameSpcType> &tt_spc_type =
      std::static_pointer_cast<TemplateTypenameSpcType>(other.type_);
    const std::shared_ptr<Type> &target_type = tt_spc_type->GetTargetType();
    if (target_type->GetVariant() == TypeVariant::kClass) {
      const std::shared_ptr<ClassType> &src_cls = std::static_pointer_cast<ClassType>(target_type);
      if (IsSubclassOf(sink_cls, src_cls, itm)) {
        return true;
      }
    }
  } else if (sink_is_tt_spc && op_is_tt_spc) {
    const std::shared_ptr<TemplateTypenameSpcType> &sink_tt = std::static_pointer_cast<TemplateTypenameSpcType>(type_);
    const std::shared_ptr<TemplateTypenameSpcType> &src_tt =
      std::static_pointer_cast<TemplateTypenameSpcType>(other.type_);
    if (sink_tt->GetName() == src_tt->GetName()) // TODO: This is technical debt :(
      return true;
  }

  if (is_both_primitives) {
    if (sink_type == src_type) {
      bool both_unsigned = sink_twm.IsUnsigned() == src_twm.IsUnsigned();
      if (both_unsigned) {
        // exception: sink const, src non-const are allowed
        if (sink_twm.IsConst() && !src_twm.IsConst())
          return true;
        return (sink_twm.IsConst() == src_twm.IsConst());
      } else {
        return false;
      }
    } else {
      bool one_is_void_ptr = sink_twm.IsVoidPtr() || src_twm.IsVoidPtr();
      if (one_is_void_ptr)
        return true;
    }
  }
  return false;
}
TypeWithModifier::TypeWithModifier(const TypeWithModifier &other) {
  type_ = other.type_;
  modifiers_ = other.modifiers_;
  bottom_type_ = other.bottom_type_;
}
bool TypeWithModifier::IsConst() const {
  return modifiers_.count(Modifier::kConst) > 0;
}
bool TypeWithModifier::IsUnsigned() const {
  return modifiers_.count(Modifier::kUnsigned) > 0;
}
bool TypeWithModifier::IsPointer() const {
  return modifiers_.count(Modifier::kPointer) > 0;
}
bool TypeWithModifier::IsArray() const {
  return modifiers_.count(Modifier::kArray) > 0;
}
bool TypeWithModifier::IsReference() const {
  return modifiers_.count(Modifier::kReference) > 0;
}
bool TypeWithModifier::IsRValueReference() const {
  return modifiers_.count(Modifier::kRValueReference) > 0;
}
bool TypeWithModifier::IsPointerOrArray() const {
  return IsPointer() || IsArray();
}
bool TypeWithModifier::IsVoidType() const {
  return IsPrimitiveType() && type_ == PrimitiveType::kVoid;
}
TypeWithModifier TypeWithModifier::ResolveTemplateType(const std::shared_ptr<TemplateTypeContext> &tt_ctx) const {
  if (IsTemplateTypenameType()) {
    const std::multiset<Modifier> &original_modifiers = GetModifiers();
    const std::string &template_typename = type_->GetName();
    const TypeWithModifier &resolved_twm = tt_ctx->LookupOrResolve(template_typename);
    return resolved_twm.WithAdditionalModifiers(original_modifiers);

  } else {
    return *this;
  }
}
bool TypeWithModifier::operator==(const TypeWithModifier &rhs) const {
  return type_ == rhs.type_ &&
    modifiers_ == rhs.modifiers_;
}
bool TypeWithModifier::operator!=(const TypeWithModifier &rhs) const {
  return !(rhs == *this);
}
TypeWithModifier TypeWithModifier::StripAllModifiers() const {
  return TypeWithModifier(type_, std::multiset<Modifier>());
}
bool AllowMultimods(const Modifier &item) {
  switch (item) {
    case Modifier::kConst:
    case Modifier::kConstOnPointer:
    case Modifier::kUnsigned:
    case Modifier::kRValueReference:
    case Modifier::kReference: {
      return false;
    }
    case Modifier::kPointer:
    case Modifier::kArray: {
      return true;
    }
  }
}

TypeWithModifier TypeWithModifier::WithAdditionalModifiers(const std::multiset<Modifier> &mods) const {
  std::multiset<Modifier> new_mods = modifiers_;
  for (const auto &item : mods) {
    if (AllowMultimods(item)) {
      new_mods.insert(item);
    } else if (new_mods.count(item) == 0) {
      new_mods.insert(item);
    }
  }
  return TypeWithModifier(type_, new_mods);
}
bool TypeWithModifier::IsBottomType() const {
  return bottom_type_;
}
TypeWithModifier TypeWithModifier::Bottom() {
  TypeWithModifier tmp_twm{nullptr, std::multiset<Modifier>()};
  tmp_twm.bottom_type_ = true;
  return tmp_twm;
}
bool TypeWithModifier::IsConstOnPointer() const {
  return modifiers_.count(Modifier::kConstOnPointer) == 1;
}
TypeWithModifier TypeWithModifier::StripParticularModifiers(const std::multiset<Modifier> &mods) const {
  std::multiset<Modifier> new_mods;
  for (const auto &item : modifiers_) {
    if (mods.count(item) == 0)
      new_mods.insert(item);
  }
  return TypeWithModifier{type_, new_mods};
}
bool TypeWithModifier::IsVoidPtr() const {
  return type_ == PrimitiveType::kVoid && IsPointer();
}

// ##########
// # TemplateTypeInstMapping
// #####

TemplateTypeInstMapping::TemplateTypeInstMapping() = default;
TemplateTypeInstMapping::TemplateTypeInstMapping(std::map<std::string, TypeWithModifier> inst_mapping)
  : inst_mapping_(std::move(inst_mapping)) {}
TemplateTypeInstMapping::TemplateTypeInstMapping(const TemplateTypeInstMapping &other) = default;
const std::map<std::string, TypeWithModifier> &TemplateTypeInstMapping::GetInstMapping() const {
  return inst_mapping_;
}
TemplateTypeInstMapping &TemplateTypeInstMapping::Bind(
  const TemplateTypeParam &ttp,
  const TypeWithModifier &type
) {
  const std::string &name = ttp.GetName();
  inst_mapping_.emplace(name, type);
  return *this;
}
const TypeWithModifier &TemplateTypeInstMapping::LookupOrResolve(
  const std::string &template_typename,
  const std::function<TypeWithModifier()> &resolver
) {
  const auto &find_it = inst_mapping_.find(template_typename);
  if (find_it == inst_mapping_.end()) {
    const TypeWithModifier &resolved_type = resolver();
//    Logger::Debug(
//      "TemplateTypeInstMapping::LookupOrResolve",
//      "Resolving type " + template_typename + " with " + resolved_type.GetType()->GetQualifiedName());
    const auto &new_binding = inst_mapping_.emplace(template_typename, resolved_type);
    return new_binding.first->second;
  }
  return find_it->second;
}

TypeWithModifier DefaultTTResolverForType() {
  const std::shared_ptr<Random> &r = Random::GetInstance();
  bool should_int = r->NextBoolean();
  const std::shared_ptr<PrimitiveType> &target_type = should_int ? PrimitiveType::kInteger : PrimitiveType::kDouble;
  const TWMSpec &twm_spec = TWMSpec::ByType(target_type, nullptr);
  return TypeWithModifier::FromSpec(twm_spec);
};

TemplateTypeInstList TemplateTypeInstMapping::LookupFromTemplateTypeParamList(
  const TemplateTypeParamList &template_type_param_list
) {
  std::vector<TemplateTypeInstantiation> result;
  const std::vector<TemplateTypeParam> &params = template_type_param_list.GetList();
  for (const auto &item : params) {
    const std::string &template_typename = item.GetName();
    const TemplateTypeParamVariant variant = item.GetVariant();
    if (variant == TemplateTypeParamVariant::kTypeParam) {
      const TypeWithModifier &resolved_type = LookupOrResolve(template_typename, DefaultTTResolverForType);
      const TemplateTypeInstantiation &tt_inst = TemplateTypeInstantiation::ForType(resolved_type);
      result.push_back(tt_inst);
    } else {
      const std::shared_ptr<Random> &r = Random::GetInstance();
      int integral = r->NextInt(1, 8);
      const TemplateTypeInstantiation &tt_inst = TemplateTypeInstantiation::ForIntegral(integral);
      result.push_back(tt_inst);
    }
  }
  return TemplateTypeInstList{result};
}

TemplateTypeInstList TemplateTypeInstMapping::LookupForClass(
  const std::shared_ptr<ClassTypeModel> &class_type_model
) {
  const TemplateTypeParamList &tt_params = class_type_model->GetTemplateParamList();
  return LookupFromTemplateTypeParamList(tt_params);
}
TemplateTypeInstList TemplateTypeInstMapping::LookupForExecutable(
  const std::shared_ptr<Executable> &executable
) {
  const TemplateTypeParamList &tt_params = executable->GetTemplateParamList();
  return LookupFromTemplateTypeParamList(tt_params);
}
void TemplateTypeInstMapping::ApplyBindings(const std::map<std::string, TypeWithModifier> &bindings) {
  for (const auto &item : bindings) {
    const std::string &tt_name = item.first;
    const TypeWithModifier &target_twm = item.second;

    const auto &find_it = inst_mapping_.find(tt_name);
    if (find_it == inst_mapping_.end()) {
      inst_mapping_.emplace(tt_name, target_twm);

    } else {
      TypeWithModifier &prev_twm = find_it->second;
      if (prev_twm != target_twm) {
        Logger::Warn(
          "TemplateTypeInstMapping::ApplyBindings",
          "Overriding template typename " + tt_name + " with " + target_twm.ToString(nullptr)
            + " (prev_type was " + prev_twm.ToString(nullptr) + ")"
        );
        prev_twm = target_twm;
      }
    }
  }

}

// ##########
// # TemplateTypeContext
// #####

TemplateTypeContext::TemplateTypeContext() = default;
TemplateTypeContext::TemplateTypeContext(const TemplateTypeContext &other) = default;
TemplateTypeInstMapping &TemplateTypeContext::GetMapping() {
  return mapping_;
}
const TypeWithModifier &TemplateTypeContext::LookupOrResolve(const std::string &template_typename) {
  // Hard-coded resolver for now :)
  return mapping_.LookupOrResolve(template_typename, DefaultTTResolverForType);
}
void TemplateTypeContext::ApplyBindings(const std::map<std::string, TypeWithModifier> &bindings) {
  mapping_.ApplyBindings(bindings);
}
std::shared_ptr<TemplateTypeContext> TemplateTypeContext::Clone(const std::shared_ptr<TemplateTypeContext> &ref) {
  if (ref == nullptr)
    return New();
  return std::make_shared<TemplateTypeContext>(*ref);
}
TemplateTypeContext &TemplateTypeContext::Bind(const TemplateTypeParam &ttp, const TypeWithModifier &type) {
  mapping_.Bind(ttp, type);
  return *this;
}
TemplateTypeContext::TemplateTypeContext(const TemplateTypeInstMapping &mapping) : mapping_(mapping) {}
std::shared_ptr<TemplateTypeContext> TemplateTypeContext::New() {
  return std::make_shared<TemplateTypeContext>();
}

// ##########
// # TemplateTypenameType
// #####

TemplateTypenameType::TemplateTypenameType(const std::string &name) : Type(name, TypeVariant::kTemplateTypename) {}

// ##########
// # TemplateTypeInstList
// #####

TemplateTypeInstList::TemplateTypeInstList(std::vector<TemplateTypeInstantiation> insts)
  : insts_(std::move(insts)) {}
std::string TemplateTypeInstList::ToString() const {
  std::stringstream tt;
  if (!insts_.empty()) {
    bool first = true;
    tt << '<';
    for (const auto &inst : insts_) {
      tt << (first ? "" : ", ") << inst.ToString();
      first = false;
    }
    tt << '>';
  }
  return tt.str();
}
bool TemplateTypeInstList::Equals(const TemplateTypeInstList &other) const {
  if (insts_.size() != other.insts_.size())
    return false;
  int length = (int) insts_.size();
  for (int i = 0; i < length; i++) {
    if (insts_[i] != other.insts_[i])
      return false;
  }
  return true;
}
const std::vector<TemplateTypeInstantiation> &TemplateTypeInstList::GetInstantiations() const {
  return insts_;
}

// ##########
// # TemplateTypenameSpcType
// #####

std::map<std::shared_ptr<Type>, std::vector<std::shared_ptr<TemplateTypenameSpcType>>>
  TemplateTypenameSpcType::kGlobalExistingSpcTypes;
TemplateTypenameSpcType::TemplateTypenameSpcType(
  std::shared_ptr<Type> target_type,
  TemplateTypeInstList inst_list
) : Type(target_type->GetName(), TypeVariant::kTemplateTypenameSpc),
    target_type_(std::move(target_type)),
    inst_list_(std::move(inst_list)) {}
const std::shared_ptr<Type> &TemplateTypenameSpcType::GetTargetType() const {
  return target_type_;
}
const TemplateTypeInstList &TemplateTypenameSpcType::GetInstList() const {
  return inst_list_;
}
const std::shared_ptr<TemplateTypenameSpcType> &TemplateTypenameSpcType::From(
  const std::shared_ptr<Type> &target_type,
  const TemplateTypeInstList &inst_list
) {
  assert(target_type != nullptr);
  for (const auto &item : inst_list.GetInstantiations()) {
    if (item.IsType()) {
      const TypeWithModifier &inst_twm = item.GetType();
      assert(!inst_twm.IsBottomType());
      assert(inst_twm.GetType() != nullptr);
    }
  }
  std::vector<std::shared_ptr<TemplateTypenameSpcType>> &existing_types = LookupExistingByTargetType(target_type);
  const auto &find_it = std::find_if(
    existing_types.begin(), existing_types.end(), [inst_list](const std::shared_ptr<TemplateTypenameSpcType> &it) {
      return inst_list.Equals(it->GetInstList());
    });
  if (find_it == existing_types.end()) {
    const std::shared_ptr<TemplateTypenameSpcType> &new_spc_type =
      std::make_shared<TemplateTypenameSpcType>(target_type, inst_list);
    existing_types.push_back(new_spc_type);
    return existing_types.back();

  } else {
    return *find_it;
  }
}
std::vector<std::shared_ptr<TemplateTypenameSpcType>> &TemplateTypenameSpcType::LookupExistingByTargetType(
  const std::shared_ptr<Type> &target
) {
  auto find_it = kGlobalExistingSpcTypes.find(target);
  if (find_it == kGlobalExistingSpcTypes.end()) {
    auto new_it = kGlobalExistingSpcTypes.emplace(target, std::vector<std::shared_ptr<TemplateTypenameSpcType>>());
    return new_it.first->second;
  }
  return find_it->second;
}

// ##########
// # STLType
// #####

std::shared_ptr<STLType> InitSTLType(
  const std::string &name,
  STLTypeVariant template_spc_type,
  const std::vector<std::string> &name_aliases
) {
  return std::make_shared<STLType>(name, template_spc_type, name_aliases);
}

STLType::STLType(
  const std::string &name,
  STLTypeVariant template_spc_type,
  std::vector<std::string> name_aliases
)
  : Type(name, TypeVariant::kSTL),
    stl_type_variant_(template_spc_type), name_aliases_(std::move(name_aliases)) {}

const std::shared_ptr<STLType> STLType::kVector =
  InitSTLType("std::vector", STLTypeVariant::kRegContainer, {});
const std::shared_ptr<STLType> STLType::kDeque =
  InitSTLType("std::deque", STLTypeVariant::kRegContainer, {});
const std::shared_ptr<STLType> STLType::kForwardList =
  InitSTLType("std::forward_list", STLTypeVariant::kRegContainer, {});
const std::shared_ptr<STLType> STLType::kList =
  InitSTLType(
    "std::list", STLTypeVariant::kRegContainer, {
      "std::__cxx11::list"
    });
const std::shared_ptr<STLType> STLType::kStack =
  InitSTLType("std::stack", STLTypeVariant::kRegContainer, {});
const std::shared_ptr<STLType> STLType::kQueue =
  InitSTLType("std::queue", STLTypeVariant::kRegContainer, {});
const std::shared_ptr<STLType> STLType::kPriorityQueue =
  InitSTLType("std::priority_queue", STLTypeVariant::kRegContainer, {});
const std::shared_ptr<STLType> STLType::kSet =
  InitSTLType("std::set", STLTypeVariant::kRegContainer, {});
const std::shared_ptr<STLType> STLType::kMultiset =
  InitSTLType("std::multiset", STLTypeVariant::kRegContainer, {});
const std::shared_ptr<STLType> STLType::kUnorderedSet =
  InitSTLType("std::unordered_set", STLTypeVariant::kRegContainer, {});
const std::shared_ptr<STLType> STLType::kUnorderedMultiset =
  InitSTLType("std::unordered_multiset", STLTypeVariant::kRegContainer, {});

const std::shared_ptr<STLType> STLType::kMap =
  InitSTLType("std::map", STLTypeVariant::kKeyValueContainer, {});
const std::shared_ptr<STLType> STLType::kMultimap =
  InitSTLType("std::multimap", STLTypeVariant::kKeyValueContainer, {});
const std::shared_ptr<STLType> STLType::kUnorderedMap =
  InitSTLType("std::unordered_map", STLTypeVariant::kKeyValueContainer, {});
const std::shared_ptr<STLType> STLType::kUnorderedMultimap =
  InitSTLType("std::unordered_multimap", STLTypeVariant::kKeyValueContainer, {});

const std::shared_ptr<STLType> STLType::kArray =
  InitSTLType("std::array", STLTypeVariant::kRegContainerWithSize, {});
const std::shared_ptr<STLType> STLType::kPair =
  InitSTLType("std::pair", STLTypeVariant::kPair, {});
const std::shared_ptr<STLType> STLType::kTuple =
  InitSTLType("std::tuple", STLTypeVariant::kTuple, {});

const std::shared_ptr<STLType> STLType::kSharedPtr =
  InitSTLType("std::shared_ptr", STLTypeVariant::kSmartPointer, {});
const std::shared_ptr<STLType> STLType::kUniquePtr =
  InitSTLType("std::unique_ptr", STLTypeVariant::kSmartPointer, {});
const std::shared_ptr<STLType> STLType::kBasicString =
  InitSTLType("std::basic_string", STLTypeVariant::kString, {"std::__cxx11::basic_string"});

std::vector<std::shared_ptr<STLType>> STLType::kInstalledSTLTypes = {
  kVector,
  kDeque,
  kForwardList,
  kList,
  kStack,
  kQueue,
  kPriorityQueue,
  kSet,
  kMultiset,
  kUnorderedSet,
  kUnorderedMultiset,
  kMap,
  kMultimap,
  kUnorderedMap,
  kUnorderedMultimap,
  kArray,
  kPair,
  kTuple,
//  kSharedPtr, PROBLEMATIC FOR NOW
//  kUniquePtr,
  kBasicString,
};

STLTypeVariant STLType::GetSTLTypeVariant() const {
  return stl_type_variant_;
}
std::shared_ptr<STLType> STLType::IsInstalledSTLType(const std::string &name) {
  auto find_it = std::find_if(
    kInstalledSTLTypes.begin(), kInstalledSTLTypes.end(), [name](const std::shared_ptr<STLType> &it) {
      if (it->GetName() == name)
        return true;
      const std::vector<std::string> &aliases = it->name_aliases_;
      bool has_matching_alias = std::find(aliases.begin(), aliases.end(), name) != aliases.end();
      return has_matching_alias;
    });
  if (find_it == kInstalledSTLTypes.end())
    return nullptr;
  return *find_it;
}
int STLType::GetTemplateArgumentLength() const {
  switch (stl_type_variant_) {
    case STLTypeVariant::kRegContainer:
    case STLTypeVariant::kSmartPointer:
    case STLTypeVariant::kString:
      return 1;
    case STLTypeVariant::kRegContainerWithSize:
    case STLTypeVariant::kKeyValueContainer:
    case STLTypeVariant::kPair:
      return 2;
    case STLTypeVariant::kTuple:
      assert(false);
      return 0;
  }
}
bool STLType::IsSTLType(const std::string &name) {
  return name.rfind("std::", 0) == 0;
}
bool STLType::IsUnhandledSTLType(const std::string &name) {
  const std::shared_ptr<STLType> &ptr = IsInstalledSTLType(name);
  bool is_unhandled = ptr == nullptr;
  bool is_stl_type = IsSTLType(name);
  return is_unhandled && is_stl_type;
}

// ##########
// # TemplateTypeInstantiation
// #####

TemplateTypeInstantiation::TemplateTypeInstantiation(
  bpstd::optional<TypeWithModifier> type, const bpstd::optional<int> &integral, TemplateTypeInstVariant variant
)
  : type_(std::move(type)), integral_(integral), variant_(variant) {}
const TypeWithModifier &TemplateTypeInstantiation::GetType() const {
  assert(type_.has_value());
  return type_.value();
}
int TemplateTypeInstantiation::GetIntegral() const {
  assert(integral_.has_value());
  return integral_.value();
}
TemplateTypeInstVariant TemplateTypeInstantiation::GetVariant() const {
  return variant_;
}
TemplateTypeInstantiation TemplateTypeInstantiation::ForType(const TypeWithModifier &type) {
  return TemplateTypeInstantiation{bpstd::make_optional(type), bpstd::nullopt, TemplateTypeInstVariant::kType};
}
TemplateTypeInstantiation TemplateTypeInstantiation::ForIntegral(const int &integral) {
  return TemplateTypeInstantiation{bpstd::nullopt, bpstd::make_optional(integral), TemplateTypeInstVariant::kIntegral};
}
bool TemplateTypeInstantiation::operator==(const TemplateTypeInstantiation &rhs) const {
  return type_ == rhs.type_ &&
    integral_ == rhs.integral_ &&
    variant_ == rhs.variant_;
}
bool TemplateTypeInstantiation::operator!=(const TemplateTypeInstantiation &rhs) const {
  return !(rhs == *this);
}
std::string TemplateTypeInstantiation::ToString() const {
  TemplateTypeInstVariant variant = GetVariant();
  switch (variant) {
    case TemplateTypeInstVariant::kType:
      return type_->ToString();
    case TemplateTypeInstVariant::kIntegral:
      return std::to_string(*integral_);
    case TemplateTypeInstVariant::kNullptr:
      return "nullptr";
  }
}
bool TemplateTypeInstantiation::IsType() const {
  return GetVariant() == TemplateTypeInstVariant::kType;
}
TemplateTypeInstantiation TemplateTypeInstantiation::ForNullptr() {
  return TemplateTypeInstantiation{{}, {}, TemplateTypeInstVariant::kNullptr};
}
} // namespace cxxfoozz