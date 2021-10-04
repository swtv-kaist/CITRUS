#include <iostream>
#include <utility>

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"

#include "analyzer.hpp"
#include "logger.hpp"
#include "type.hpp"

namespace cxxfoozz {

std::vector<clang::QualType> ExtractArguments(clang::FunctionDecl *method) {
  const llvm::MutableArrayRef<clang::ParmVarDecl *> &parameters = method->parameters();
  std::vector<clang::QualType> arguments;
  std::transform(
    parameters.begin(), parameters.end(), std::back_inserter(arguments), [](auto i) {
      return i->getOriginalType();
    });
  return arguments;
}

TemplateTypeParamList ExtractTemplateType(clang::TemplateParameterList *template_parameter_list) {
  int idx = 0;
  std::vector<TemplateTypeParam> type_params;
  for (const auto &item : *template_parameter_list) {
    auto *nttpd = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(item);
    auto *ttpd = llvm::dyn_cast<clang::TemplateTypeParmDecl>(item);
    if (nttpd) {
      const std::string &name = nttpd->getNameAsString();
      const TemplateTypeParam &nttp = TemplateTypeParam(name, idx++, TemplateTypeParamVariant::kNonTypeParam);
      type_params.push_back(nttp);
    } else if (ttpd) {
      const std::string &name = ttpd->getNameAsString();
      const TemplateTypeParam &ttp = TemplateTypeParam(name, idx++, TemplateTypeParamVariant::kTypeParam);
      type_params.push_back(ttp);
    }
  }
  return TemplateTypeParamList(type_params);
}

TemplateTypeParamList ExtractTemplateType(clang::ClassTemplateDecl *class_template_decl) {
  clang::TemplateParameterList *template_list = class_template_decl->getTemplateParameters();
  assert(template_list->size() > 0);
  return ExtractTemplateType(template_list);
}

TemplateTypeParamList ExtractTemplateType(clang::FunctionTemplateDecl *func_template_decl) {
  clang::TemplateParameterList *template_list = func_template_decl->getTemplateParameters();
  assert(template_list->size() > 0);
  return ExtractTemplateType(template_list);
}

std::vector<clang::CXXRecordDecl *> GetInnerClasses(clang::CXXRecordDecl *class_decl) {
  std::vector<clang::CXXRecordDecl *> result;
  const clang::DeclContext::decl_range &decls = class_decl->decls();
  for (const auto &decl : decls) {
    if (auto cxx_decl = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(decl)) {
      result.push_back(cxx_decl);
      const std::vector<clang::CXXRecordDecl *> &rec = GetInnerClasses(cxx_decl);
      std::copy(rec.begin(), rec.end(), std::back_inserter(result));
    }
  }
  return result;
}

std::set<std::string> ignored_cls;
void AddToIgnoredClasses(std::vector<clang::CXXRecordDecl *> classes) {
  for (const auto &item : classes) {
    const std::string &qname = item->getQualifiedNameAsString();
    Logger::Info("Ignoring inner class (nested within templated class): " + qname);
    ignored_cls.insert(qname);
  }
}

std::shared_ptr<ClassTypeModel> MakeClassTypeModel(
  clang::CXXRecordDecl *class_decl,
  const std::map<clang::CXXRecordDecl *, clang::ClassTemplateDecl *> &template_decl_mapping,
  const std::vector<std::shared_ptr<ClassTypeModel>> &all_class_decls
) {
  std::string name, qual_name;
  bool is_anon = name.empty() || qual_name == "(anonymous)";
  clang::TypedefNameDecl *typedef_decl = class_decl->getTypedefNameForAnonDecl();
  if (is_anon && typedef_decl != nullptr) {
    name = typedef_decl->getNameAsString();
    qual_name = typedef_decl->getQualifiedNameAsString();
  } else {
    name = class_decl->getNameAsString();
    qual_name = class_decl->getQualifiedNameAsString();
  }
  const auto &find_it = std::find_if(
    all_class_decls.begin(), all_class_decls.end(), [qual_name](const auto &i) {
      return i->GetQualifiedName() == qual_name;
    });
  if (find_it != all_class_decls.end()) {
    return *find_it;
  }

  bool is_class = class_decl->isClass();
  bool is_struct = class_decl->isStruct();
  bool is_union = class_decl->isUnion();
  assert(is_class || is_struct);
  assert(!is_union);

  auto variant = is_class ? ClassTypeModelVariant::kClass : ClassTypeModelVariant::kStruct;

  auto find_template_decl = template_decl_mapping.find(class_decl);
  bool is_templated_class = find_template_decl != template_decl_mapping.end();
  if (is_templated_class) {
    // TRL91 - Inner classes of templated classes are ignored for now
    const std::vector<clang::CXXRecordDecl *> &inners = GetInnerClasses(class_decl);
    AddToIgnoredClasses(inners);

    clang::ClassTemplateDecl *template_decl = find_template_decl->second;
    const TemplateTypeParamList &template_param_list = ExtractTemplateType(template_decl);
    const std::shared_ptr<ClassTypeModel> &class_type_model =
      std::make_shared<ClassTypeModel>(name, qual_name, class_decl, bpstd::make_optional(template_decl), variant);
    class_type_model->SetTemplateParamList(template_param_list);
    return class_type_model;
  }
  return std::make_shared<ClassTypeModel>(name, qual_name, class_decl, variant);
}

std::shared_ptr<EnumTypeModel> MakeEnumTypeModel(clang::EnumDecl *enum_decl) {
  const std::string &name = enum_decl->getNameAsString();
  const std::string &enum_name = enum_decl->getQualifiedNameAsString();
  const clang::EnumDecl::enumerator_range &enumerators = enum_decl->enumerators();

  std::vector<std::string> variants;
  std::transform(
    enumerators.begin(), enumerators.end(), std::back_inserter(variants), [](auto i) {
      return i->getNameAsString();
    });
  return std::make_shared<EnumTypeModel>(name, enum_name, variants, enum_decl);
}

int FilterOutUnsatisfiableExecutables(
  std::vector<std::shared_ptr<Executable>> &all_executables,
  std::vector<std::shared_ptr<Creator>> &all_creators,
  const std::shared_ptr<InheritanceTreeModel> &inheritance_model
) {
  // TODO: HARDCODED FOR NOW.
  ignored_cls.insert("_Z3_config"); // CROWN

  std::map<std::shared_ptr<ClassTypeModel>, std::vector<std::shared_ptr<Creator>>> creators_by_ctms;
  std::set<std::shared_ptr<ClassTypeModel>> has_creators;
  for (const auto &creator : all_creators) {
    const std::shared_ptr<ClassTypeModel> &owner = creator->GetTargetClass();
    auto it = creators_by_ctms.find(owner);
    if (it == creators_by_ctms.end()) {
      creators_by_ctms.insert(std::make_pair(owner, std::vector<std::shared_ptr<Creator>>{creator}));
    } else {
      it->second.push_back(creator);
    }
    has_creators.insert(owner);
  }
  bool has_update;
  do {
    has_update = false;
    for (const auto &class_model : has_creators) {
      const std::set<std::shared_ptr<ClassTypeModel>> &base_class = inheritance_model->LookupBaseClasses(class_model);
      for (const auto &item : base_class) {
        const auto &insert_res = has_creators.insert(item);
        bool success = insert_res.second;
        has_update |= success;
      }
    }
  } while (has_update);

  auto executable_filter = [&has_creators](const std::shared_ptr<Executable> &e) {
    const std::vector<clang::QualType> &arguments = e->GetArguments();
    const bpstd::optional<clang::QualType> &opt_return_type = e->GetReturnType();

    const auto &is_unsatisfiable_cls = [&](const std::shared_ptr<ClassType> &class_type) {
      const std::shared_ptr<ClassTypeModel> &class_model = class_type->GetModel();
      bool no_creator = has_creators.count(class_model) == 0;
      bool is_ignored = ignored_cls.count(class_model->GetQualifiedName()) > 0; // TRL-91
      if (no_creator || is_ignored)
        return true;
      return false;
    };

    std::function<bool(const TypeWithModifier &)> is_unsatisfiable_twm;
    is_unsatisfiable_twm = [&](const TypeWithModifier &type_wm) {
      unsigned long ptr_cnt = type_wm.GetModifiers().count(Modifier::kPointer);
      unsigned long arr_cnt = type_wm.GetModifiers().count(Modifier::kArray);
      bool is_multidim_ptr = ptr_cnt + arr_cnt > 1;
      bool is_nullptr = type_wm.GetType() == PrimitiveType::kNullptrType;
      if (type_wm.IsBottomType() || is_nullptr || is_multidim_ptr) {
        return true;

      } else if (type_wm.IsClassType()) {
        const std::shared_ptr<Type> &type = type_wm.GetType();
        const std::shared_ptr<ClassType> &class_type = std::static_pointer_cast<ClassType>(type);
        return is_unsatisfiable_cls(class_type);

      } else if (type_wm.IsTemplateTypenameSpcType()) {
        const std::shared_ptr<Type> &type = type_wm.GetType();
        const std::shared_ptr<TemplateTypenameSpcType>
          &tt_type = std::static_pointer_cast<TemplateTypenameSpcType>(type);
        const std::shared_ptr<Type> &tgt_type = tt_type->GetTargetType();
        if (STLType::IsInstalledSTLType(tgt_type->GetName())) {
          const TemplateTypeInstList &inst_list = tt_type->GetInstList();
          const std::vector<TemplateTypeInstantiation> &insts = inst_list.GetInstantiations();
          for (const auto &item : insts) {
            bool is_type = item.IsType();
            if (is_type) {
              const TypeWithModifier &inst_twm = item.GetType();
              bool is_unsatisfiable = is_unsatisfiable_twm(inst_twm);
              if (is_unsatisfiable)
                return true;
            }
          }
        } else if (tgt_type->GetVariant() == TypeVariant::kClass) {
          const std::shared_ptr<ClassType> &cls_type = std::static_pointer_cast<ClassType>(tgt_type);
          return is_unsatisfiable_cls(cls_type);
        }

      } else if (type_wm.IsPrimitiveType()) {
        const std::shared_ptr<Type> &type = type_wm.GetType();
        bool is_void = type == PrimitiveType::kVoid;
        if (is_void)
          return true;
      }
      return false;
    };

    const auto &is_unsatisfiable_type = [&](const clang::QualType &arg) {
      const TWMSpec &twm_spec = TWMSpec::ByClangType(arg, nullptr);
      const TypeWithModifier &type_wm = TypeWithModifier::FromSpec(twm_spec);
      return is_unsatisfiable_twm(type_wm);
    };

    bool has_unsatisfiable_arg = std::any_of(arguments.begin(), arguments.end(), is_unsatisfiable_type);
    if (has_unsatisfiable_arg)
      return true;

    if (!e->IsNotRequireInvokingObj()) {
      const std::shared_ptr<ClassTypeModel> &class_owner = e->GetOwner();
      bool no_creator = has_creators.count(class_owner) == 0;
      bool is_ignored = ignored_cls.count(class_owner->GetQualifiedName()) > 0; // TRL-91
      if (no_creator || is_ignored)
        return true;
    }

    if (opt_return_type.has_value()) {
      const clang::QualType &return_type = opt_return_type.value();
      const TWMSpec &twm_spec = TWMSpec::ByClangType(return_type, nullptr);
      const TypeWithModifier &type_wm = TypeWithModifier::FromSpec(twm_spec);

      bool is_bottom = type_wm.IsBottomType(); // allow void and class with no creator
      if (!is_bottom && type_wm.IsClassType()) {
        const std::shared_ptr<ClassType> &cls_type = std::static_pointer_cast<ClassType>(type_wm.GetType());
        const std::shared_ptr<ClassTypeModel> &ctm = cls_type->GetModel();
        bool is_ignored = ignored_cls.count(ctm->GetQualifiedName()) > 0; // TRL-91
        if (is_ignored)
          return true;
      }
      return is_bottom;

    } else {
      // !has_unsatisfiable_arg && no return type -> nothing wrong -> return false :)
      return false;
    }
  };

  auto executable_filter_and_debug = [&executable_filter](const std::shared_ptr<Executable> &e) {
    bool should_remove = executable_filter(e);
    if (should_remove) {
      const std::string &dbg_str = e->DebugString();
      Logger::Warn("Filtering out executable with signature: " + dbg_str);
    }
    return should_remove;
  };

  const std::vector<std::shared_ptr<Executable>>::iterator &erase1 = std::remove_if(
    all_executables.begin(), all_executables.end(), executable_filter_and_debug);
  int has_removal1 = std::distance(erase1, all_executables.end());
  all_executables.erase(erase1, all_executables.end());

  const std::vector<std::shared_ptr<Creator>>::iterator &erase2 = std::remove_if(
    all_creators.begin(), all_creators.end(), executable_filter_and_debug);
  bool has_removal2 = std::distance(erase2, all_creators.end());
  all_creators.erase(erase2, all_creators.end());

  return has_removal1 + has_removal2;
}

void InstallRecognizedTypes(
  const std::vector<std::shared_ptr<cxxfoozz::ClassTypeModel>> &class_tms,
  const std::vector<std::shared_ptr<cxxfoozz::EnumTypeModel>> &enum_tms
) {
  cxxfoozz::ClassType::Install(class_tms);
  cxxfoozz::EnumType::Install(enum_tms);
}
std::array<std::string, 6> impl_exts{
  ".c", ".cc", ".cpp", ".c++", ".cp", ".cxx"
};

void InsertNewClassTM(std::vector<std::shared_ptr<ClassTypeModel>> &ctm, const std::shared_ptr<ClassTypeModel> &item) {
  ctm.push_back(item);
}

void InsertNewCreator(std::vector<std::shared_ptr<Creator>> &creators, const std::shared_ptr<Creator> &item) {
  creators.push_back(item);
}

void InsertNewExecutable(std::vector<std::shared_ptr<Executable>> &execs, const std::shared_ptr<Executable> &item) {
  execs.push_back(item);
}

bool ClassHasImplicitDefaultConstructor(clang::CXXRecordDecl *record_decl) {
  bool has_implicit_default_ctor = record_decl->hasDefaultConstructor(); // needsImplicitDefaultConstructor API (?)
  auto *decl_context = llvm::cast<clang::DeclContext>(record_decl);
  const clang::DeclContext::decl_range &decl_range = decl_context->decls();
  for (const auto &decl : decl_range) {
    clang::CXXMethodDecl *method;
    if (auto *func_template_decl = llvm::dyn_cast<clang::FunctionTemplateDecl>(decl)) {
      clang::FunctionDecl *func_decl = func_template_decl->getTemplatedDecl();
      method = llvm::dyn_cast<clang::CXXMethodDecl>(func_decl);
    } else if (auto *cxx_method_decl = llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
      if (cxx_method_decl->isDeleted())
        continue;
      method = cxx_method_decl;
      if (llvm::isa<clang::CXXDestructorDecl>(method))
        continue;
    } else {
      continue;
    }

    assert(method != nullptr);
    bool is_implicit = method->isImplicit();
    if (is_implicit) continue;

    if (auto ctor_decl = llvm::dyn_cast_or_null<clang::CXXConstructorDecl>(method)) {
      has_implicit_default_ctor = false;
    }
  }
  if (has_implicit_default_ctor) {
    const clang::CXXRecordDecl::base_class_range &bases = record_decl->bases();
    std::vector<clang::CXXRecordDecl *> parent_classes;
    for (const auto &base : bases) {
      const clang::QualType &base_type = base.getType();
      clang::CXXRecordDecl *base_cxx = base_type->getAsCXXRecordDecl();
      // because "clang::ElaboratedType" was found as base_type variable -> base_cxx became null
      if (base_cxx != nullptr) {
        bool has_default_ctor = base_cxx->hasDefaultConstructor();
        if (!has_default_ctor) {
          has_implicit_default_ctor = false;
        }
      }
    }
  }
  return has_implicit_default_ctor;
}

bool ClassHasPublicCopyConstructor(clang::CXXRecordDecl *record_decl) {
  auto *decl_context = llvm::cast<clang::DeclContext>(record_decl);
  const clang::DeclContext::decl_range &decl_range = decl_context->decls();
  for (const auto &decl : decl_range) {
    if (auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
      bool is_implicit = method->isImplicit();
      clang::AccessSpecifier specifier = method->getAccess();
      bool is_public_access = specifier == clang::AccessSpecifier::AS_public;

      if (auto ctor_decl = llvm::dyn_cast_or_null<clang::CXXConstructorDecl>(method)) {
        bool is_cctor = ctor_decl->isCopyConstructor();
        bool is_move_ctor = ctor_decl->isMoveConstructor();
        if (is_cctor) {
          if (is_public_access || is_implicit)
            return true;
          else if (ctor_decl->isDeleted() || !is_public_access)
            return false;
        }
      }
    }
  }
  return true;
}

bpstd::optional<const clang::CXXRecordDecl *> GetReturnTypeAsCXXDecl(const clang::QualType &call_ret_type) {
  const clang::Type *return_type = call_ret_type.getTypePtrOrNull();
  const clang::CXXRecordDecl *as_cxx;
  if (return_type->isPointerType() || return_type->isReferenceType()) {
    as_cxx = return_type->getPointeeCXXRecordDecl();
  } else {
    as_cxx = return_type->getAsCXXRecordDecl();
  }
  if (as_cxx == nullptr)
    return bpstd::nullopt;
  return {as_cxx};
}

// ignore decl from cpp files based on current assumptions
// (assumption 1: cxxfoozz targets testing compilable libraries)
// (assumption 2: to use the library, driver includes the header files)
bool IsLocatedInImplementationFile(clang::CXXRecordDecl *record_decl) {
  const clang::SourceLocation &src_loc = record_decl->getLocation();
  const clang::SourceManager &src_mgr = record_decl->getASTContext().getSourceManager();
  const std::string &loc_str = src_loc.printToString(src_mgr);
  std::string loc_lower;
  const auto &lastpos_filename_it = loc_str.begin() + (int) loc_str.find_first_of(':');
  std::transform(
    loc_str.begin(), lastpos_filename_it, std::back_inserter(loc_lower), [](char i) {
      return std::tolower(i);
    });

  const auto &ends_with = [](const std::string &value, const std::string &ending) {
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
  };
  const auto &is_impl_ext = [&](const std::string &loc_lower) {
    return std::any_of(
      impl_exts.begin(), impl_exts.end(), [&](const auto &ext) {
        return ends_with(loc_lower, ext);
      });
  };
  return is_impl_ext(loc_lower);
}

analysis::AnalysisResult ProgramAnalyzer::Analyze(const analysis::AnalysisSpec &spec) {
  std::vector<std::shared_ptr<ClassTypeModel>> all_class_tm;
  std::vector<std::shared_ptr<Executable>> all_executables;
  std::vector<std::shared_ptr<Creator>> all_creators;
  clang::MangleContext *mangle_ctx = spec.GetMangleCtx();

  std::map<clang::CXXRecordDecl *, clang::ClassTemplateDecl *> cls_tt_mapping;
  const std::vector<clang::ClassTemplateDecl *> &class_template_decls = spec.GetClassTemplateDecls();
  for (const auto &class_template_decl : class_template_decls) {
    clang::CXXRecordDecl *templated_class = class_template_decl->getTemplatedDecl();
    cls_tt_mapping.emplace(templated_class, class_template_decl);
  }

  ITMBuilder itm_builder;
  const std::vector<clang::CXXRecordDecl *> &record_decls = spec.GetRecordDecls();
  for (clang::CXXRecordDecl *record_decl : record_decls) {
    if (IsLocatedInImplementationFile(record_decl))
      continue;
    if (record_decl->isAnonymousStructOrUnion())
      continue;
    const std::string &qual_name = record_decl->getQualifiedNameAsString();
    if (qual_name.find("(anonymous") != std::string::npos)
      continue;

    bool is_private = record_decl->getAccess() == clang::AS_private;
    bool is_protected = record_decl->getAccess() == clang::AS_protected;
    if (is_private || is_protected)
      continue;

    if (!record_decl->isThisDeclarationADefinition()) {
      const std::shared_ptr<ClassTypeModel> &class_type_model =
        MakeClassTypeModel(record_decl, cls_tt_mapping, all_class_tm);
      InsertNewClassTM(all_class_tm, class_type_model);
      continue;
    }

    const clang::CXXRecordDecl::base_class_range &bases = record_decl->bases();
    std::vector<clang::CXXRecordDecl *> parent_classes;
    for (const auto &base : bases) {
      const clang::QualType &base_type = base.getType();
      clang::CXXRecordDecl *base_cxx = base_type->getAsCXXRecordDecl();
      // because "clang::ElaboratedType" was found as base_type variable -> base_cxx became null
      if (base_cxx != nullptr) {
        parent_classes.push_back(base_cxx);
      }
    }
    if (!parent_classes.empty())
      itm_builder.AddRelation(record_decl, parent_classes);

    const std::shared_ptr<ClassTypeModel> &class_type_model =
      MakeClassTypeModel(record_decl, cls_tt_mapping, all_class_tm);
    InsertNewClassTM(all_class_tm, class_type_model);
  }

  for (clang::CXXRecordDecl *record_decl : record_decls) {
    if (IsLocatedInImplementationFile(record_decl))
      continue;
    if (record_decl->isAnonymousStructOrUnion())
      continue;

    bool is_private = record_decl->getAccess() == clang::AS_private;
    bool is_protected = record_decl->getAccess() == clang::AS_protected;
    if (is_private || is_protected)
      continue;

    if (!record_decl->isThisDeclarationADefinition())
      continue;

    const std::string &class_qname = record_decl->getQualifiedNameAsString();
    const auto &find_it = std::find_if(
      all_class_tm.begin(), all_class_tm.end(), [&class_qname](const auto &item) {
        return item->GetQualifiedName() == class_qname;
      });
    if (find_it == all_class_tm.end())
      continue;

    const std::shared_ptr<ClassTypeModel> &class_type_model = *find_it;
    auto *decl_context = llvm::cast<clang::DeclContext>(record_decl);
    const clang::DeclContext::decl_range &decl_range = decl_context->decls();
    for (const auto &decl : decl_range) {
      clang::CXXMethodDecl *method;
      TemplateTypeParamList extracted_template_type_param_list;
      if (auto *func_template_decl = llvm::dyn_cast<clang::FunctionTemplateDecl>(decl)) {
        clang::FunctionDecl *func_decl = func_template_decl->getTemplatedDecl();
        method = llvm::dyn_cast<clang::CXXMethodDecl>(func_decl);
        extracted_template_type_param_list = ExtractTemplateType(func_template_decl);
      } else if (auto *cxx_method_decl = llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
        if (cxx_method_decl->isDeleted()) {
          // ##########
          // # Handling deleted operator= for lvalue reference
          // #####
//          clang::OverloadedOperatorKind oo_kind = cxx_method_decl->getOverloadedOperator();
//          bool is_op_eq = oo_kind == clang::OO_Equal;
//          if (is_op_eq) {
//            const llvm::MutableArrayRef<clang::ParmVarDecl *> &args = cxx_method_decl->parameters();
//            for (const auto &arg : args) {
//              const clang::QualType &arg_type = arg->getOriginalType();
//              bool is_lvalue_ref = arg_type->isLValueReferenceType();
//              if (is_lvalue_ref) {
//
//              }
//            }
//          }
          continue;
        }
        method = cxx_method_decl;
      } else if (auto *field_decl = llvm::dyn_cast<clang::FieldDecl>(decl)) {
        const std::string &name = field_decl->getNameAsString();
        const clang::QualType &type = field_decl->getType();
        bool is_public = field_decl->getAccess() == clang::AS_public;
        const FieldModel &field_model = FieldModel(name, type, is_public);
        class_type_model->AppendField(field_model);
        continue;
      } else {
        continue;
      }

      assert(method != nullptr);
      bool is_implicit = method->isImplicit();
      if (is_implicit)
        continue;
//      std::cout << "Found method: " << method->getNameAsString() << std::endl;
//      std::cout << extracted_template_type_param_list.DebugString() << std::endl;

      clang::AccessSpecifier specifier = method->getAccess();
      bool is_public_access = specifier == clang::AccessSpecifier::AS_public;
      if (llvm::isa<clang::CXXDestructorDecl>(method))
        continue;

      const std::vector<clang::QualType> &arguments = ExtractArguments(method);

      if (auto ctor_decl = llvm::dyn_cast_or_null<clang::CXXConstructorDecl>(method)) {
        bool is_cctor = ctor_decl->isCopyConstructor();
        bool is_move_ctor = ctor_decl->isMoveConstructor();
        if (is_cctor || is_move_ctor) {
          std::string ctor_desc = is_cctor ? "copy" : "move";
          Logger::Info("Ignoring " + ctor_desc + " constructor: " + ctor_decl->getQualifiedNameAsString());
          continue;
        }

        bool is_abstract = record_decl->isAbstract();
        if (is_public_access && !is_abstract) {
          const std::shared_ptr<Creator> &creator =
            Creator::MakeConstructorCreator(class_type_model, arguments, method);
          creator->SetTemplateParamList(extracted_template_type_param_list);
          InsertNewCreator(all_creators, creator);
        }

      } else {
        // ignore all non-public methods
        if (!is_public_access)
          continue;

        const clang::QualType &call_result_type = method->getCallResultType();
        const bpstd::optional<const clang::CXXRecordDecl *> &as_cxx_decl = GetReturnTypeAsCXXDecl(call_result_type);
        std::shared_ptr<ClassTypeModel> target_cls = class_type_model;

        bool return_type_is_cls = as_cxx_decl.has_value();
        if (return_type_is_cls) {
          const clang::CXXRecordDecl *cxx_decl = as_cxx_decl.value();
          const std::string &qname = cxx_decl->getQualifiedNameAsString();
          const std::string &curr_cls_qname = class_type_model->GetQualifiedName();
          if (qname != curr_cls_qname) {
            const auto &find_it2 = std::find_if(
              all_class_tm.begin(), all_class_tm.end(), [&qname](const std::shared_ptr<ClassTypeModel> &ctm) {
                return ctm->GetQualifiedName() == qname;
              });
            if (find_it2 != all_class_tm.end()) {
              target_cls = *find_it2;
            } else if (STLType::IsSTLType(qname)) {
              return_type_is_cls = false;
            } else {
              return_type_is_cls = false;
//              assert(false);
            }
          }
        }

        bool is_static_factory = return_type_is_cls && method->isStatic();
        if (is_static_factory) {
          const std::shared_ptr<Creator> &creator =
            Creator::MakeStaticFactoryCreator(class_type_model, target_cls, arguments, method, mangle_ctx);
          creator->SetTemplateParamList(extracted_template_type_param_list);
          InsertNewCreator(all_creators, creator);

        } else {
          const std::shared_ptr<Executable> &executable =
            Executable::MakeMethodExecutable(class_type_model, arguments, method, mangle_ctx);
          executable->SetTemplateParamList(extracted_template_type_param_list);
          InsertNewExecutable(all_executables, executable);
        }
      }
    }
    // TODO (TRL-10): Should consider public fields too.
    // All non-primitive returning methods are creators as well! *possible cross-class creators

    bool has_implicit_default_ctor = ClassHasImplicitDefaultConstructor(record_decl);
    bool is_abstract = record_decl->isAbstract();
    if (has_implicit_default_ctor && !is_abstract) {
      const std::shared_ptr<Creator> &implicit_default_ctor = Creator::MakeImplicitDefaultCtor(class_type_model);
      InsertNewCreator(all_creators, implicit_default_ctor);

      if (class_type_model->IsAllPublicFields()) {
        const std::shared_ptr<Creator> &implicit_ctor_by_fields = Creator::MakeImplicitCtorByFields(class_type_model);
        InsertNewCreator(all_creators, implicit_ctor_by_fields);
      }
    }

    bool has_public_cctor = ClassHasPublicCopyConstructor(record_decl);
    if (!has_public_cctor) {
      class_type_model->SetHasPublicCctor(false);
    }
  }

  std::vector<std::shared_ptr<EnumTypeModel>> all_enum_tm;
  const std::vector<clang::EnumDecl *> &enum_decls = spec.GetEnumDecls();
  for (const auto &enum_decl : enum_decls) {
    bool is_anon = enum_decl->getQualifiedNameAsString().find("(anonymous") != std::string::npos;
    if (is_anon) continue;
    bool is_empty_enum = enum_decl->enumerators().empty();
    if (is_empty_enum) continue;
    const std::shared_ptr<EnumTypeModel> &type_model = MakeEnumTypeModel(enum_decl);
    all_enum_tm.push_back(type_model);
  }

  std::map<clang::FunctionDecl *, clang::FunctionTemplateDecl *> func_tt_mapping;
  const std::vector<clang::FunctionTemplateDecl *> &func_template_decls = spec.GetFuncTemplateDecls();
  for (const auto &func_template_decl : func_template_decls) {
    clang::FunctionDecl *templated_decl = func_template_decl->getTemplatedDecl();
    func_tt_mapping.emplace(templated_decl, func_template_decl);
  }

  const std::vector<clang::FunctionDecl *> &glob_func_decls = spec.GetFuncDecls();
  for (const auto &func_decl : glob_func_decls) {
    if (func_decl->isOverloadedOperator())
      continue;
    if (llvm::isa<clang::CXXConversionDecl>(func_decl))
      continue;
    const std::string &qual_name = func_decl->getQualifiedNameAsString();
    if (qual_name.find("(anonymous") != std::string::npos)
      continue;

    std::shared_ptr<Executable> new_executable;
    const std::vector<clang::QualType> &arguments = ExtractArguments(func_decl);

    const clang::QualType &call_ret_type = func_decl->getReturnType();
    const bpstd::optional<const clang::CXXRecordDecl *> &as_cxx_decl = GetReturnTypeAsCXXDecl(call_ret_type);
    bool return_type_is_cls = as_cxx_decl.has_value();
    if (return_type_is_cls) {
      const clang::CXXRecordDecl *cxx_decl = as_cxx_decl.value();
      const std::string &qname = cxx_decl->getQualifiedNameAsString();
      const auto &find_it = std::find_if(
        all_class_tm.begin(), all_class_tm.end(), [&qname](const std::shared_ptr<ClassTypeModel> &ctm) {
          return ctm->GetQualifiedName() == qname;
        });

      if (find_it != all_class_tm.end()) {
        const std::shared_ptr<ClassTypeModel> &target_cls = *find_it;
        const std::shared_ptr<Creator> &ext_creator = Creator::MakeExternalCreator(
          target_cls,
          arguments,
          func_decl,
          mangle_ctx);
        InsertNewCreator(all_creators, ext_creator);
        new_executable = ext_creator;
      } else {
        const std::shared_ptr<Executable> &ext_executable = Executable::MakeExternalExecutable(
          arguments,
          func_decl,
          mangle_ctx);
        InsertNewExecutable(all_executables, ext_executable);
        new_executable = ext_executable;
      }

    } else {
      const std::shared_ptr<Executable> &ext_executable = Executable::MakeExternalExecutable(
        arguments,
        func_decl,
        mangle_ctx);
      InsertNewExecutable(all_executables, ext_executable);
      new_executable = ext_executable;
    }

    if (new_executable != nullptr) {
      auto find_template_decl = func_tt_mapping.find(func_decl);
      bool is_templated_func = find_template_decl != func_tt_mapping.end();
      if (is_templated_func) {
        clang::FunctionTemplateDecl *template_decl = find_template_decl->second;
        const TemplateTypeParamList &template_param_list = ExtractTemplateType(template_decl);
        new_executable->SetTemplateParamList(template_param_list);
      }
    }
  }

  // concatenate creators into executables (because creator IS A executable)
  all_executables.insert(all_executables.end(), all_creators.begin(), all_creators.end());

  // FilterOutUnsatisfiableExecutables requires to Install types first :)
  InstallRecognizedTypes(all_class_tm, all_enum_tm);

  // Build inheritance model
  const std::shared_ptr<InheritanceTreeModel> &inheritance_model = itm_builder.Build(all_class_tm);

  int total_removed = 0, curr_removed;
  do {
    curr_removed = FilterOutUnsatisfiableExecutables(all_executables, all_creators, inheritance_model);
    total_removed += curr_removed;
  } while (curr_removed > 0);
  int remain = (int) all_executables.size();
  Logger::Warn(
    "Filtering out " + std::to_string(total_removed) + " executables for unhandled arguments. "
      + "Remaining = " + std::to_string(remain));

  // TODO (TRL-10): Functions outside CXX classes
  return analysis::AnalysisResult{
    all_class_tm,
    all_executables,
    all_creators,
    all_enum_tm,
    inheritance_model,
  };
}

namespace analysis {

const std::vector<clang::CXXRecordDecl *> &AnalysisSpec::GetRecordDecls() const {
  return record_decls_;
}
const std::vector<clang::EnumDecl *> &AnalysisSpec::GetEnumDecls() const {
  return enum_decls_;
}
const std::vector<clang::ClassTemplateDecl *> &AnalysisSpec::GetClassTemplateDecls() const {
  return class_template_decls_;
}
const std::vector<clang::FunctionDecl *> &AnalysisSpec::GetFuncDecls() const {
  return func_decls_;
}
const std::vector<clang::FunctionTemplateDecl *> &AnalysisSpec::GetFuncTemplateDecls() const {
  return func_template_decls_;
}
AnalysisSpec AnalysisSpec::FromTraversalResult(
  const ClassTraversingResult &traversal_result,
  clang::MangleContext *mangle_ctx
) {
  AnalysisSpec result;
  result.record_decls_ = traversal_result.GetRecordDecls();
  result.enum_decls_ = traversal_result.GetEnumDecls();
  result.class_template_decls_ = traversal_result.GetClassTemplateDecls();
  result.func_decls_ = traversal_result.GetFuncDecls();
  result.func_template_decls_ = traversal_result.GetFuncTemplateDecls();
  result.mangle_ctx_ = mangle_ctx;
  return result;
}
clang::MangleContext *AnalysisSpec::GetMangleCtx() const {
  return mangle_ctx_;
}

AnalysisResult::AnalysisResult(
  std::vector<std::shared_ptr<ClassTypeModel>> class_type_models,
  std::vector<std::shared_ptr<Executable>> executables,
  std::vector<std::shared_ptr<Creator>> creators,
  std::vector<std::shared_ptr<EnumTypeModel>> enum_type_models,
  std::shared_ptr<InheritanceTreeModel> inheritance_model
)
  : class_type_models_(std::move(class_type_models)),
    executables_(std::move(executables)),
    creators_(std::move(creators)),
    enum_type_models_(std::move(enum_type_models)),
    inheritance_model_(std::move(inheritance_model)) {}
const std::vector<std::shared_ptr<ClassTypeModel>> &AnalysisResult::GetClassTypeModels() const {
  return class_type_models_;
}
const std::vector<std::shared_ptr<Executable>> &AnalysisResult::GetExecutables() const {
  return executables_;
}
const std::vector<std::shared_ptr<Creator>> &AnalysisResult::GetCreators() const {
  return creators_;
}
const std::vector<std::shared_ptr<EnumTypeModel>> &AnalysisResult::GetEnumTypeModels() const {
  return enum_type_models_;
}
const std::shared_ptr<InheritanceTreeModel> &AnalysisResult::GetInheritanceModel() const {
  return inheritance_model_;
}

} // namespace analysis

} // namespace cxxfoozz

