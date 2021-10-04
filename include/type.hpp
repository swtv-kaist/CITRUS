#ifndef CXXFOOZZ_SRC_TYPE_HPP_
#define CXXFOOZZ_SRC_TYPE_HPP_

#include "model.hpp"

namespace cxxfoozz {

enum class TypeVariant {
  kPrimitive = 0,
  kClass,
  kEnum,
  kTemplateTypename,
  kTemplateTypenameSpc,
  kSTL,
};
class Type {
 public:
  explicit Type(std::string name, TypeVariant variant);
  const std::string &GetName() const;
  TypeVariant GetVariant() const;
 private:
  std::string name_;
  TypeVariant variant_;
};
enum class PrimitiveTypeVariant {
  kVoid,
  kBoolean,
  kShort,
  kCharacter,
  kInteger,
  kLong,
  kLongLong,
  kFloat,
  kDouble,
  kWideCharacter,
  kNullptrType,
};
class PrimitiveType : public Type {
 public:
  PrimitiveType(
    const std::string &name,
    PrimitiveTypeVariant primitive_type_variant
  );
  static const std::shared_ptr<PrimitiveType> kVoid;
  static const std::shared_ptr<PrimitiveType> kBoolean;
  static const std::shared_ptr<PrimitiveType> kShort;
  static const std::shared_ptr<PrimitiveType> kCharacter;
  static const std::shared_ptr<PrimitiveType> kInteger;
  static const std::shared_ptr<PrimitiveType> kLong;
  static const std::shared_ptr<PrimitiveType> kLongLong;
  static const std::shared_ptr<PrimitiveType> kFloat;
  static const std::shared_ptr<PrimitiveType> kDouble;
  static const std::shared_ptr<PrimitiveType> kWideCharacter;
  static const std::shared_ptr<PrimitiveType> kNullptrType;
  static int SizeOf(const std::shared_ptr<PrimitiveType> &t);
  PrimitiveTypeVariant GetPrimitiveTypeVariant() const;
 private:
  PrimitiveTypeVariant primitive_type_variant_;
};

class StructType : public Type {
  // TODO (TRL-9)!
  StructType() : Type(std::string(), cxxfoozz::TypeVariant::kClass) {}
};

enum class STLTypeVariant {
  kRegContainer = 0,
  kRegContainerWithSize,
  kKeyValueContainer,
  kPair,
  kTuple,
  kSmartPointer,
  kString,
};

class STLType : public Type {
 public:
  STLType(
    const std::string &name,
    STLTypeVariant template_spc_type,
    std::vector<std::string> name_aliases
  );
  STLTypeVariant GetSTLTypeVariant() const;
  static bool IsSTLType(const std::string &name);
  static bool IsUnhandledSTLType(const std::string &name);
  static std::shared_ptr<STLType> IsInstalledSTLType(const std::string &name);
  int GetTemplateArgumentLength() const;

  // Regular Containers
  static const std::shared_ptr<STLType> kVector;
  static const std::shared_ptr<STLType> kDeque;
  static const std::shared_ptr<STLType> kForwardList;
  static const std::shared_ptr<STLType> kList;
  static const std::shared_ptr<STLType> kStack;
  static const std::shared_ptr<STLType> kQueue;
  static const std::shared_ptr<STLType> kPriorityQueue;
  static const std::shared_ptr<STLType> kSet;
  static const std::shared_ptr<STLType> kMultiset;
  static const std::shared_ptr<STLType> kUnorderedSet;
  static const std::shared_ptr<STLType> kUnorderedMultiset;

  // Key Value Containers
  static const std::shared_ptr<STLType> kMap;
  static const std::shared_ptr<STLType> kMultimap;
  static const std::shared_ptr<STLType> kUnorderedMap;
  static const std::shared_ptr<STLType> kUnorderedMultimap;

  // Special Containers
  static const std::shared_ptr<STLType> kArray;
  static const std::shared_ptr<STLType> kPair;
  static const std::shared_ptr<STLType> kTuple;

  // Smart Pointers
  static const std::shared_ptr<STLType> kSharedPtr;
  static const std::shared_ptr<STLType> kUniquePtr;

  // String
  static const std::shared_ptr<STLType> kBasicString;

  static std::vector<std::shared_ptr<STLType>> kInstalledSTLTypes;

 private:
  STLTypeVariant stl_type_variant_;
  std::vector<std::string> name_aliases_;
};

class ClassType : public Type {
 public:
  explicit ClassType(std::shared_ptr<ClassTypeModel> model);
  static const std::map<std::string, std::shared_ptr<ClassType>> &GetKGlobalClassTypes();
  const std::shared_ptr<ClassTypeModel> &GetModel() const;
  static std::shared_ptr<ClassType> GetTypeByQualName(const std::string &qual_name);
  static std::shared_ptr<ClassType> GetTypeByQualNameLifted(const std::string &qual_name);
  static void Install(const std::vector<std::shared_ptr<ClassTypeModel>> &models);

 private:
  static std::map<std::string, std::shared_ptr<ClassType>> kGlobalClassTypes;
  std::shared_ptr<ClassTypeModel> model_;
};
class EnumType : public Type {
 public:
  explicit EnumType(std::shared_ptr<EnumTypeModel> model);
  static const std::map<std::string, std::shared_ptr<EnumType>> &GetKGlobalEnumTypes();
  const std::shared_ptr<EnumTypeModel> &GetModel() const;
  static std::shared_ptr<EnumType> GetTypeByQualName(const std::string &qual_name);
  static void Install(const std::vector<std::shared_ptr<EnumTypeModel>> &models);
 private:
  static std::map<std::string, std::shared_ptr<EnumType>> kGlobalEnumTypes;
  std::shared_ptr<EnumTypeModel> model_;
};

class TemplateTypenameType : public Type {
 public:
  explicit TemplateTypenameType(const std::string &name);
};

enum class Modifier {
  kConst = 0,
  kConstOnPointer,
  kUnsigned,
  kPointer,
  kArray,
  kReference, // lvalue reference
  kRValueReference,
};

class TypeWithModifier;
class TemplateTypeInstantiation;

class TemplateTypeInstList {
 public:
  explicit TemplateTypeInstList(std::vector<TemplateTypeInstantiation> inst_types);
  const std::vector<TemplateTypeInstantiation> &GetInstantiations() const;
  std::string ToString() const;
  bool Equals(const TemplateTypeInstList &other) const;

 private:
  std::vector<TemplateTypeInstantiation> insts_;
};

class TemplateTypenameSpcType : public Type {
 public:
  TemplateTypenameSpcType(
    std::shared_ptr<Type> target_type,
    TemplateTypeInstList inst_list
  );
  static const std::shared_ptr<TemplateTypenameSpcType> &From(
    const std::shared_ptr<Type> &target_type,
    const TemplateTypeInstList &inst_list
  );
  const std::shared_ptr<Type> &GetTargetType() const;
  const TemplateTypeInstList &GetInstList() const;
 private:
  static std::vector<std::shared_ptr<TemplateTypenameSpcType>> &LookupExistingByTargetType(const std::shared_ptr<Type> &target);
  static std::map<std::shared_ptr<Type>, std::vector<std::shared_ptr<TemplateTypenameSpcType>>> kGlobalExistingSpcTypes;
  std::shared_ptr<Type> target_type_;
  TemplateTypeInstList inst_list_;
};

class TemplateTypeInstMapping {
 public:
  TemplateTypeInstMapping();
  explicit TemplateTypeInstMapping(std::map<std::string, TypeWithModifier> inst_mapping);
  TemplateTypeInstMapping(const TemplateTypeInstMapping &other);
  const std::map<std::string, TypeWithModifier> &GetInstMapping() const;
  const TypeWithModifier &LookupOrResolve(
    const std::string &template_typename,
    const std::function<TypeWithModifier()> &resolver
  );
  TemplateTypeInstList LookupForClass(const std::shared_ptr<ClassTypeModel> &class_type_model);
  TemplateTypeInstList LookupForExecutable(const std::shared_ptr<Executable> &executable);
  TemplateTypeInstList LookupFromTemplateTypeParamList(const TemplateTypeParamList &template_type_param_list);
  TemplateTypeInstMapping &Bind(const TemplateTypeParam &ttp, const TypeWithModifier &type);
  void ApplyBindings(const std::map<std::string, TypeWithModifier> &bindings);
 private:
  std::map<std::string, TypeWithModifier> inst_mapping_;
};

class TemplateTypeContext {
 public:
  TemplateTypeContext();
  explicit TemplateTypeContext(const TemplateTypeInstMapping &mapping);
  TemplateTypeContext(const TemplateTypeContext &other);
  TemplateTypeInstMapping &GetMapping();
  static std::shared_ptr<TemplateTypeContext> New();
  static std::shared_ptr<TemplateTypeContext> Clone(const std::shared_ptr<TemplateTypeContext> &ref);
  const TypeWithModifier &LookupOrResolve(const std::string &template_typename);
  TemplateTypeContext &Bind(const TemplateTypeParam &ttp, const TypeWithModifier &type);
  void ApplyBindings(const std::map<std::string, TypeWithModifier> &bindings);
 private:
  TemplateTypeInstMapping mapping_;
};

class TWMSpec {
 public:
  TWMSpec();
  static TWMSpec ByType(const std::shared_ptr<Type> &by_type, const std::shared_ptr<TemplateTypeContext> &tt_ctx);
  static TWMSpec ByClangType(const clang::QualType &by_clang_type, const std::shared_ptr<TemplateTypeContext> &tt_ctx);
  const std::shared_ptr<Type> &GetByType() const;
  void SetByType(const std::shared_ptr<Type> &by_type);
  const bpstd::optional<clang::QualType> &GetByClangType() const;
  void SetByClangType(const bpstd::optional<clang::QualType> &by_clang_type);
  const std::multiset<Modifier> &GetAdditionalMods() const;
  void SetAdditionalMods(const std::multiset<Modifier> &additional_mods);
  const std::shared_ptr<TemplateTypeContext> &GetTemplateTypeContext() const;
  void SetTemplateTypeContext(const std::shared_ptr<TemplateTypeContext> &template_type_context);

 private:
  std::shared_ptr<Type> by_type_;
  bpstd::optional<clang::QualType> by_clang_type_;
  std::multiset<Modifier> additional_mods_;
  std::shared_ptr<TemplateTypeContext> template_type_context_;
};

class TypeWithModifier {
 public:
  TypeWithModifier(std::shared_ptr<Type> type, std::multiset<Modifier> modifiers);
  TypeWithModifier(const TypeWithModifier &other);
  static TypeWithModifier FromSpec(const TWMSpec &spec);
  static TypeWithModifier Bottom();
  TypeWithModifier ResolveTemplateType(const std::shared_ptr<TemplateTypeContext> &tt_ctx) const;
  TypeWithModifier StripAllModifiers() const;
  TypeWithModifier WithAdditionalModifiers(const std::multiset<Modifier> &mods) const;
  TypeWithModifier StripParticularModifiers(const std::multiset<Modifier> &mods) const;

  bool operator==(const TypeWithModifier &rhs) const;
  bool operator!=(const TypeWithModifier &rhs) const;

  const std::shared_ptr<Type> &GetType() const;
  const std::multiset<Modifier> &GetModifiers() const;
  bool IsVoidType() const;
  bool IsPrimitiveType() const;
  bool IsClassType() const;
  bool IsEnumType() const;
  bool IsTemplateTypenameType() const;
  bool IsTemplateTypenameSpcType() const;
  std::string ToString() const;
  std::string ToString(const std::shared_ptr<TemplateTypeContext> &template_type_context) const;

  std::string GetDefaultVarName() const;
  bool IsAssignableFrom(
    const TypeWithModifier &other,
    const std::shared_ptr<TemplateTypeContext> &tt_ctx,
    const std::shared_ptr<InheritanceTreeModel> &itm
  ) const;
  bool IsConst() const;
  bool IsUnsigned() const;
  bool IsPointer() const;
  bool IsConstOnPointer() const;
  bool IsVoidPtr() const;

  bool IsArray() const;
  bool IsReference() const;
  bool IsRValueReference() const;
  bool IsPointerOrArray() const;
  bool IsBottomType() const;
 private:
  std::shared_ptr<Type> type_;
  std::multiset<Modifier> modifiers_;
  bool bottom_type_;
};

enum class TemplateTypeInstVariant {
  kType = 0,
  kIntegral,
  kNullptr,
};

class TemplateTypeInstantiation {
 public:
  TemplateTypeInstantiation(
    bpstd::optional<TypeWithModifier> type,
    const bpstd::optional<int> &integral,
    TemplateTypeInstVariant variant
  );
  static TemplateTypeInstantiation ForType(const TypeWithModifier &type);
  static TemplateTypeInstantiation ForIntegral(const int &integral);
  static TemplateTypeInstantiation ForNullptr();
  const TypeWithModifier &GetType() const;
  int GetIntegral() const;
  TemplateTypeInstVariant GetVariant() const;
  bool IsType() const;
  std::string ToString() const;
  bool operator==(const TemplateTypeInstantiation &rhs) const;
  bool operator!=(const TemplateTypeInstantiation &rhs) const;
 private:
  bpstd::optional<TypeWithModifier> type_;
  bpstd::optional<int> integral_;
  TemplateTypeInstVariant variant_;
};
}

#endif //CXXFOOZZ_SRC_TYPE_HPP_
