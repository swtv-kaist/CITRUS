#include "logger.hpp"
#include "mutator.hpp"

#include <utility>
#include "function-selector.hpp"
#include "random.hpp"
#include "writer.hpp"
#include "type.hpp"

namespace cxxfoozz {

// ##########
// # OperandMutator
// #####

Operand OperandMutator::MutateConstantOperand(const Operand &op) {
  assert(op.GetOperandType() == OperandType::kConstantOperand);
  if (op.IsNullPtr()) {
    return op;
  }
  const TypeWithModifier &type = op.GetType();
  return MutateConstantOperand(type);
}
Operand OperandMutator::MutateConstantOperand(const TypeWithModifier &type) {
  const std::shared_ptr<Type> &type_ptr = type.GetType();

  const std::shared_ptr<Random> &r = Random::GetInstance();
  bool is_enum = type_ptr->GetVariant() == TypeVariant::kEnum;
  if (type.IsPrimitiveType() && !is_enum) {
    const std::multiset<Modifier> &modifiers = type.GetModifiers();
    bool is_unsigned = modifiers.count(Modifier::kUnsigned);
    bool is_ptr_or_array = type.IsPointerOrArray();

    const std::shared_ptr<PrimitiveType> &primitive_type = std::static_pointer_cast<PrimitiveType>(type_ptr);
    PrimitiveTypeVariant primitive_type_variant = primitive_type->GetPrimitiveTypeVariant();
    switch (primitive_type_variant) {
      case PrimitiveTypeVariant::kVoid:
      case PrimitiveTypeVariant::kNullptrType: {
        return Operand::MakeConstantOperand(type.WithAdditionalModifiers({Modifier::kPointer}), "nullptr");
      }
      case PrimitiveTypeVariant::kBoolean: {
        bool next = r->NextBoolean();
        return Operand::MakeConstantOperand(type, next ? "true" : "false");
      }
      case PrimitiveTypeVariant::kShort: {
        const std::string &next = is_unsigned ? r->NextIntGen<unsigned short>() : r->NextIntGen<short>();
        return Operand::MakeConstantOperand(type, next);
      }
      case PrimitiveTypeVariant::kCharacter: {
        if (is_ptr_or_array) {
          const std::string &next_string = r->NextString();
          return Operand::MakeConstantOperand(type, next_string);
        } else {
          const std::string &next = is_unsigned ? r->NextIntGen<unsigned char>() : r->NextIntGen<signed char>();
          return Operand::MakeConstantOperand(type, next);
        }
      }
      case PrimitiveTypeVariant::kInteger: {
        const std::string &next = is_unsigned ? r->NextIntGen<unsigned int>() : r->NextIntGen<int>();
        return Operand::MakeConstantOperand(type, next);
      }
      case PrimitiveTypeVariant::kLong: {
        const std::string &next = is_unsigned ? r->NextIntGen<unsigned long>() : r->NextIntGen<long>();
        return Operand::MakeConstantOperand(type, next);
      }
      case PrimitiveTypeVariant::kLongLong: {
        const std::string &next = is_unsigned ? r->NextIntGen<unsigned long long>() : r->NextIntGen<long long>();
        return Operand::MakeConstantOperand(type, next);
      }
      case PrimitiveTypeVariant::kFloat: {
        const std::string &next = r->NextRealGen<float>();
        return Operand::MakeConstantOperand(type, next);
      }
      case PrimitiveTypeVariant::kDouble: {
        const std::string &next = r->NextRealGen<double>();
        return Operand::MakeConstantOperand(type, next);
      }
      case PrimitiveTypeVariant::kWideCharacter: {
        const std::string &next = r->NextIntGen<wchar_t>();
        return Operand::MakeConstantOperand(type, next);
      }
    }
  } else if (is_enum) {
    const std::shared_ptr<EnumType> &enum_type = std::static_pointer_cast<EnumType>(type_ptr);
    const std::shared_ptr<EnumTypeModel> &enum_tm = enum_type->GetModel();
    const std::string &enum_name = enum_tm->GetQualifiedName();
    const std::vector<std::string> &enum_variants = enum_tm->GetVariants();
    int length = (int) enum_variants.size();

    int choice = r->NextInt(length);
    const std::string &target_variant = enum_variants[choice];
    const std::string &combined = enum_name + "::" + target_variant;
    return Operand::MakeConstantOperand(type, combined);
  }
  assert(false);
  return Operand::MakeBottom();
}
Operand OperandMutator::MutateRefOperand(
  const Operand &op,
  const std::shared_ptr<Statement> &op_stmt_ctx,
  const TestCase &tc_ctx,
  const bpstd::optional<TypeWithModifier> &type_rq
) {
  assert(op.GetOperandType() == OperandType::kRefOperand);
  const TypeWithModifier &target_type = type_rq.value_or(op.GetType());

  OperandResolver op_resolver{context_};
  const std::vector<std::shared_ptr<Statement>> &assignable_stmts =
    op_resolver.GetAssignableStatements(target_type, op_stmt_ctx, tc_ctx);

  unsigned long max_choice = assignable_stmts.size();
  if (max_choice <= 1)
    return op;

  const std::shared_ptr<Random> &r = Random::GetInstance();
  int choice = r->NextInt((int) max_choice);
  const std::shared_ptr<Statement> &next = assignable_stmts[choice];

  const std::shared_ptr<TemplateTypeContext> &tt_ctx = tc_ctx.GetTemplateTypeContext();
  const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm = context_->GetInheritanceModel();
  assert(target_type.IsAssignableFrom(next->GetType(), tt_ctx, itm));

  return Operand::MakeRefOperand(next);
}
Operand OperandMutator::MutateOperand(
  const Operand &op,
  const std::shared_ptr<Statement> &op_stmt_ctx,
  const TestCase &tc_ctx,
  const bpstd::optional<TypeWithModifier> &type_rq
) {
//  std::shared_ptr<TemplateTypeContext> tt_ctx;
//  if (op_stmt_ctx->GetVariant() == StatementVariant::kCall) {
//    const std::shared_ptr<CallStatement> &call_stmt = std::static_pointer_cast<CallStatement>(op_stmt_ctx);
//    tt_ctx = call_stmt->GetTemplateTypeContext();
//  } else {
//    tt_ctx = tc_ctx.GetTemplateTypeContext();
//  }
  const std::shared_ptr<InheritanceTreeModel> &itm = context_->GetInheritanceModel();
  Operand result = Operand::MakeBottom();
//  if (type_rq.has_value()) {
//    assert(type_rq->IsAssignableFrom(op.GetType(), tt_ctx, itm));
//  }

  const std::shared_ptr<Random> &r = Random::GetInstance();
  int choice = r->NextInt(2);
  if (choice == 0) { // Mutate accordingly
    bool is_constant = op.GetOperandType() == OperandType::kConstantOperand;
    if (is_constant) {
      result = MutateConstantOperand(op);
    } else {
      result = MutateRefOperand(op, op_stmt_ctx, tc_ctx, type_rq);
    }

  } else { // Mutate crosstype (ref <-> constant)
    result = MutateOperandCrossVariant(op, op_stmt_ctx, tc_ctx, type_rq);
  }
  const TypeWithModifier &res_twm = result.GetType();
//  if (type_rq.has_value()) {
//    assert(type_rq->IsAssignableFrom(res_twm, tt_ctx, itm));
//  }
  return result;
}

bool IsRefPtrAndUsedAsInvokingObject(const Operand &op, const std::shared_ptr<Statement> &stmt) {
  bool is_ref = op.GetOperandType() == OperandType::kRefOperand;
  bool is_ptr = op.GetType().IsPointer();
  bool is_call = stmt->GetVariant() == StatementVariant::kCall;
  if (is_call && is_ref && is_ptr) {
    const std::shared_ptr<CallStatement> &call_stmt = std::static_pointer_cast<CallStatement>(stmt);
    const bpstd::optional<Operand> &opt_inv_obj = call_stmt->GetInvokingObj();
    bool has_inv_obj = opt_inv_obj.has_value();
    if (has_inv_obj) {
      const Operand &inv_operand = opt_inv_obj.value();
      return std::addressof(op) == std::addressof(inv_operand);
//      bool is_inv_ref = inv_operand.GetOperandType() == OperandType::kRefOperand;
//      if (is_inv_ref) {
//        return op.GetRef() == inv_operand.GetRef();
//      }
    }
  }
  return false;
}

Operand OperandMutator::MutateOperandCrossVariant(
  const Operand &op,
  const std::shared_ptr<Statement> &op_stmt_ctx,
  const TestCase &tc_ctx,
  const bpstd::optional<TypeWithModifier> &type_rq
) {
  if (op.GetOperandType() == OperandType::kConstantOperand) { // constant -> ref
    const std::shared_ptr<TemplateTypeContext> &tt_ctx = tc_ctx.GetTemplateTypeContext();
    const std::vector<std::shared_ptr<Statement>> &stmts = tc_ctx.GetStatements();
    const std::vector<std::shared_ptr<Statement>>::const_iterator &op_stmt_ctx_it =
      std::find(stmts.begin(), stmts.end(), op_stmt_ctx);
    assert(op_stmt_ctx_it != stmts.end());

    const TypeWithModifier &target_type = op.GetType();
    std::vector<std::shared_ptr<Statement>> assignable_stmts;
    std::copy_if(
      stmts.begin(), op_stmt_ctx_it, std::back_inserter(assignable_stmts), [&](const auto &i) {
        const TypeWithModifier &stmt_type = i->GetType();
        const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm = context_->GetInheritanceModel();
        return target_type.IsAssignableFrom(stmt_type, tt_ctx, itm);
      });

    if (assignable_stmts.empty())
      return MutateConstantOperand(op);

    const std::shared_ptr<Random> &r = Random::GetInstance();
    int idx = r->NextInt((int) assignable_stmts.size());
    std::shared_ptr<Statement> &next_stmt = assignable_stmts[idx];
    return Operand::MakeRefOperand(next_stmt);

  } else { // ref -> constant
    const TypeWithModifier &target_type = op.GetType();
    if (target_type.IsPrimitiveType() && !type_rq.has_value()) { // requirement = nullopt -> can mutate freely
      return MutateConstantOperand(target_type);
    } else if (target_type.IsPrimitiveType()) {
      bool is_ptr_or_array = type_rq->IsPointerOrArray();
      if (!is_ptr_or_array)
        return MutateConstantOperand(target_type);
    } else if (target_type.IsPointer()) {
      static const double kNullptrProb = 0.1;
      const std::shared_ptr<Random> &r = Random::GetInstance();
      double prob = r->NextGaussian();
      bool is_mutating_inv_obj = IsRefPtrAndUsedAsInvokingObject(op, op_stmt_ctx);
      if (prob < kNullptrProb && !is_mutating_inv_obj) { // inv_obj cannot be nullptr
        return Operand::MakeConstantOperand(target_type, "nullptr");
      }
    }
    // No way to construct constant operand from non-primitive (class) types.
    return op;
  }
}
OperandMutator::OperandMutator(const std::shared_ptr<ProgramContext> &context) : context_(context) {}

// ##########
// # StatementMutator
// #####

StatementMutator::StatementMutator(
  std::shared_ptr<ClassType> cut,
  std::shared_ptr<ProgramContext> context
) : cut_(std::move(cut)), context_(std::move(context)) {}
const std::shared_ptr<ClassType> &StatementMutator::GetCut() const {
  return cut_;
}
const std::shared_ptr<ProgramContext> &StatementMutator::GetContext() const {
  return context_;
}

std::array<GeneralPrimitiveOp, 4> kBinaryOperators{
  GeneralPrimitiveOp::kAdd,
  GeneralPrimitiveOp::kSub,
  GeneralPrimitiveOp::kMul,
//  GeneralPrimitiveOp::kDiv, // (TRL-12): may cause division by 0, prevented for now
  GeneralPrimitiveOp::kMod,
};

std::array<GeneralPrimitiveOp, 2> kUnaryOperators{
  GeneralPrimitiveOp::kNop,
  GeneralPrimitiveOp::kMinus,
};

std::shared_ptr<Statement> StatementMutator::MutatePrimitiveAssignment(
  const std::shared_ptr<Statement> &stmt,
  const TestCase &tc_ctx
) {
  assert(stmt->GetVariant() == StatementVariant::kPrimitiveAssignment);
  const std::shared_ptr<Statement> &cloned = stmt->Clone();
  const std::shared_ptr<PrimitiveAssignmentStatement> &prim_ass_stmt =
    std::static_pointer_cast<PrimitiveAssignmentStatement>(cloned);

  const std::shared_ptr<Random> &r = Random::GetInstance();
  int choice = r->NextInt(2);

  const TypeWithModifier &stmt_twm = prim_ass_stmt->GetType();
  bool is_enum = stmt_twm.IsEnumType();
  if (is_enum)
    choice = 1;

  if (choice == 0) { // mutate operator
    OpArity arity = prim_ass_stmt->GetOpArity();
    if (arity == OpArity::kUnary) {
      int idx = r->NextInt(kUnaryOperators.size());
      const auto newOp = kUnaryOperators[idx];
      prim_ass_stmt->SetOp(newOp);
    } else {
      int idx = r->NextInt(kBinaryOperators.size());
      const auto newOp = kBinaryOperators[idx];
      prim_ass_stmt->SetOp(newOp);
    }

  } else { // mutate operand
    std::vector<Operand> &operands = prim_ass_stmt->GetOperands();
    int idx = r->NextInt((int) operands.size());
    const Operand &operand = operands[idx];

    OperandMutator mutator{context_};
    const Operand &mutated = mutator.MutateOperand(operand, stmt, tc_ctx);
    operands[idx] = mutated;
  }

  return prim_ass_stmt;
}

bool IsMethodSameSignature(const std::shared_ptr<Executable> &m1, const std::shared_ptr<Executable> &m2) {
  const std::shared_ptr<ClassTypeModel> &owner1 = m1->GetOwner();
  const std::shared_ptr<ClassTypeModel> &owner2 = m2->GetOwner();
  const std::vector<clang::QualType> &arg1 = m1->GetArguments();
  const std::vector<clang::QualType> &arg2 = m2->GetArguments();

  bool is_ctor1 = m1->GetExecutableVariant() == ExecutableVariant::kConstructor;
  bool is_ctor2 = m2->GetExecutableVariant() == ExecutableVariant::kConstructor;
  if (is_ctor1 && is_ctor2)
    return false;

  unsigned long arglen = arg1.size();
  if (arglen != arg2.size())
    return false;

  for (int i = 0; i < arglen; i++) {
    if (arg1[i] != arg2[i])
      return false;
  }

  if (owner1 == nullptr && owner2 == nullptr) { // Global functions
    return true; // no additional checking
  } else if (owner1 == owner2) { // Member function, same class
    return m1->IsNotRequireInvokingObj() == m2->IsNotRequireInvokingObj();
  } else { // Member function, different class
    return m1->IsNotRequireInvokingObj() && m2->IsNotRequireInvokingObj();
  }
}
std::shared_ptr<Statement> StatementMutator::MutateCall(
  const std::shared_ptr<Statement> &stmt,
  const TestCase &tc_ctx
) {
  assert(stmt->GetVariant() == StatementVariant::kCall);
  const std::shared_ptr<Statement> &cloned = stmt->Clone();
  const std::shared_ptr<CallStatement> &call_stmt = std::static_pointer_cast<CallStatement>(cloned);
  const std::shared_ptr<Executable> &target = call_stmt->GetTarget();
  const std::vector<clang::QualType> &args = target->GetArguments();

  const std::shared_ptr<Random> &r = Random::GetInstance();
  int choice = r->NextInt(2);
  if (choice == 0) { // mutate operands
    std::vector<Operand> &operands = call_stmt->GetOperands();
    const unsigned long op_size = operands.size();
    if (op_size > 0) {
      int idx = r->NextInt((int) op_size);
      const Operand &operand = operands[idx];
      const TWMSpec &twm_spec = TWMSpec::ByClangType(args[idx], tc_ctx.GetTemplateTypeContext());
      const TypeWithModifier &type_rq = TypeWithModifier::FromSpec(twm_spec);

      OperandMutator mutator{context_};
      const Operand &mutated = mutator.MutateOperand(operand, stmt, tc_ctx, bpstd::make_optional(type_rq));
      operands[idx] = mutated;
    }

  } else if (choice == 1) { // mutate invoking_obj_, if not static
    const bpstd::optional<Operand> &opt_invoking_obj = call_stmt->GetInvokingObj();
    if (opt_invoking_obj.has_value()) {
      const Operand &curr_operand = opt_invoking_obj.value();

      OperandMutator mutator{context_};
      const Operand &mutated = mutator.MutateOperand(curr_operand, stmt, tc_ctx, {opt_invoking_obj->GetType()});
      call_stmt->SetInvokingObj(bpstd::make_optional(mutated));
    }

  } else { // mutate target with same signature
    const std::vector<std::shared_ptr<Executable>> &executables = context_->GetExecutables();
    std::vector<std::shared_ptr<Executable>> morphing_forms;
    std::copy_if(
      executables.begin(), executables.end(), std::back_inserter(morphing_forms), [target](const auto &i) {
        return IsMethodSameSignature(target, i);
      });
    unsigned long morph_len = morphing_forms.size();
    if (morph_len > 0) {
      FunctionSelector function_selector{morphing_forms, FunctionSelectorMode::kComplexityBased};
      const std::shared_ptr<Executable> &selected_form = function_selector.NextExecutable();
      call_stmt->SetTarget(selected_form);
    }
  }
  return call_stmt;
}

std::shared_ptr<Statement> StatementMutator::MutateSTLConstruction(
  const std::shared_ptr<Statement> &stmt,
  const TestCase &tc_ctx
) {
  assert(stmt->GetVariant() == StatementVariant::kSTLConstruction);
  const std::shared_ptr<Statement> &cloned = stmt->Clone();
  const std::shared_ptr<STLStatement> &stl_stmt = std::static_pointer_cast<STLStatement>(cloned);

  const TypeWithModifier &stmt_twm = stl_stmt->GetType();
  const std::shared_ptr<Type> &stmt_type = stmt_twm.GetType();
  assert(stmt_type->GetVariant() == TypeVariant::kTemplateTypenameSpc);

  const std::shared_ptr<TemplateTypenameSpcType> &tt_spc_type =
    std::static_pointer_cast<TemplateTypenameSpcType>(stmt_type);
  const TemplateTypeInstList &inst_list = tt_spc_type->GetInstList();
  const std::vector<TemplateTypeInstantiation> &tt_inst_list = inst_list.GetInstantiations();
  assert(!tt_inst_list.empty());

  const std::shared_ptr<Random> &r = Random::GetInstance();
  const STLElement &elmts = stl_stmt->GetElements();
  if (elmts.IsRegContainerElements()) {
    const std::vector<Operand> &operands = elmts.GetRegContainerElmts();
    if (operands.empty())
      return stmt;

    int choice_idx = r->NextInt((int) operands.size());
    const Operand &tgt_operand = operands[choice_idx];

    const TemplateTypeInstantiation &inst_type = tt_inst_list[0];
    assert(inst_type.IsType());
    const TypeWithModifier &type_rq = inst_type.GetType();

    OperandMutator mutator{context_};
    const Operand &mutated = mutator.MutateOperand(tgt_operand, stmt, tc_ctx, bpstd::make_optional(type_rq));
    std::vector<Operand> operands_cp(operands);
    operands_cp[choice_idx] = mutated;

    const STLElement &mutated_elmts = STLElement::ForRegularContainer(operands_cp);
    stl_stmt->SetElements(mutated_elmts);
    return stl_stmt;

  } else {
    assert(elmts.IsKeyValueElements());
    assert(tt_inst_list.size() >= 2);

    const std::vector<std::pair<Operand, Operand>> &kv_operands = elmts.GetKeyValueElmts();
    if (kv_operands.empty())
      return stmt;

    int choice_idx = r->NextInt((int) kv_operands.size());
    int kv_idx = r->NextInt(2);
    std::pair<Operand, Operand> kv_mutable = kv_operands[choice_idx];

    Operand &tgt_operand = kv_idx == 0 ? kv_mutable.first : kv_mutable.second;
    const TemplateTypeInstantiation &inst_type = tt_inst_list[kv_idx];
    assert(inst_type.IsType());
    const TypeWithModifier &type_rq = inst_type.GetType();

    OperandMutator mutator{context_};
    const Operand &mutated_op = mutator.MutateOperand(tgt_operand, stmt, tc_ctx, bpstd::make_optional(type_rq));
    tgt_operand = mutated_op;

    std::vector<std::pair<Operand, Operand>> operands_cp(kv_operands);
    operands_cp[choice_idx] = kv_mutable;

    const STLElement &mutated_elmts = STLElement::ForKeyValueContainer(operands_cp);
    stl_stmt->SetElements(mutated_elmts);
    return stl_stmt;
  }
}

std::shared_ptr<Statement> StatementMutator::MutateArrayInit(
  const std::shared_ptr<Statement> &stmt,
  const TestCase &tc_ctx
) {
  assert(stmt->GetVariant() == StatementVariant::kArrayInitialization);
  const std::shared_ptr<Statement> &cloned = stmt->Clone();
  const std::shared_ptr<ArrayInitStatement> &arr_stmt = std::static_pointer_cast<ArrayInitStatement>(cloned);
  const TypeWithModifier &type_rq = arr_stmt->GetType();

  const std::shared_ptr<Random> &r = Random::GetInstance();
  const bpstd::optional<Operand> &string_literal = arr_stmt->GetStringLiteral();
  if (string_literal.has_value()) {
    const std::string &next_string = r->NextString();
    const TypeWithModifier &const_char_twm = type_rq.WithAdditionalModifiers({Modifier::kConst, Modifier::kPointer});
    const Operand &operand = Operand::MakeConstantOperand(const_char_twm, next_string);
    arr_stmt->SetStringLiteral({operand});

  } else {
    const bpstd::optional<std::vector<Operand>> &elements = arr_stmt->GetElements();
    assert(elements.has_value());
    const std::vector<Operand> &operands = elements.value();

    if (operands.empty())
      return stmt;

    int choice_idx = r->NextInt((int) operands.size());
    const Operand &tgt_operand = operands[choice_idx];

    OperandMutator mutator{context_};
    const Operand &mutated = mutator.MutateOperand(tgt_operand, stmt, tc_ctx, bpstd::make_optional(type_rq));
    std::vector<Operand> operands_cp(operands);
    operands_cp[choice_idx] = mutated;
    arr_stmt->SetElements({operands_cp});
  }
  return arr_stmt;
}

std::shared_ptr<Statement> StatementMutator::MutateStatement(
  const std::shared_ptr<Statement> &stmt,
  const TestCase &tc_ctx
) {
  StatementVariant variant = stmt->GetVariant();
  switch (variant) {
    case StatementVariant::kPrimitiveAssignment:
      return MutatePrimitiveAssignment(stmt, tc_ctx);
    case StatementVariant::kCall:
      return MutateCall(stmt, tc_ctx);
    case StatementVariant::kSTLConstruction:
      return MutateSTLConstruction(stmt, tc_ctx);
    case StatementVariant::kArrayInitialization:
      return MutateArrayInit(stmt, tc_ctx);
  }
}

// ##########
// # TestCaseMutator
// #####

TestCaseMutator::TestCaseMutator(std::shared_ptr<ClassType> cut, std::shared_ptr<ProgramContext> context)
  : cut_(std::move(cut)), context_(std::move(context)) {}
const std::shared_ptr<ClassType> &TestCaseMutator::GetCut() const {
  return cut_;
}
const std::shared_ptr<ProgramContext> &TestCaseMutator::GetContext() const {
  return context_;
}
TestCase TestCaseMutator::MutateTestCase(const TestCase &tc, int maxHavoc) {
  const std::vector<std::shared_ptr<Statement>> &statements = tc.GetStatements();
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = tc.GetTemplateTypeContext();
  TestCase cloned{statements, tt_ctx};

  assert(cloned.Verify());

  const std::shared_ptr<Random> &r = Random::GetInstance();
  int havoc_stack = r->NextInt(maxHavoc);
  for (int i = 0; i < havoc_stack; i++) {
//    Logger::Debug("Starting havoc #" + std::to_string(i));
    int choice = r->NextInt(3);
    if (choice == 0) {
      InplaceMutationByInsertion(cloned);
    } else if (choice == 1) {
      InplaceMutationByUpdate(cloned);
    } else {
      InplaceMutationByCleanup(cloned);
    }

    // For Debugging purpose =)
//    if (Logger::GetMessageId() > 375) {
//    Logger::Debug(cloned.DebugString(context_));
//    }
  }

  return cloned;
}

void TestCaseMutator::InplaceMutationByInsertion(TestCase &tc) {
  std::vector<std::shared_ptr<Statement>> &statements = tc.GetMutableStatements();
  TestCaseGenerator tcgen{cut_, context_};

  const std::shared_ptr<Random> &r = Random::GetInstance();
  const std::vector<std::shared_ptr<Executable>> &executables = context_->GetExecutables();
  FunctionSelector function_selector{executables, FunctionSelectorMode::kComplexityBased};
  const std::shared_ptr<Executable> &target_method = function_selector.NextExecutable();

  const std::shared_ptr<TemplateTypeContext> &tt_ctx = TemplateTypeContext::New();
//  Logger::Debug("Choosing method: " + target_method->DebugString());

  int length = (int) statements.size();
  int ins_pos = r->NextInt(length + 1);
  bool should_force_reuse_op = r->NextBoolean();

  const seqgen::GenTCForMethodSpec &gen_spec =
    seqgen::GenTCForMethodSpec{target_method, tt_ctx, statements, ins_pos, should_force_reuse_op};
  const TestCase &res = tcgen.GenForMethod(gen_spec);
  tc = res;
  assert(tc.Verify());
}
void TestCaseMutator::InplaceMutationByUpdate(TestCase &tc) {
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = tc.GetTemplateTypeContext();
  std::vector<std::shared_ptr<Statement>> &statements = tc.GetMutableStatements();
  StatementMutator mutator{cut_, context_};

  const std::shared_ptr<Random> &r = Random::GetInstance();
  int idx = r->NextInt((int) statements.size());
  std::shared_ptr<Statement> &victim = statements[idx];
  std::shared_ptr<Statement> next_stmt = mutator.MutateStatement(victim, tc);

  assert(tc.Verify());
  if (victim != next_stmt) {
    int total_repl = 0;
    std::map<std::shared_ptr<Statement>, std::shared_ptr<Statement>> repl_map{{victim, next_stmt}};
    for (auto &item : statements) {
      const auto &repl_res = item->ReplaceRefOperand(repl_map, tt_ctx);
      int repl_count = repl_res.second;
      if (repl_count > 0) {
        total_repl += repl_count;
        const std::shared_ptr<Statement> &repl_stmt = repl_res.first;
        repl_map.emplace(item, repl_stmt);
        item = repl_stmt;
      }
    }
    // We cannot store before replace refs, because victim is referencing statements[idx]
    statements[idx] = next_stmt;
    assert(tc.Verify());
  }
}
void TestCaseMutator::InplaceMutationByCleanup(TestCase &tc) {
  std::vector<std::shared_ptr<Statement>> &statements = tc.GetMutableStatements();

  std::set<std::shared_ptr<Statement>> used_stmts;
  for (const auto &stmt : statements) {
    const std::vector<Operand> &stmt_operands = stmt->GetStatementOperands();
    for (const auto &op : stmt_operands) {
      if (op.GetOperandType() == OperandType::kRefOperand) {
        const std::shared_ptr<Statement> &ref_stmt = op.GetRef();
        used_stmts.insert(ref_stmt);
      }
    }
  }
  auto erase_it = std::remove_if(
    statements.begin(), statements.end(), [used_stmts](const auto &item) {
      bool is_primitive = item->GetVariant() == StatementVariant::kPrimitiveAssignment;
      return is_primitive && used_stmts.count(item) == 0;
    });
  statements.erase(erase_it, statements.end());
  assert(tc.Verify());
}
} // namespace cxxfoozz