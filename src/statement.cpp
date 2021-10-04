#include "logger.hpp"
#include "statement.hpp"

#include <iostream>
#include <sstream>
#include <utility>

namespace cxxfoozz {

// ##########
// # Statement
// #####

const TypeWithModifier &Statement::GetType() const {
  return type_;
}
Statement::Statement(const TypeWithModifier &type) : type_(type) {
  assert(!type.IsTemplateTypenameType());
}
void Statement::SetType(const TypeWithModifier &type) {
  type_ = type;
}
const bpstd::optional<std::string> &Statement::GetVarName() const {
  return var_name_;
}
void Statement::SetVarName(const bpstd::optional<std::string> &var_name) {
  var_name_ = var_name;
}
void Statement::ClearVarName() {
  var_name_ = bpstd::nullopt;
}
Statement::~Statement() = default;

// ##########
// # Operand
// #####

Operand Operand::MakeRefOperand(const std::shared_ptr<Statement> &ref) {
  return Operand{ref->GetType(), ref, bpstd::nullopt};
}
Operand Operand::MakeConstantOperand(const TypeWithModifier &type, const std::string &literal) {
  return Operand{type, nullptr, bpstd::make_optional(literal)};
}
Operand::Operand(
  TypeWithModifier type,
  std::shared_ptr<Statement> ref,
  bpstd::optional<std::string> constant_literal
) : type_(type), ref_(std::move(ref)), constant_literal_(std::move(constant_literal)) {}
const TypeWithModifier &Operand::GetType() const {
  return type_;
}
const std::shared_ptr<Statement> &Operand::GetRef() const {
  return ref_;
}
const bpstd::optional<std::string> &Operand::GetConstantLiteral() const {
  return constant_literal_;
}
OperandType Operand::GetOperandType() const {
  if (ref_ == nullptr)
    return OperandType::kConstantOperand;
  return OperandType::kRefOperand;
}
std::string Operand::InternalToString() const {
  if (ref_ == nullptr) {
    bool is_primitive = type_.IsPrimitiveType();
    bool is_char_star = type_.IsPointerOrArray() && type_.GetType() == PrimitiveType::kCharacter;
    const std::string &value = constant_literal_.value();
    bool is_nullptr = value == "nullptr";
    if (is_char_star && !is_nullptr) {
      return '"' + value + '"';
    } else if (kLibFuzzerMode) {
      if (is_nullptr || !is_primitive) {
        return value;
      } else {
        const std::string &type_name = type_.GetType()->GetName();
        bool is_unsigned = type_.IsUnsigned();
        return std::string("Get<") + (is_unsigned ? "unsigned " : "") + type_name + ">()";
      }
    } else {
      return value;
    }
  }

  const TypeWithModifier &twm = ref_->GetType();
  bool is_void = twm.GetType() == PrimitiveType::kVoid;
  bool is_ptr = twm.IsPointer();
  assert(!is_void || (is_void && is_ptr));

  const bpstd::optional<std::string> &opt_var_name = ref_->GetVarName();
  if (!opt_var_name.has_value()) {
    Logger::Error("Operand::ToStringWithAutoCasting", "Unnamed RefOperand", true);
    return "unnamed";
  }

  return opt_var_name.value();
}

std::string Operand::ToStringWithAutoCasting(
  const TypeWithModifier &type_rq,
  const std::shared_ptr<InheritanceTreeModel> &itm
) const {
  return ToStringWithAutoCasting(type_rq, nullptr, itm);
}

bool RequireConstPointerCasting(const TypeWithModifier &op_twm, const TypeWithModifier &rq_twm) {
  return op_twm.IsPointer() && op_twm.IsConst() && rq_twm.IsPointer() && !rq_twm.IsConst();
}

std::string Operand::ToStringWithAutoCasting(
  const TypeWithModifier &type_rq,
  const std::shared_ptr<TemplateTypeContext> &tt_ctx,
  const std::shared_ptr<InheritanceTreeModel> &itm
) const {
  bool is_assignable = type_rq.IsAssignableFrom(type_, tt_ctx, itm);
  if (!is_assignable) {
    if (type_.IsPrimitiveType() && type_rq.IsPrimitiveType()) {
    } else {
      assert(false);
    }
  }
  std::stringstream ss;

  const TypeWithModifier &operand_type = type_.ResolveTemplateType(tt_ctx);
  const TypeWithModifier &required_type = type_rq.ResolveTemplateType(tt_ctx);
  bool require_casting = required_type.GetType() != operand_type.GetType();
  // oh no, what is this for?
  if (required_type.IsClassType() && operand_type.IsTemplateTypenameSpcType()) {
    const std::shared_ptr<ClassType> &cls_type = std::static_pointer_cast<ClassType>(required_type.GetType());
    const std::shared_ptr<TemplateTypenameSpcType> &tt_spc_type =
      std::static_pointer_cast<TemplateTypenameSpcType>(operand_type.GetType());
    const std::shared_ptr<Type> &tgt_type = tt_spc_type->GetTargetType();
    require_casting = false;
  }
  if (RequireConstPointerCasting(operand_type, required_type)) {
    require_casting = true;
  } else if (IsNullPtr()) {
    require_casting = true;
  }

  // special case: void ptr must be casted into its propriate ptr type first
  // e.g. void* void1; --to make int--> *(int*)void1;
  if (operand_type.IsVoidPtr()) {
    require_casting = false;
    bool rq_ptr = type_rq.IsPointer();
    if (rq_ptr) {
      const std::string &typecast_str = type_rq.ToString(tt_ctx);
      ss << '(' << typecast_str << ") ";
    } else {
      const std::string &typecast_str = type_rq.WithAdditionalModifiers({Modifier::kPointer}).ToString(tt_ctx);
      ss << "*(" << typecast_str << ") ";
    }

  } else {
    if (require_casting) {
      const std::string &typecast_str = type_rq.ToString(tt_ctx);
      ss << '(' << typecast_str << ") ";
    }

    if (GetOperandType() == OperandType::kRefOperand) {
      if (type_rq.IsPointerOrArray() && !type_.IsPointerOrArray())
        ss << '&';
      else if (type_.IsPointerOrArray() && !type_rq.IsPointerOrArray())
        ss << '*';
    }
  }


  // Apply std::move for std::unique_ptr
  // or if the required type is rvalue reference
  bool apply_std_move = false;
  bool is_ref_operand = GetOperandType() == OperandType::kRefOperand;
  if (required_type.IsRValueReference() && is_ref_operand) {
    apply_std_move = true;
  }
  if (required_type.IsTemplateTypenameSpcType()) {
    const std::shared_ptr<TemplateTypenameSpcType> &tt_spc_type =
      std::static_pointer_cast<TemplateTypenameSpcType>(required_type.GetType());
    const std::shared_ptr<Type> &target_type = tt_spc_type->GetTargetType();
    if (target_type == STLType::kUniquePtr && !required_type.IsReference() && !required_type.IsPointerOrArray()) {
      apply_std_move = true;
    }
  }

  ss << InternalToString();
  return apply_std_move
         ? "std::move(" + ss.str() + ")"
         : ss.str();
}

Operand &Operand::operator=(const Operand &other) {
  if (this == &other)
    return *this;
  type_ = other.type_;
  constant_literal_ = other.constant_literal_;
  std::atomic_store(&ref_, other.ref_);
  return *this;
}
Operand::Operand(const Operand &other)
  : type_(other.GetType()), ref_(other.GetRef()), constant_literal_(other.GetConstantLiteral()) {}
bool Operand::operator==(const Operand &rhs) const {
  return type_ == rhs.type_ &&
    ref_ == rhs.ref_ &&
    constant_literal_ == rhs.constant_literal_;
}
bool Operand::operator!=(const Operand &rhs) const {
  return !(rhs == *this);
}
Operand Operand::MakeBottom() {
  return Operand{TypeWithModifier::Bottom(), nullptr, bpstd::nullopt};
}
bool Operand::IsNullPtr() const {
  return GetOperandType() == OperandType::kConstantOperand
    && GetType().IsPointer()
    && GetConstantLiteral().value_or("") == "nullptr";
}
bool Operand::kLibFuzzerMode = false;
bool Operand::IsKLibFuzzerMode() {
  return kLibFuzzerMode;
}

// ##########
// # PrimitiveAssignmentStatement
// #####

OpArity GetPrimitiveOperatorArity(GeneralPrimitiveOp op) {
  if (op == GeneralPrimitiveOp::kNop || op == GeneralPrimitiveOp::kMinus)
    return OpArity::kUnary;
  return OpArity::kBinary;
}

std::shared_ptr<Statement> PrimitiveAssignmentStatement::MakeUnaryOpStatement(const Operand &op, GeneralPrimitiveOp o) {
  return std::make_shared<PrimitiveAssignmentStatement>(op.GetType(), o, std::vector<Operand>{op});
}
std::shared_ptr<Statement> PrimitiveAssignmentStatement::MakeBinOpStatement(
  const Operand &op1,
  const Operand &op2,
  GeneralPrimitiveOp o
) {
  const TypeWithModifier &operand_type = op1.GetType();
  return std::make_shared<PrimitiveAssignmentStatement>(operand_type, o, std::vector<Operand>{op1, op2});
}
PrimitiveAssignmentStatement::PrimitiveAssignmentStatement(
  const TypeWithModifier &type,
  GeneralPrimitiveOp op,
  std::vector<Operand> operands
) : Statement(type), op_(op), operands_(std::move(operands)) {}
GeneralPrimitiveOp PrimitiveAssignmentStatement::GetOp() const {
  return op_;
}
std::vector<Operand> &PrimitiveAssignmentStatement::GetOperands() {
  return operands_;
}
std::shared_ptr<Statement> PrimitiveAssignmentStatement::Clone() {
  return std::make_shared<PrimitiveAssignmentStatement>(
    GetType(),
    GetOp(),
    GetOperands()
  );
}
StatementVariant PrimitiveAssignmentStatement::GetVariant() const {
  return StatementVariant::kPrimitiveAssignment;
}
OpArity PrimitiveAssignmentStatement::GetOpArity() const {
  return GetPrimitiveOperatorArity(op_);
}
void PrimitiveAssignmentStatement::SetOp(GeneralPrimitiveOp op) {
  op_ = op;
}
void PrimitiveAssignmentStatement::SetOperands(const std::vector<Operand> &operands) {
  operands_ = operands;
}

bool TryReplaceRefOperand(
  Operand &operand,
  const std::map<std::shared_ptr<Statement>, std::shared_ptr<Statement>> &repl_map,
  const std::shared_ptr<TemplateTypeContext> &tt_ctx
) {
  bool is_ref = operand.GetOperandType() == OperandType::kRefOperand;
  if (!is_ref)
    return false;

  const std::shared_ptr<Statement> &ref_stmt = operand.GetRef();
  const auto &find_it = repl_map.find(ref_stmt);
  if (find_it == repl_map.end())
    return false;

  const std::shared_ptr<Statement> &target_stmt = find_it->second;
  const Operand &new_ref_operand = Operand::MakeRefOperand(target_stmt);
  const TypeWithModifier &new_type = new_ref_operand.GetType();
  const TypeWithModifier &old_type = target_stmt->GetType();
//  assert(old_type.IsAssignableFrom(new_type, tt_ctx, itm));
  operand = new_ref_operand;
  return true;
}

std::pair<std::shared_ptr<Statement>, int> PrimitiveAssignmentStatement::ReplaceRefOperand(
  const std::map<std::shared_ptr<Statement>, std::shared_ptr<Statement>> &repl_map,
  const std::shared_ptr<TemplateTypeContext> &tt_ctx
) {
  const std::shared_ptr<PrimitiveAssignmentStatement> &cloned =
    std::static_pointer_cast<PrimitiveAssignmentStatement>(Clone());
  int replacement = 0;
  for (auto &operand : cloned->operands_) {
    if (TryReplaceRefOperand(operand, repl_map, tt_ctx))
      ++replacement;
  }
  return {cloned, replacement};
}
std::vector<Operand> PrimitiveAssignmentStatement::GetStatementOperands() const {
  return operands_;
}
PrimitiveAssignmentStatement::~PrimitiveAssignmentStatement() = default;

// ##########
// # CallStatement
// #####

CallStatement::CallStatement(
  const TypeWithModifier &type,
  std::shared_ptr<Executable> target,
  std::vector<Operand> operands,
  bpstd::optional<Operand> invoking_obj,
  const std::shared_ptr<TemplateTypeContext> &template_type_context
)
  : Statement(type),
    target_(std::move(target)),
    operands_(std::move(operands)),
    invoking_obj_(std::move(invoking_obj)),
    template_type_context_(TemplateTypeContext::Clone(template_type_context)) {
}
const std::shared_ptr<Executable> &CallStatement::GetTarget() const {
  return target_;
}
const bpstd::optional<Operand> &CallStatement::GetInvokingObj() const {
  return invoking_obj_;
}
std::vector<Operand> &CallStatement::GetOperands() {
  return operands_;
}
std::shared_ptr<Statement> CallStatement::MakeExecutableCall(
  const std::shared_ptr<Executable> &target,
  const std::vector<Operand> &ops,
  const bpstd::optional<Operand> &invoking_obj,
  const std::shared_ptr<TemplateTypeContext> &tt_ctx
) {
  if (target->GetExecutableVariant() == ExecutableVariant::kConstructor) {
    const std::string &class_name = target->GetOwner()->GetQualifiedName();
    const std::shared_ptr<ClassType> &type_ptr = ClassType::GetTypeByQualName(class_name);
    const std::shared_ptr<ClassTypeModel> &class_model = type_ptr->GetModel();

    if (class_model->IsTemplatedClass()) {
      TemplateTypeInstMapping &tt_inst_mapping = tt_ctx->GetMapping();
      const TemplateTypeInstList &tt_inst_list = tt_inst_mapping.LookupForClass(class_model);

      const std::shared_ptr<TemplateTypenameSpcType> &tt_spc_type =
        TemplateTypenameSpcType::From(type_ptr, tt_inst_list);
      const TWMSpec &twm_spec = TWMSpec::ByType(tt_spc_type, nullptr);
      const TypeWithModifier &tt_spc_twm = TypeWithModifier::FromSpec(twm_spec);
      return std::make_shared<CallStatement>(tt_spc_twm, target, ops, invoking_obj, tt_ctx);

    } else {
      const TWMSpec &twm_spec = TWMSpec::ByType(type_ptr, nullptr);
      const TypeWithModifier &type_with_modifier = TypeWithModifier::FromSpec(twm_spec);
      assert(type_with_modifier.GetType() != nullptr);
      return std::make_shared<CallStatement>(type_with_modifier, target, ops, invoking_obj, tt_ctx);
    }

  } else {
    const clang::QualType &return_type = target->GetReturnType().value();
    const TWMSpec &twm_spec = TWMSpec::ByClangType(return_type, nullptr);
    const TypeWithModifier &twm = TypeWithModifier::FromSpec(twm_spec);
    TypeWithModifier resolved_twm =
      twm.IsTemplateTypenameType() ? twm.ResolveTemplateType(tt_ctx) : twm;
    bool is_copy_value = !resolved_twm.IsPointerOrArray() && !resolved_twm.IsReference();
    if (is_copy_value) {
      resolved_twm = resolved_twm.StripParticularModifiers({Modifier::kConst});
    }

    const std::shared_ptr<Type> &inner_type = resolved_twm.GetType();
    assert(inner_type != nullptr);
    bool is_class = resolved_twm.IsClassType();
    if (is_class) {
      const std::shared_ptr<ClassType> &cls_type = std::static_pointer_cast<ClassType>(inner_type);
      const std::shared_ptr<ClassTypeModel> &ctm = cls_type->GetModel();
      bool has_cctor = ctm->IsHasPublicCctor();
      if (!has_cctor) {
        bool is_ref = resolved_twm.IsReference();
        bool is_ptr = resolved_twm.IsPointer();
        if (!is_ref && !is_ptr) {
          const TypeWithModifier &twm_with_const_ref =
            resolved_twm.WithAdditionalModifiers({Modifier::kConst, Modifier::kReference});
          return std::make_shared<CallStatement>(twm_with_const_ref, target, ops, invoking_obj, tt_ctx);
        }
      }
    }
    return std::make_shared<CallStatement>(resolved_twm, target, ops, invoking_obj, tt_ctx);
  }
}

std::shared_ptr<Statement> CallStatement::Clone() {
  return std::make_shared<CallStatement>(
    GetType(),
    GetTarget(),
    GetOperands(),
    GetInvokingObj(),
    TemplateTypeContext::Clone(GetTemplateTypeContext())
  );
}
StatementVariant CallStatement::GetVariant() const {
  return StatementVariant::kCall;
}
void CallStatement::SetTarget(const std::shared_ptr<Executable> &target) {
  target_ = target;
}
void CallStatement::SetOperands(const std::vector<Operand> &operands) {
  operands_ = operands;
}
void CallStatement::SetInvokingObj(const bpstd::optional<Operand> &invoking_obj) {
  invoking_obj_ = invoking_obj;
}
std::pair<std::shared_ptr<Statement>, int> CallStatement::ReplaceRefOperand(
  const std::map<std::shared_ptr<Statement>, std::shared_ptr<Statement>> &repl_map,
  const std::shared_ptr<TemplateTypeContext> &tt_ctx
) {
  const std::shared_ptr<CallStatement> &cloned = std::static_pointer_cast<CallStatement>(Clone());
  int replacement = 0;
  for (auto &operand : cloned->operands_) {
    if (TryReplaceRefOperand(operand, repl_map, tt_ctx))
      ++replacement;
  }
  if (cloned->invoking_obj_.has_value()) {
    Operand &operand = cloned->invoking_obj_.value();
    if (TryReplaceRefOperand(operand, repl_map, tt_ctx))
      ++replacement;
  }
  return {cloned, replacement};
}
std::vector<Operand> CallStatement::GetStatementOperands() const {
  std::vector<Operand> result(operands_);
  if (invoking_obj_.has_value())
    result.push_back(invoking_obj_.value());
  return result;
}
const std::shared_ptr<TemplateTypeContext> &CallStatement::GetTemplateTypeContext() const {
  return template_type_context_;
}

// ##########
// # StatementWriter
// #####

std::string StatementWriter::StmtAsString(const std::shared_ptr<Statement> &stmt, unsigned int stmt_id) {
  StatementVariant stmt_variant = stmt->GetVariant();
  switch (stmt_variant) {
    case StatementVariant::kPrimitiveAssignment: {
      const std::shared_ptr<PrimitiveAssignmentStatement> &primitive_stmt =
        std::static_pointer_cast<PrimitiveAssignmentStatement>(stmt);
      return PrimitiveAssStmtAsString(primitive_stmt, stmt_id);
    }
    case StatementVariant::kCall: {
      const std::shared_ptr<CallStatement> &call_stmt = std::static_pointer_cast<CallStatement>(stmt);
      return CallStmtAsString(call_stmt, stmt_id);
    }
    case StatementVariant::kSTLConstruction: {
      const std::shared_ptr<STLStatement> &stl_stmt = std::static_pointer_cast<STLStatement>(stmt);
      STLStatementWriter stl_stmt_writer{context_};
      return stl_stmt_writer.STLStmtAsString(stl_stmt, stmt_id);
    }
    case StatementVariant::kArrayInitialization: {
      const std::shared_ptr<ArrayInitStatement> &arr_stmt = std::static_pointer_cast<ArrayInitStatement>(stmt);
      return ArrayInitStmtAsString(arr_stmt, stmt_id);
    }
  }
}
std::string StatementWriter::PrimitiveAssStmtAsString(
  const std::shared_ptr<PrimitiveAssignmentStatement> &stmt,
  unsigned int stmt_id
) {
  const TypeWithModifier &type = stmt->GetType();
  const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm = context_->GetInheritanceModel();

  std::stringstream var_name;
  var_name << type.GetDefaultVarName() << stmt_id;

  std::stringstream ss;
  ss << type.ToString() << " " << var_name.str() << " = ";

  const GeneralPrimitiveOp op_ = stmt->GetOp();
  const std::vector<Operand> &operands_ = stmt->GetOperands();
  OpArity arity = GetPrimitiveOperatorArity(op_);
  if (arity == OpArity::kUnary) {
    const Operand &operand = operands_[0];
    const std::string &operand_str = operand.ToStringWithAutoCasting(type, itm);
    if (op_ == GeneralPrimitiveOp::kMinus) ss << "-(" << operand_str << ")";
    else ss << operand_str;

  } else {
    const Operand &o1 = operands_[0], &o2 = operands_[1];
    ss << o1.ToStringWithAutoCasting(type, itm);
    if (op_ == GeneralPrimitiveOp::kAdd) ss << " + ";
    else if (op_ == GeneralPrimitiveOp::kSub) ss << " - ";
    else if (op_ == GeneralPrimitiveOp::kMul) ss << " * ";
    else if (op_ == GeneralPrimitiveOp::kDiv) ss << " / ";
    else if (op_ == GeneralPrimitiveOp::kMod) ss << " % ";
    ss << o2.ToStringWithAutoCasting(type, itm);
  }
  stmt->SetVarName(bpstd::make_optional(var_name.str()));
  return ss.str();
}
std::string StatementWriter::CallStmtAsString(const std::shared_ptr<CallStatement> &stmt, unsigned int stmt_id) {
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = stmt->GetTemplateTypeContext();
  const std::shared_ptr<Executable> &target = stmt->GetTarget();
  assert(target != nullptr);

  // ##########
  // # LHS Type = statement's type
  // #####
  const TypeWithModifier &stmt_type = stmt->GetType();
  const std::shared_ptr<Type> &stmt_inner_type = stmt_type.GetType();

  std::stringstream ss;
  bpstd::optional<std::string> var_name_opt = bpstd::nullopt;

  bool is_void = stmt_inner_type == PrimitiveType::kVoid;
  bool is_void_ptr = is_void && stmt_type.IsPointer();
  bool is_ctor_call = target->GetExecutableVariant() == ExecutableVariant::kConstructor;
  if (is_void_ptr || !is_void) {
    std::string var_name = stmt_type.GetDefaultVarName() + std::to_string(stmt_id);
    var_name_opt = bpstd::make_optional(var_name);
    ss << stmt_type.ToString(tt_ctx) << " " << var_name;
    if (!is_ctor_call)
      ss << " = ";
  }

  // ##########
  // # RHS Type = method's return type
  // #####
  const bpstd::optional<clang::QualType> &opt_return_type = target->GetReturnType();
  if (opt_return_type.has_value()) {
    const clang::QualType &return_type = opt_return_type.value();
    const TWMSpec &twm_spec = TWMSpec::ByClangType(return_type, nullptr);
    const TypeWithModifier &type_with_modifier = TypeWithModifier::FromSpec(twm_spec);
    const TypeWithModifier &rhs_type =
      type_with_modifier.IsTemplateTypenameType() ? type_with_modifier.ResolveTemplateType(tt_ctx) : type_with_modifier;
    assert(rhs_type.GetType() != nullptr);

    if (RequireConstPointerCasting(rhs_type, stmt_type)) {
      ss << '(' << stmt_type.ToString(tt_ctx) << ") ";
    }
  }

  // ##########
  // # Arguments
  // #####
  if (stmt->GetInvokingObj().has_value()) {
    const Operand &invoking_operand = stmt->GetInvokingObj().value();
    const TypeWithModifier &operand_type = invoking_operand.GetType();
    bool is_ptr = operand_type.IsPointer();
    bool is_array = operand_type.IsArray();

    const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm = context_->GetInheritanceModel();
    if (is_ptr || is_array)
      ss << invoking_operand.ToStringWithAutoCasting(operand_type, itm) << "->"; // handling array with [0] only
    else
      ss << invoking_operand.ToStringWithAutoCasting(operand_type, itm) << ".";

  } else if (target->IsMember() && target->IsNotRequireInvokingObj()) {
    const std::shared_ptr<ClassTypeModel> &owner = target->GetOwner();
    std::string template_inst_str;
    if (owner->IsTemplatedClass()) {
      const TemplateTypeInstList &inst_list = tt_ctx->GetMapping().LookupForClass(owner);
      template_inst_str = inst_list.ToString();
    }
    const std::string &class_name = owner->GetQualifiedName();
    ss << class_name << template_inst_str << "::";
  }

  if (is_ctor_call) {
// ##########
// # LEGACY CODE: it was <type> <varname> = <ctor name>{<args>}
// # Now replaced with <type> <varname>{<args>} to prevent unnecessary cctor calls
// #####
//    const std::shared_ptr<ClassTypeModel> &class_model = target->GetOwner();
//    std::string template_inst_str;
//    if (class_model->IsTemplatedClass()) {
//      assert(stmt_type.IsTemplateTypenameSpcType());
//      const std::shared_ptr<TemplateTypenameSpcType> &tt_spc_type =
//        std::static_pointer_cast<TemplateTypenameSpcType>(stmt_inner_type);
//      const TemplateTypeInstList &inst_list = tt_spc_type->GetInstList();
//      template_inst_str = inst_list.ToString();
//    }
//    const std::string &ctor_name_with_namespace = class_model->GetQualifiedName();
//    ss << ctor_name_with_namespace << template_inst_str;
  } else {
    std::string template_inst_str;
    if (target->IsTemplatedExecutable()) {
      const TemplateTypeInstList &inst_list = tt_ctx->GetMapping().LookupForExecutable(target);
      template_inst_str = inst_list.ToString();
    }
    const bpstd::optional<clang::QualType> &opt_ret_type = target->GetReturnType();
    if (target->IsConversionDecl() && opt_ret_type.has_value()) {
      const clang::QualType &ret_type = opt_ret_type.value();
      const TWMSpec &twm_spec = TWMSpec::ByClangType(ret_type, tt_ctx);
      const TypeWithModifier &twm = TypeWithModifier::FromSpec(twm_spec);
      const std::string &tgt_type = twm.ToString(tt_ctx);
      ss << "operator " << tgt_type;

    } else if (target->GetOwner() == nullptr) {
      const std::string &method_qname = target->GetQualifiedName();
      ss << method_qname << template_inst_str;

    } else {
      const std::string &method_name = target->GetName();
      ss << method_name << template_inst_str;
    }
  }

  const std::vector<clang::QualType> &arguments = target->GetArguments();
  const std::vector<Operand> &operands_ = stmt->GetOperands();
  assert(arguments.size() == operands_.size());

  // ZIP OPERATION
  std::vector<std::pair<Operand, TypeWithModifier>> operands_with_type_requirement;
  std::transform(
    operands_.begin(), operands_.end(), arguments.begin(),
    std::back_inserter(operands_with_type_requirement),
    [](const auto &op, const auto &rq) {
      const TWMSpec &twm_spec = TWMSpec::ByClangType(rq, nullptr);
      const TypeWithModifier &type_req = TypeWithModifier::FromSpec(twm_spec);
      return std::make_pair(op, type_req);
    });

  std::stringstream arg_ss;
  bool first_elmt = true;
  for (const auto &op_typerq : operands_with_type_requirement) {
    const Operand &operand = op_typerq.first;
    const TypeWithModifier &type_rq = op_typerq.second;
    const TypeWithModifier &tt_resolved_rq =
      type_rq.IsTemplateTypenameType() ? type_rq.ResolveTemplateType(tt_ctx) : type_rq;
    const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm = context_->GetInheritanceModel();
    arg_ss << (!first_elmt ? ", " : "") << operand.ToStringWithAutoCasting(tt_resolved_rq, tt_ctx, itm);
    first_elmt = false;
  }

  if (is_ctor_call)
    ss << "{" << arg_ss.str() << "}";
  else
    ss << "(" << arg_ss.str() << ")";

  stmt->SetVarName(var_name_opt);
  if (!is_void) {
    assert((is_void_ptr || !is_void) && var_name_opt.has_value());
  }
  return ss.str();
}
StatementWriter::StatementWriter(const std::shared_ptr<ProgramContext> &context) : context_(context) {}
std::string StatementWriter::ArrayInitStmtAsString(
  const std::shared_ptr<ArrayInitStatement> &stmt,
  unsigned int stmt_id
) {
  const TypeWithModifier &type = stmt->GetType();
  assert(type.IsArray()); // if not array, there's something wrong with your code!
  const TypeWithModifier &type_for_write = type.StripParticularModifiers({Modifier::kArray});
  const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm = context_->GetInheritanceModel();
  const bpstd::optional<int> &capacity = stmt->GetCapacity();
  const bpstd::optional<Operand> &string_literal = stmt->GetStringLiteral();
  const bpstd::optional<std::vector<Operand>> &elements = stmt->GetElements();

  if (string_literal.has_value()) {
    const Operand &str_operand = string_literal.value();
    const std::string &literal = str_operand.GetConstantLiteral().value();
    int arr_size = capacity.value_or(literal.size() + 1);

    std::stringstream var_name;
    var_name << type.GetDefaultVarName() << stmt_id;

    std::stringstream ss;
    ss << type_for_write.ToString() << " " << var_name.str() << '[' << arr_size << ']';
    ss << " = " << '"' << literal << '"';
    stmt->SetVarName(bpstd::make_optional(var_name.str()));
    return ss.str();

  } else if (elements.has_value()) {
    const std::vector<Operand> &elmt_ops = elements.value();
    int arr_size = capacity.value_or(elmt_ops.size() + 1);

    std::stringstream var_name;
    var_name << type.GetDefaultVarName() << stmt_id;

    std::stringstream ss;
    ss << type_for_write.ToString() << " " << var_name.str() << '[' << arr_size << ']' << ' ';
    if (!elmt_ops.empty()) {
      ss << '{';
      bool first_elmt = true;
      for (const auto &op : elmt_ops) {
        const std::string &op_string = op.ToStringWithAutoCasting(type, itm);
        ss << (!first_elmt ? ", " : "") << op_string;
        first_elmt = false;
      }
      ss << '}';
    }
    stmt->SetVarName(bpstd::make_optional(var_name.str()));
    return ss.str();

  } else {
    assert(capacity.has_value());
    int arr_size = capacity.value();

    std::stringstream var_name;
    var_name << type.GetDefaultVarName() << stmt_id;
    std::stringstream ss;
    ss << type_for_write.ToString() << " " << var_name.str() << '[' << arr_size << ']';
    stmt->SetVarName({var_name.str()});
    return ss.str();
  }
}

// ##########
// # STLStatementWriter
// #####
void STLStatementWriter::HandleStackAndQueue(
  const TemplateTypeInstList &inst_list,
  const STLElement &stl_elements,
  std::stringstream &ss
) {
  assert(stl_elements.IsRegContainerElements());
  const std::vector<TemplateTypeInstantiation> &instantiations = inst_list.GetInstantiations();
  assert(instantiations.size() == 1);
  const TypeWithModifier &rq_type = instantiations[0].GetType();

  const std::vector<Operand> &operands = stl_elements.GetRegContainerElmts();
  if (!operands.empty()) {
    std::stringstream arg_ss;
    bool first_elmt = true;
    for (const auto &operand : operands) {
      const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm = context_->GetInheritanceModel();
      arg_ss << (!first_elmt ? ", " : "") << operand.ToStringWithAutoCasting(rq_type, itm);
      first_elmt = false;
    }
    ss << "({" << arg_ss.str() << "})";
  }
}
void STLStatementWriter::HandlePriorityQueue(
  const TemplateTypeInstList &inst_list,
  const STLElement &stl_elements,
  unsigned int stmt_id,
  std::stringstream &ss,
  std::stringstream &prelim_ss
) {
  assert(stl_elements.IsRegContainerElements());
  const std::vector<TemplateTypeInstantiation> &instantiations = inst_list.GetInstantiations();
  assert(instantiations.size() == 1);
  const TypeWithModifier &rq_type = instantiations[0].GetType();

  const std::vector<Operand> &operands = stl_elements.GetRegContainerElmts();
  if (!operands.empty()) {
    // Create temporary vector
    const std::shared_ptr<TemplateTypenameSpcType> &vc_type =
      TemplateTypenameSpcType::From(STLType::kVector, inst_list);
    const TWMSpec &twm_spec = TWMSpec::ByType(vc_type, nullptr);
    const TypeWithModifier &vc_twm = TypeWithModifier::FromSpec(twm_spec);

    const std::shared_ptr<Statement> &vc_stmt =
      STLStatement::MakeSTLStatement(vc_twm, STLType::kVector, stl_elements);
    const std::shared_ptr<STLStatement> &vc_stl_stmt = std::static_pointer_cast<STLStatement>(vc_stmt);
    const std::string &vc_name = "__tvc" + std::to_string(stmt_id);

    const std::string &vc_as_string = STLStmtAsString(vc_stl_stmt, stmt_id, vc_name);
    prelim_ss << vc_as_string << "; ";

    ss << "(" << vc_name << ".begin(), " << vc_name << ".end())";
  }
}
void STLStatementWriter::HandleStandardRegContainer(
  const TemplateTypeInstList &inst_list,
  const STLElement &stl_elements,
  std::stringstream &ss
) {
  assert(stl_elements.IsRegContainerElements());
  const std::vector<TemplateTypeInstantiation> &instantiations = inst_list.GetInstantiations();
  assert(instantiations.size() == 1);
  const TypeWithModifier &rq_type = instantiations[0].GetType();

  const std::vector<Operand> &operands = stl_elements.GetRegContainerElmts();
  if (!operands.empty()) {
    std::stringstream arg_ss;
    bool first_elmt = true;
    for (const auto &operand : operands) {
      const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm = context_->GetInheritanceModel();
      arg_ss << (!first_elmt ? ", " : "") << operand.ToStringWithAutoCasting(rq_type, itm);
      first_elmt = false;
    }
    ss << '{' << arg_ss.str() << '}';
  }
}
void STLStatementWriter::HandleArray(
  const TemplateTypeInstList &inst_list,
  const STLElement &stl_elements,
  std::stringstream &ss
) {
  assert(stl_elements.IsRegContainerElements());
  const std::vector<TemplateTypeInstantiation> &instantiations = inst_list.GetInstantiations();
  assert(instantiations.size() == 2 && instantiations[1].GetVariant() == TemplateTypeInstVariant::kIntegral);
  const TypeWithModifier &rq_type = instantiations[0].GetType();

  const std::vector<Operand> &operands = stl_elements.GetRegContainerElmts();
  std::stringstream arg_ss;
  bool first_elmt = true;
  for (const auto &operand : operands) {
    const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm = context_->GetInheritanceModel();
    arg_ss << (!first_elmt ? ", " : "") << operand.ToStringWithAutoCasting(rq_type, itm);
    first_elmt = false;
  }
  ss << '{' << arg_ss.str() << '}';
}
void STLStatementWriter::HandleKeyValueContainer(
  const TemplateTypeInstList &inst_list,
  const STLElement &stl_elements,
  std::stringstream &ss
) {
  assert(stl_elements.IsKeyValueElements());
  const std::vector<TemplateTypeInstantiation> &instantiations = inst_list.GetInstantiations();
  assert(instantiations.size() == 2);
  const TypeWithModifier &rq_type_key = instantiations[0].GetType();
  const TypeWithModifier &rq_type_value = instantiations[1].GetType();

  const std::vector<std::pair<Operand, Operand>> &operands = stl_elements.GetKeyValueElmts();
  if (!operands.empty()) {
    std::stringstream arg_ss;
    bool first_elmt = true;
    for (const auto &operand_pair : operands) {
      const Operand &key = operand_pair.first;
      const Operand &value = operand_pair.second;
      const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm = context_->GetInheritanceModel();
      arg_ss << (!first_elmt ? ", " : "")
             << '{' << key.ToStringWithAutoCasting(rq_type_key, itm) << ','
             << value.ToStringWithAutoCasting(rq_type_value, itm) << '}';
      first_elmt = false;
    }
    ss << '{' << arg_ss.str() << '}';
  }
}
void STLStatementWriter::HandlePair(
  const TemplateTypeInstList &inst_list,
  const STLElement &stl_elements,
  std::stringstream &ss
) {
  assert(stl_elements.IsKeyValueElements());
  const std::vector<TemplateTypeInstantiation> &instantiations = inst_list.GetInstantiations();
  assert(instantiations.size() == 2);
  const std::vector<std::pair<Operand, Operand>> &operands = stl_elements.GetKeyValueElmts();
  assert(operands.size() == 1);

  std::stringstream arg_ss;
  const std::pair<Operand, Operand> &pair_operand = operands[0];
  const Operand &operand_fi = pair_operand.first;
  const TypeWithModifier &rq_type_fi = instantiations[0].GetType();
  const Operand &operand_sc = pair_operand.second;
  const TypeWithModifier &rq_type_sc = instantiations[1].GetType();
  const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm = context_->GetInheritanceModel();
  arg_ss << operand_fi.ToStringWithAutoCasting(rq_type_fi, itm)
         << ", " << operand_sc.ToStringWithAutoCasting(rq_type_sc, itm);

  ss << '{' << arg_ss.str() << '}';
}
void STLStatementWriter::HandleTuple(
  const TemplateTypeInstList &inst_list,
  const STLElement &stl_elements,
  std::stringstream &ss
) {
  assert(stl_elements.IsRegContainerElements());
  const std::vector<TemplateTypeInstantiation> &instantiations = inst_list.GetInstantiations();
  const std::vector<Operand> &operands = stl_elements.GetRegContainerElmts();
  assert(operands.size() == instantiations.size());
  int op_len = (int) operands.size();

  if (!operands.empty()) {
    std::stringstream arg_ss;
    bool first_elmt = true;
    for (int i = 0; i < op_len; i++) {
      const Operand &operand = operands[i];
      const TypeWithModifier &rq_type = instantiations[i].GetType();
      const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm = context_->GetInheritanceModel();
      arg_ss << (!first_elmt ? ", " : "") << operand.ToStringWithAutoCasting(rq_type, itm);
      first_elmt = false;
    }
    ss << '{' << arg_ss.str() << '}';
  }
}
void STLStatementWriter::HandleSmartPointer(
  const TemplateTypeInstList &inst_list,
  const STLElement &stl_elements,
  std::stringstream &ss
) {
  assert(stl_elements.IsRegContainerElements());
  const std::vector<TemplateTypeInstantiation> &instantiations = inst_list.GetInstantiations();
  assert(instantiations.size() == 1);
  const std::vector<Operand> &operands = stl_elements.GetRegContainerElmts();
  assert(operands.size() == instantiations.size());

  std::stringstream arg_ss;
  const Operand &operand = operands[0];
  const TypeWithModifier &rq_twm = instantiations[0].GetType();
  const TypeWithModifier &rq_twm_ptr = rq_twm.WithAdditionalModifiers({Modifier::kPointer});

  const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm = context_->GetInheritanceModel();
  arg_ss << operand.ToStringWithAutoCasting(rq_twm_ptr, itm);
  ss << '(' << arg_ss.str() << ')';
}
void STLStatementWriter::HandleString(
  const TemplateTypeInstList &inst_list,
  const STLElement &stl_elements,
  std::stringstream &ss
) {
  assert(stl_elements.IsRegContainerElements());
  const std::vector<TemplateTypeInstantiation> &instantiations = inst_list.GetInstantiations();
  assert(instantiations.size() == 1);
  const TypeWithModifier &rq_type = instantiations[0].GetType();

  const std::vector<Operand> &operands = stl_elements.GetRegContainerElmts();
  if (!operands.empty()) {
    std::stringstream arg_ss;
    bool first_elmt = true;
    for (const auto &operand : operands) {
      const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm = context_->GetInheritanceModel();
      arg_ss << (!first_elmt ? ", " : "") << operand.ToStringWithAutoCasting(rq_type, itm);
      first_elmt = false;
    }
    ss << '{' << arg_ss.str() << '}';
  }
}
std::string STLStatementWriter::STLStmtAsString(
  const std::shared_ptr<STLStatement> &stmt,
  unsigned int stmt_id,
  const std::string &force_varname
) {
  const TypeWithModifier &stmt_twm = stmt->GetType();
  assert(stmt_twm.IsTemplateTypenameSpcType());

  const std::shared_ptr<Type> &stmt_inner_type = stmt_twm.GetType();
  const std::shared_ptr<TemplateTypenameSpcType> &tt_spc_type =
    std::static_pointer_cast<TemplateTypenameSpcType>(stmt_inner_type);
  const std::shared_ptr<Type> &target_type = tt_spc_type->GetTargetType();
  const TemplateTypeInstList &inst_list = tt_spc_type->GetInstList();
  assert(target_type->GetVariant() == TypeVariant::kSTL);

  const std::shared_ptr<STLType> &stl_type = std::static_pointer_cast<STLType>(target_type);
  const STLElement &stl_elements = stmt->GetElements();

  std::stringstream ss;
  std::stringstream prelim_ss;
  std::string var_name = force_varname.empty() ? stmt_twm.GetDefaultVarName() + std::to_string(stmt_id) : force_varname;
  const bpstd::optional<std::string> &var_name_opt = bpstd::make_optional(var_name);
  ss << stmt_twm.ToString() << ' ' << var_name;

  STLTypeVariant stl_variant = stl_type->GetSTLTypeVariant();
  switch (stl_variant) {
    case STLTypeVariant::kRegContainer: {
      if (stl_type == STLType::kStack || stl_type == STLType::kQueue) {
        HandleStackAndQueue(inst_list, stl_elements, ss);
      } else if (stl_type == STLType::kPriorityQueue) {
        HandlePriorityQueue(inst_list, stl_elements, stmt_id, ss, prelim_ss);
      } else {
        HandleStandardRegContainer(inst_list, stl_elements, ss);
      }
      break;
    }
    case STLTypeVariant::kRegContainerWithSize: {
      HandleArray(inst_list, stl_elements, ss);
      break;
    }
    case STLTypeVariant::kKeyValueContainer: {
      HandleKeyValueContainer(inst_list, stl_elements, ss);
      break;
    }
    case STLTypeVariant::kPair: {
      HandlePair(inst_list, stl_elements, ss);
      break;
    }
    case STLTypeVariant::kTuple: {
      HandleTuple(inst_list, stl_elements, ss);
      break;
    }
    case STLTypeVariant::kSmartPointer: {
      HandleSmartPointer(inst_list, stl_elements, ss);
      break;
    }
    case STLTypeVariant::kString: {
      HandleString(inst_list, stl_elements, ss);
      break;
    }
  }

  stmt->SetVarName(var_name_opt);
  assert(var_name_opt.has_value());
  return prelim_ss.str() + ss.str();
}
STLStatementWriter::STLStatementWriter(const std::shared_ptr<ProgramContext> &context) : context_(context) {}

// ##########
// # STLStatement
// #####

STLStatement::STLStatement(
  const TypeWithModifier &type,
  std::shared_ptr<STLType> target,
  STLElement elements
) : Statement(type), target_(std::move(target)), elements_(std::move(elements)) {}
std::shared_ptr<Statement> STLStatement::Clone() {
  return std::make_shared<STLStatement>(
    GetType(),
    target_,
    elements_
  );
}
std::shared_ptr<Statement> STLStatement::MakeSTLStatement(
  const TypeWithModifier &type,
  std::shared_ptr<STLType> target,
  STLElement elements
) {
  return std::make_shared<STLStatement>(
    type,
    std::move(target),
    std::move(elements)
  );
}
StatementVariant STLStatement::GetVariant() const {
  return StatementVariant::kSTLConstruction;
}
std::pair<std::shared_ptr<Statement>, int> STLStatement::ReplaceRefOperand(
  const std::map<std::shared_ptr<Statement>, std::shared_ptr<Statement>> &repl_map,
  const std::shared_ptr<TemplateTypeContext> &tt_ctx
) {
  const std::shared_ptr<STLStatement> &cloned = std::static_pointer_cast<STLStatement>(Clone());
  int replacement = 0;
  bpstd::optional<std::vector<Operand>> &reg_container_elmts = cloned->elements_.reg_container_elmts_;
  if (reg_container_elmts.has_value()) {
    std::vector<Operand> &operands = reg_container_elmts.value();
    for (auto &operand : operands) {
      if (TryReplaceRefOperand(operand, repl_map, tt_ctx))
        ++replacement;
    }
  }
  bpstd::optional<std::vector<std::pair<Operand, Operand>>> &kv_elmts = cloned->elements_.key_value_elmts_;
  if (kv_elmts.has_value()) {
    std::vector<std::pair<Operand, Operand>> &operands = kv_elmts.value();
    for (auto &operand_pair : operands) {
      Operand &fi = operand_pair.first;
      if (TryReplaceRefOperand(fi, repl_map, tt_ctx))
        ++replacement;
      Operand &sc = operand_pair.second;
      if (TryReplaceRefOperand(sc, repl_map, tt_ctx))
        ++replacement;
    }
  }
  return {cloned, replacement};
}
std::vector<Operand> STLStatement::GetStatementOperands() const {
  std::vector<Operand> result;
  if (elements_.IsRegContainerElements()) {
    const std::vector<Operand> &operands = elements_.GetRegContainerElmts();
    for (const auto &operand : operands) {
      result.push_back(operand);
    }
  }
  if (elements_.IsKeyValueElements()) {
    const std::vector<std::pair<Operand, Operand>> &operands = elements_.GetKeyValueElmts();
    for (auto &operand_pair : operands) {
      const Operand &fi = operand_pair.first;
      result.push_back(fi);
      const Operand &sc = operand_pair.second;
      result.push_back(sc);
    }
  }
  return result;
}
const std::shared_ptr<STLType> &STLStatement::GetTarget() const {
  return target_;
}
const STLElement &STLStatement::GetElements() const {
  return elements_;
}
void STLStatement::SetElements(const STLElement &elements) {
  elements_ = elements;
}

// ##########
// # STLElement
// #####

STLElement::STLElement(
  bpstd::optional<std::vector<Operand>> reg_container_elmts,
  bpstd::optional<std::vector<std::pair<Operand, Operand>>> key_value_elmts
) : reg_container_elmts_(std::move(reg_container_elmts)), key_value_elmts_(std::move(key_value_elmts)) {}
const std::vector<Operand> &STLElement::GetRegContainerElmts() const {
  assert(reg_container_elmts_.has_value());
  return reg_container_elmts_.value();
}
const std::vector<std::pair<Operand, Operand>> &STLElement::GetKeyValueElmts() const {
  assert(key_value_elmts_.has_value());
  return key_value_elmts_.value();
}
STLElement STLElement::ForRegularContainer(const std::vector<Operand> &operands) {
  return STLElement(bpstd::make_optional(operands), bpstd::nullopt);
}
STLElement STLElement::ForKeyValueContainer(const std::vector<std::pair<Operand, Operand>> &operands) {
  return STLElement(bpstd::nullopt, bpstd::make_optional(operands));
}
bool STLElement::IsKeyValueElements() const {
  return key_value_elmts_.has_value();
}
bool STLElement::IsRegContainerElements() const {
  return reg_container_elmts_.has_value();
}

// ##########
// # ArrayInitStatement
// #####

std::shared_ptr<Statement> ArrayInitStatement::Clone() {
  return std::make_shared<ArrayInitStatement>(
    GetType(),
    capacity_,
    string_literal_,
    elements_
  );
}
StatementVariant ArrayInitStatement::GetVariant() const {
  return StatementVariant::kArrayInitialization;
}
std::pair<std::shared_ptr<Statement>, int> ArrayInitStatement::ReplaceRefOperand(
  const std::map<std::shared_ptr<Statement>, std::shared_ptr<Statement>> &repl_map,
  const std::shared_ptr<TemplateTypeContext> &tt_ctx
) {
  const std::shared_ptr<ArrayInitStatement> &cloned = std::static_pointer_cast<ArrayInitStatement>(Clone());
  if (string_literal_.has_value()) {
    return {cloned, 0};
  } else {
    int replacement = 0;
    if (elements_.has_value()) {
      std::vector<Operand> &operands = elements_.value();
      for (auto &operand : operands) {
        if (TryReplaceRefOperand(operand, repl_map, tt_ctx))
          ++replacement;
      }
    }
    return {cloned, replacement};
  }
}
std::vector<Operand> ArrayInitStatement::GetStatementOperands() const {
  std::vector<Operand> result;
  if (string_literal_.has_value()) {
    result.push_back(string_literal_.value());
  }
  if (elements_.has_value()) {
    const std::vector<Operand> &operands = elements_.value();
    for (const auto &operand : operands) {
      result.push_back(operand);
    }
  }
  return result;
}
ArrayInitStatement::ArrayInitStatement(
  const TypeWithModifier &type,
  const bpstd::optional<int> &capacity,
  bpstd::optional<Operand> string_literal,
  bpstd::optional<std::vector<Operand>> elements
) : Statement(type), capacity_(capacity), string_literal_(std::move(string_literal)), elements_(std::move(elements)) {}
const bpstd::optional<int> &ArrayInitStatement::GetCapacity() const {
  return capacity_;
}
void ArrayInitStatement::SetCapacity(const bpstd::optional<int> &capacity) {
  capacity_ = capacity;
}
const bpstd::optional<Operand> &ArrayInitStatement::GetStringLiteral() const {
  return string_literal_;
}
void ArrayInitStatement::SetStringLiteral(const bpstd::optional<Operand> &string_literal) {
  string_literal_ = string_literal;
}
const bpstd::optional<std::vector<Operand>> &ArrayInitStatement::GetElements() const {
  return elements_;
}
void ArrayInitStatement::SetElements(const bpstd::optional<std::vector<Operand>> &elements) {
  elements_ = elements;
}
std::shared_ptr<Statement> ArrayInitStatement::MakeCString(const Operand &op) {
  TWMSpec twm_spec = TWMSpec::ByType(PrimitiveType::kCharacter, nullptr);
  twm_spec.SetAdditionalMods({Modifier::kArray});
  const TypeWithModifier &twm = TypeWithModifier::FromSpec(twm_spec);
  const bpstd::optional<Operand> &string_literal = bpstd::make_optional(op);
  bool is_unsigned = op.GetType().IsUnsigned();
  return std::make_shared<ArrayInitStatement>(
    is_unsigned ? twm.WithAdditionalModifiers({Modifier::kUnsigned}) : twm,
    bpstd::nullopt,
    string_literal,
    bpstd::nullopt
  );
}
std::shared_ptr<Statement> ArrayInitStatement::MakeArrayInitialization(
  const TypeWithModifier &target_type,
  const std::vector<Operand> &operands
) {
  for (const auto &item : operands) {
    const TypeWithModifier &twm = item.GetType();
    assert(target_type.IsAssignableFrom(twm, nullptr, nullptr));
  }
  const TypeWithModifier &type_arr = target_type.WithAdditionalModifiers({Modifier::kArray});
  int capacity = (int) operands.size();
  return std::make_shared<ArrayInitStatement>(
    type_arr,
    bpstd::nullopt,
    bpstd::nullopt,
    bpstd::make_optional(operands)
  );

}

// ##########
// # LibFuzzerModeHacker
// #####

Operand::LibFuzzerModeHacker::LibFuzzerModeHacker() {
  Operand::kLibFuzzerMode = true;
}
Operand::LibFuzzerModeHacker::~LibFuzzerModeHacker() {
  Operand::kLibFuzzerMode = false;
}
} // namespace cxxfoozz