#include "sequencegen.hpp"

#include <set>
#include <sstream>
#include <utility>

#include "logger.hpp"
#include "mutator.hpp"
#include "random.hpp"
#include "type.hpp"

namespace cxxfoozz {

// ##########
// # TestCase
// #####

TestCase::TestCase(
  std::vector<std::shared_ptr<Statement>> statements,
  std::shared_ptr<TemplateTypeContext> template_type_context
) : statements_(std::move(statements)), template_type_context_(std::move(template_type_context)) {}
const std::vector<std::shared_ptr<Statement>> &TestCase::GetStatements() const {
  return statements_;
}
std::vector<std::shared_ptr<Statement>> &TestCase::GetMutableStatements() {
  return statements_;
}
std::string TestCase::DebugString(const std::shared_ptr<ProgramContext> &prog_ctx) const {
  std::for_each(
    statements_.begin(), statements_.end(), [](auto &i) {
      i->ClearVarName();
    });

  std::stringstream ss;
  ss << "\n ##########\n # BEGIN TEST CASE\n #####\n";
  int idx = 0;
  for (const auto &statement : statements_) {
    StatementWriter stmt_writer{prog_ctx};
    const std::string &stmt = stmt_writer.StmtAsString(statement, idx);
    ss << '[' << idx << "] " << stmt << '\n';
    ++idx;
  }
  return ss.str();
}

void AssertRefOperandsValid(
  const std::set<std::shared_ptr<Statement>> &recognized_stmt,
  const std::vector<Operand> &operands
) {
  for (const auto &operand : operands) {
    bool is_ref = operand.GetOperandType() == OperandType::kRefOperand;
    if (is_ref) {
      const std::shared_ptr<Statement> &ref_stmt = operand.GetRef();
      assert(recognized_stmt.count(ref_stmt) != 0);
    }
  }
}

bool TestCase::Verify() const {
  int idx = 0;
  std::set<std::shared_ptr<Statement>> recognized_stmt;
  const std::shared_ptr<ProgramContext> &program_ctx = ProgramContext::GetKGlobProgramCtx();
  const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm = program_ctx->GetInheritanceModel();
  for (const auto &statement : statements_) {
    assert(recognized_stmt.count(statement) == 0); // SSA Assertion
    const std::vector<Operand> &operands = statement->GetStatementOperands();
    AssertRefOperandsValid(recognized_stmt, operands);
    if (statement->GetVariant() == StatementVariant::kCall) {
      const std::shared_ptr<CallStatement> &call_stmt = std::static_pointer_cast<CallStatement>(statement);
      const std::shared_ptr<Executable> &target = call_stmt->GetTarget();
      const std::shared_ptr<TemplateTypeContext> &tt_ctx = call_stmt->GetTemplateTypeContext();

      const std::vector<clang::QualType> &rq_types = target->GetArguments();
      const std::vector<Operand> &operands_no_inv_obj = call_stmt->GetOperands();
      assert(rq_types.size() == operands_no_inv_obj.size());
      int len = (int) rq_types.size();
      for (int i = 0; i < len; i++) {
        const TWMSpec &twm_spec = TWMSpec::ByClangType(rq_types[i], nullptr);
        const TypeWithModifier &rq_twm = TypeWithModifier::FromSpec(twm_spec);
        const TypeWithModifier &op_twm = operands_no_inv_obj[i].GetType();
//        assert(rq_twm.IsAssignableFrom(op_twm, tt_ctx, itm)); // commented because tt_ctx scoping is still buggy
        if (rq_twm.IsPointerOrArray()
          && operands_no_inv_obj[i].GetOperandType() == OperandType::kConstantOperand) {
          if (op_twm.GetType() == PrimitiveType::kCharacter) {
          } else if (operands_no_inv_obj[i].IsNullPtr()) {
          } else {
            assert(false);
          }
        }
      }
    }
    recognized_stmt.insert(statement);
    idx++;
  }
  return true;
}
const std::shared_ptr<TemplateTypeContext> &TestCase::GetTemplateTypeContext() const {
  return template_type_context_;
}

// ##########
// # TestCaseGenerator
// #####

TestCaseGenerator::TestCaseGenerator(std::shared_ptr<ClassType> cut, std::shared_ptr<ProgramContext> context)
  : cut_(std::move(cut)), context_(std::move(context)) {}

std::unique_ptr<TemplateTypeContext> CreateTemplateTypeContext(const std::vector<TemplateTypeParamList> &template_typenames) {
  TemplateTypeInstMapping mapping;
  for (const auto &param_list : template_typenames) {
    const std::vector<TemplateTypeParam> &param_vc = param_list.GetList();
    for (const auto &type : param_vc) {
      const std::shared_ptr<Random> &r = Random::GetInstance();
      bool should_int = r->NextBoolean();
      const std::shared_ptr<PrimitiveType> &target_type = should_int ? PrimitiveType::kInteger : PrimitiveType::kDouble;

      const TWMSpec &twm_spec = TWMSpec::ByType(target_type, nullptr);
      const TypeWithModifier &target_twm = TypeWithModifier::FromSpec(twm_spec);
      mapping.Bind(type, target_twm);
    }
  }
  return std::make_unique<TemplateTypeContext>(mapping);
}

TestCase TestCaseGenerator::GenForMethod(const seqgen::GenTCForMethodSpec &spec) const {
  const std::shared_ptr<Executable> &target = spec.GetTarget();
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = spec.GetTemplateTypeContext();
  const std::vector<std::shared_ptr<Statement>> &statement_ctx = spec.GetStatementContext();
  int placement_idx = spec.GetPlacementIdx();
  bool force_avail_op = spec.IsForceAvailOp();

  const std::shared_ptr<ClassTypeModel> &owner = target->GetOwner();
  std::vector<TemplateTypeParamList> template_typenames;
  if (owner != nullptr && !owner->GetTemplateParamList().IsEmpty()) {
    template_typenames.push_back(owner->GetTemplateParamList());
  }
  const TemplateTypeParamList &method_template_types = target->GetTemplateParamList();
  if (!method_template_types.IsEmpty()) {
    template_typenames.push_back(method_template_types);
  }

  const std::vector<clang::QualType> &arguments = target->GetArguments();
  std::vector<std::shared_ptr<Statement>> statements(statement_ctx.begin(), statement_ctx.begin() + placement_idx);

  OperandResolver operand_resolver{context_};
  std::vector<Operand> operands;
  for (const auto &arg : arguments) {
    const TWMSpec &twm_spec = TWMSpec::ByClangType(arg, nullptr);
    const TypeWithModifier &type_with_modifier = TypeWithModifier::FromSpec(twm_spec);
    const seqgen::ResolveOperandSpec &operand_spec =
      seqgen::ResolveOperandSpec{type_with_modifier, statements, tt_ctx, force_avail_op};
    const Operand &operand = operand_resolver.ResolveOperand(operand_spec);
    operands.push_back(operand);
  }

  bool is_method_variant = target->GetExecutableVariant() == ExecutableVariant::kMethod;
  bool require_invoking_object = is_method_variant && owner != nullptr && !target->IsNotRequireInvokingObj();

  bpstd::optional<Operand> opt_invoking_obj;
  if (require_invoking_object) {
    const std::string &class_name = owner->GetQualifiedName();
    const std::shared_ptr<ClassType> &class_type = ClassType::GetTypeByQualName(class_name);

    const TWMSpec &twm_spec = TWMSpec::ByType(class_type, nullptr);
    const TypeWithModifier &type_with_modifier = TypeWithModifier::FromSpec(twm_spec);
    const seqgen::ResolveOperandSpec &operand_spec =
      seqgen::ResolveOperandSpec{type_with_modifier, statements, tt_ctx, force_avail_op};
    const Operand &invoking_obj = operand_resolver.ResolveOperand(operand_spec);
    opt_invoking_obj = bpstd::make_optional(invoking_obj);
  }

  const std::shared_ptr<Random> &r = Random::GetInstance();
  int repeat = r->NextInt(1, 4);
  for (int i = 0; i < repeat; i++) {
    const std::shared_ptr<Statement> &statement = CallStatement::MakeExecutableCall(
      target,
      operands,
      opt_invoking_obj,
      tt_ctx
    );
    statements.push_back(statement);
  }

  std::vector<std::shared_ptr<Statement>> remaining(statement_ctx.begin() + placement_idx, statement_ctx.end());
  statements.insert(statements.end(), remaining.begin(), remaining.end());

  return TestCase{statements, tt_ctx};
}

namespace seqgen {

// ##########
// # ResolveOperandSpec
// #####

ResolveOperandSpec::ResolveOperandSpec(
  const TypeWithModifier &type,
  std::vector<std::shared_ptr<Statement>> &statements,
  std::shared_ptr<TemplateTypeContext> template_type_context,
  bool force_avail_op
)
  : type_(type),
    statements_(statements),
    template_type_context_(std::move(template_type_context)),
    force_avail_op_(force_avail_op) {}
const TypeWithModifier &ResolveOperandSpec::GetType() const {
  return type_;
}
std::vector<std::shared_ptr<Statement>> &ResolveOperandSpec::GetStatements() const {
  return statements_;
}
const std::shared_ptr<TemplateTypeContext> &ResolveOperandSpec::GetTemplateTypeContext() const {
  return template_type_context_;
}
bool ResolveOperandSpec::IsForceAvailOp() const {
  return force_avail_op_;
}

// ##########
// # GenTCForMethodSpec
// #####

GenTCForMethodSpec::GenTCForMethodSpec(
  std::shared_ptr<Executable> target,
  std::shared_ptr<TemplateTypeContext> template_type_context,
  bool force_avail_op
)
  : target_(std::move(target)),
    placement_idx_(0),
    template_type_context_(std::move(template_type_context)),
    force_avail_op_(force_avail_op) {}
GenTCForMethodSpec::GenTCForMethodSpec(
  std::shared_ptr<Executable> target,
  std::shared_ptr<TemplateTypeContext> template_type_context,
  std::vector<std::shared_ptr<Statement>> context,
  int placement_idx,
  bool force_avail_op
)
  : target_(std::move(target)),
    statement_context_(std::move(context)),
    placement_idx_(placement_idx),
    template_type_context_(std::move(template_type_context)),
    force_avail_op_(force_avail_op) {}
const std::shared_ptr<Executable> &GenTCForMethodSpec::GetTarget() const {
  return target_;
}
const std::vector<std::shared_ptr<Statement>> &GenTCForMethodSpec::GetStatementContext() const {
  return statement_context_;
}
int GenTCForMethodSpec::GetPlacementIdx() const {
  return placement_idx_;
}
const std::shared_ptr<TemplateTypeContext> &GenTCForMethodSpec::GetTemplateTypeContext() const {
  return template_type_context_;
}
bool GenTCForMethodSpec::IsForceAvailOp() const {
  return force_avail_op_;
}
} // namespace seqgen

// ##########
// # STLOperandResolver
// #####

const int STLOperandResolver::kMaxElementsExclusive = 4;
const int STLOperandResolver::kMaxElementsForStringExclusive = 11;
Operand STLOperandResolver::Handle(const seqgen::ResolveOperandSpec &spec) {
  const TypeWithModifier &twm = spec.GetType();
  std::vector<std::shared_ptr<Statement>> &statements = spec.GetStatements();
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = spec.GetTemplateTypeContext();
  assert(twm.IsTemplateTypenameSpcType());

  const std::shared_ptr<Type> &type = twm.GetType();
  const std::shared_ptr<TemplateTypenameSpcType> &tt_spc_type = std::static_pointer_cast<TemplateTypenameSpcType>(type);
  const std::shared_ptr<Type> &target_type = tt_spc_type->GetTargetType();
  assert(target_type->GetVariant() == TypeVariant::kSTL);

  const std::shared_ptr<STLType> &stl_type = std::static_pointer_cast<STLType>(target_type);
  STLTypeVariant stl_type_variant = stl_type->GetSTLTypeVariant();

  const TemplateTypeInstList &inst_list = tt_spc_type->GetInstList();
  const std::vector<TemplateTypeInstantiation> &tt_insts = inst_list.GetInstantiations();

  switch (stl_type_variant) {
    case STLTypeVariant::kRegContainer:
      return ResolveRegularContainer(spec, stl_type, tt_insts);
    case STLTypeVariant::kRegContainerWithSize:
      return ResolveArray(spec, tt_insts);
    case STLTypeVariant::kKeyValueContainer:
      return ResolveKeyValueContainer(spec, stl_type, tt_insts);
    case STLTypeVariant::kPair:
      return ResolvePair(spec, tt_insts);
    case STLTypeVariant::kTuple:
      return ResolveTuple(spec, tt_insts);
    case STLTypeVariant::kSmartPointer:
      return ResolveSmartPointer(spec, stl_type, tt_insts);
    case STLTypeVariant::kString:
      return ResolveString(spec, tt_insts);
  }
  assert(false);
}
STLOperandResolver::STLOperandResolver(OperandResolver &operand_resolver) : operand_resolver_(operand_resolver) {}
Operand STLOperandResolver::ResolveRegularContainer(
  const seqgen::ResolveOperandSpec &spec,
  const std::shared_ptr<STLType> &stl_type,
  const std::vector<TemplateTypeInstantiation> &tt_insts
) {
  const TypeWithModifier &twm = spec.GetType();
  std::vector<std::shared_ptr<Statement>> &statements = spec.GetStatements();
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = spec.GetTemplateTypeContext();
  bool force_avail_op = spec.IsForceAvailOp();

  assert(tt_insts.size() == 1);
  const TemplateTypeInstantiation &inst = tt_insts[0];
  assert(inst.IsType());
  const TypeWithModifier &rq_type = inst.GetType();

  const std::shared_ptr<Random> &r = Random::GetInstance();
  int num_elements = r->NextInt(kMaxElementsExclusive);

  std::vector<Operand> operands;
  for (int i = 0; i < num_elements; i++) {
    const seqgen::ResolveOperandSpec &op_spec = seqgen::ResolveOperandSpec{rq_type, statements, tt_ctx, force_avail_op};
    const Operand &operand = operand_resolver_.ResolveOperand(op_spec);
    operands.push_back(operand);
  }
  const STLElement &stl_elements = STLElement::ForRegularContainer(operands);
  const TypeWithModifier &stmt_twm = twm.StripAllModifiers();
  const std::shared_ptr<Statement> &stl_stmt = STLStatement::MakeSTLStatement(stmt_twm, stl_type, stl_elements);
  statements.push_back(stl_stmt);
  return Operand::MakeRefOperand(stl_stmt);
}
Operand STLOperandResolver::ResolveArray(
  const seqgen::ResolveOperandSpec &spec,
  const std::vector<TemplateTypeInstantiation> &tt_insts
) {
  const TypeWithModifier &twm = spec.GetType();
  std::vector<std::shared_ptr<Statement>> &statements = spec.GetStatements();
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = spec.GetTemplateTypeContext();
  bool force_avail_op = spec.IsForceAvailOp();

  assert(tt_insts.size() == 2);
  const TemplateTypeInstantiation &inst_type = tt_insts[0];
  const TemplateTypeInstantiation &inst_size = tt_insts[1];
  assert(inst_type.IsType() && inst_size.GetVariant() == TemplateTypeInstVariant::kIntegral);

  const TypeWithModifier &rq_type = inst_type.GetType();
  const int &num_elements = inst_size.GetIntegral();

  std::vector<Operand> operands;
  for (int i = 0; i < num_elements; i++) {
    const seqgen::ResolveOperandSpec &op_spec = seqgen::ResolveOperandSpec{rq_type, statements, tt_ctx, force_avail_op};
    const Operand &operand = operand_resolver_.ResolveOperand(op_spec);
    operands.push_back(operand);
  }
  const STLElement &stl_elements = STLElement::ForRegularContainer(operands);
  const TypeWithModifier &stmt_twm = twm.StripAllModifiers();
  const std::shared_ptr<Statement>
    &stl_stmt = STLStatement::MakeSTLStatement(stmt_twm, STLType::kArray, stl_elements);
  statements.push_back(stl_stmt);
  return Operand::MakeRefOperand(stl_stmt);
}
Operand STLOperandResolver::ResolveKeyValueContainer(
  const seqgen::ResolveOperandSpec &spec,
  const std::shared_ptr<STLType> &stl_type,
  const std::vector<TemplateTypeInstantiation> &tt_insts
) {
  const TypeWithModifier &twm = spec.GetType();
  std::vector<std::shared_ptr<Statement>> &statements = spec.GetStatements();
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = spec.GetTemplateTypeContext();
  bool force_avail_op = spec.IsForceAvailOp();

  assert(tt_insts.size() == 2);
  const TemplateTypeInstantiation &inst_key = tt_insts[0];
  const TemplateTypeInstantiation &inst_value = tt_insts[1];
  assert(inst_key.IsType() && inst_value.IsType());

  const std::shared_ptr<Random> &r = Random::GetInstance();
  int num_elements = r->NextInt(kMaxElementsExclusive);

  const TypeWithModifier &rq_type_key = inst_key.GetType();
  const TypeWithModifier &rq_type_value = inst_value.GetType();

  const std::shared_ptr<InheritanceTreeModel> &itm = operand_resolver_.context_->GetInheritanceModel();
  std::vector<std::pair<Operand, Operand>> operands;
  for (int i = 0; i < num_elements; i++) {
    const seqgen::ResolveOperandSpec &op_key_spec =
      seqgen::ResolveOperandSpec{rq_type_key, statements, tt_ctx, force_avail_op};
    const Operand &op_key = operand_resolver_.ResolveOperand(op_key_spec);
    assert(rq_type_key.IsAssignableFrom(op_key.GetType(), tt_ctx, itm));

    const seqgen::ResolveOperandSpec &op_value_spec =
      seqgen::ResolveOperandSpec{rq_type_value, statements, tt_ctx, force_avail_op};
    const Operand &op_value = operand_resolver_.ResolveOperand(op_value_spec);
    assert(rq_type_value.IsAssignableFrom(op_value.GetType(), tt_ctx, itm));
    operands.emplace_back(op_key, op_value);
  }
  const STLElement &stl_elements = STLElement::ForKeyValueContainer(operands);
  const TypeWithModifier &stmt_twm = twm.StripAllModifiers();
  const std::shared_ptr<Statement> &stl_stmt = STLStatement::MakeSTLStatement(stmt_twm, stl_type, stl_elements);
  statements.push_back(stl_stmt);
  return Operand::MakeRefOperand(stl_stmt);
}
Operand STLOperandResolver::ResolvePair(
  const seqgen::ResolveOperandSpec &spec,
  const std::vector<TemplateTypeInstantiation> &tt_insts
) {
  const TypeWithModifier &twm = spec.GetType();
  std::vector<std::shared_ptr<Statement>> &statements = spec.GetStatements();
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = spec.GetTemplateTypeContext();
  bool force_avail_op = spec.IsForceAvailOp();

  assert(tt_insts.size() == 2);
  const TemplateTypeInstantiation &inst_key = tt_insts[0];
  const TemplateTypeInstantiation &inst_value = tt_insts[1];
  assert(inst_key.IsType() && inst_value.IsType());

  const TypeWithModifier &rq_type_key = inst_key.GetType();
  const TypeWithModifier &rq_type_value = inst_value.GetType();

  const seqgen::ResolveOperandSpec &op_spec1 =
    seqgen::ResolveOperandSpec{rq_type_key, statements, tt_ctx, force_avail_op};
  const Operand &op_fi = operand_resolver_.ResolveOperand(op_spec1);

  const seqgen::ResolveOperandSpec &op_spec2 =
    seqgen::ResolveOperandSpec{rq_type_value, statements, tt_ctx, force_avail_op};
  const Operand &op_sc = operand_resolver_.ResolveOperand(op_spec2);

  std::vector<std::pair<Operand, Operand>> operands{std::make_pair(op_fi, op_sc)};
  const STLElement &stl_element = STLElement::ForKeyValueContainer(operands);

  const TypeWithModifier &stmt_twm = twm.StripAllModifiers();
  const std::shared_ptr<Statement> &statement =
    STLStatement::MakeSTLStatement(stmt_twm, STLType::kPair, stl_element);
  statements.push_back(statement);
  return Operand::MakeRefOperand(statement);
}
Operand STLOperandResolver::ResolveTuple(
  const seqgen::ResolveOperandSpec &spec,
  const std::vector<TemplateTypeInstantiation> &tt_insts
) {
  const TypeWithModifier &twm = spec.GetType();
  std::vector<std::shared_ptr<Statement>> &statements = spec.GetStatements();
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = spec.GetTemplateTypeContext();
  bool force_avail_op = spec.IsForceAvailOp();

  std::vector<Operand> operands;
  int tt_len = (int) tt_insts.size();
  for (int i = 0; i < tt_len; i++) {
    const TemplateTypeInstantiation &inst = tt_insts[i];
    const TypeWithModifier &rq_type = inst.GetType();
    const seqgen::ResolveOperandSpec &op_spec = seqgen::ResolveOperandSpec{rq_type, statements, tt_ctx, force_avail_op};
    const Operand &operand = operand_resolver_.ResolveOperand(op_spec);
    operands.push_back(operand);
  }
  const STLElement &stl_elements = STLElement::ForRegularContainer(operands);
  const TypeWithModifier &stmt_twm = twm.StripAllModifiers();
  const std::shared_ptr<Statement> &stl_stmt =
    STLStatement::MakeSTLStatement(stmt_twm, STLType::kTuple, stl_elements);
  statements.push_back(stl_stmt);
  return Operand::MakeRefOperand(stl_stmt);
}
Operand STLOperandResolver::ResolveSmartPointer(
  const seqgen::ResolveOperandSpec &spec,
  const std::shared_ptr<STLType> &stl_type,
  const std::vector<TemplateTypeInstantiation> &tt_insts
) {
  const TypeWithModifier &twm = spec.GetType();
  std::vector<std::shared_ptr<Statement>> &statements = spec.GetStatements();
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = spec.GetTemplateTypeContext();
  bool force_avail_op = spec.IsForceAvailOp();

  assert(tt_insts.size() == 1);
  const TemplateTypeInstantiation &instantiation = tt_insts[0];
  const TypeWithModifier &rq_type = instantiation.GetType();

  const seqgen::ResolveOperandSpec &op_spec = seqgen::ResolveOperandSpec{rq_type, statements, tt_ctx, force_avail_op};
  const Operand &operand = operand_resolver_.ResolveOperand(op_spec);

  const STLElement &stl_element = STLElement::ForRegularContainer({operand});
  const TypeWithModifier &stmt_twm = twm.StripAllModifiers();
  const std::shared_ptr<Statement> &statement =
    STLStatement::MakeSTLStatement(stmt_twm, stl_type, stl_element);
  statements.push_back(statement);
  return Operand::MakeRefOperand(statement);
}
Operand STLOperandResolver::ResolveString(
  const seqgen::ResolveOperandSpec &spec,
  const std::vector<TemplateTypeInstantiation> &tt_insts
) {
  const TypeWithModifier &twm = spec.GetType();
  std::vector<std::shared_ptr<Statement>> &statements = spec.GetStatements();
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = spec.GetTemplateTypeContext();
  bool force_avail_op = spec.IsForceAvailOp();

  assert(tt_insts.size() == 1);
  const TemplateTypeInstantiation &inst = tt_insts[0];
  assert(inst.IsType());
  const TypeWithModifier &rq_type = inst.GetType();

  const std::shared_ptr<Random> &r = Random::GetInstance();
  int num_elements = r->NextInt(kMaxElementsForStringExclusive);

  std::vector<Operand> operands;
  for (int i = 0; i < num_elements; i++) {
    const seqgen::ResolveOperandSpec &op_spec = seqgen::ResolveOperandSpec{rq_type, statements, tt_ctx, force_avail_op};
    const Operand &operand = operand_resolver_.ResolveOperand(op_spec);
    operands.push_back(operand);
  }
  const STLElement &stl_elements = STLElement::ForRegularContainer(operands);
  const TypeWithModifier &stmt_twm = twm.StripAllModifiers();
  const std::shared_ptr<Statement> &stl_stmt =
    STLStatement::MakeSTLStatement(stmt_twm, STLType::kBasicString, stl_elements);
  statements.push_back(stl_stmt);
  return Operand::MakeRefOperand(stl_stmt);
}

// ##########
// # OperandResolver
// #####

bool AssertOperandType(
  const TypeWithModifier &target_type,
  const Operand &result_operand,
  const std::shared_ptr<TemplateTypeContext> &tt_ctx,
  const std::shared_ptr<cxxfoozz::InheritanceTreeModel> &itm
) {
  const TypeWithModifier &result_type = result_operand.GetType();
  bool is_assignable = target_type.IsAssignableFrom(result_type, tt_ctx, itm);
  if (!is_assignable) {
    bool second_chance = target_type.IsAssignableFrom(result_type, tt_ctx, itm);
    assert(false);
  }
  return true;
}

OperandResolver::OperandResolver(std::shared_ptr<ProgramContext> context)
  : context_(std::move(context)) {}

Operand OperandResolver::ResolveOperandPrimitiveType(const seqgen::ResolveOperandSpec &spec) {
  const TypeWithModifier &target_type = spec.GetType();
  std::vector<std::shared_ptr<Statement>> &statements = spec.GetStatements();

  const std::shared_ptr<Type> &strip_type = target_type.GetType();
  if (strip_type == PrimitiveType::kVoid) {
    Logger::Error("[ResolveOperandPrimitiveType]", "Unhandled void type :(", true);
    return Operand::MakeConstantOperand(target_type, "nullptr");
  }

  bool is_char = strip_type == PrimitiveType::kCharacter;
  bool is_ptr = target_type.IsPointer();
  if (is_char && is_ptr) {
    const std::shared_ptr<Random> &r = Random::GetInstance();
    const std::string &random_string = r->NextString();
    const TypeWithModifier &const_char_twm = target_type.WithAdditionalModifiers({Modifier::kConst});
    const Operand &operand = Operand::MakeConstantOperand(const_char_twm, random_string);
    const std::shared_ptr<Statement> &stmt = ArrayInitStatement::MakeCString(operand);
    statements.push_back(stmt);
    return Operand::MakeRefOperand(stmt);
//    const std::shared_ptr<Statement> &statement =
//      PrimitiveAssignmentStatement::MakeUnaryOpStatement(operand, GeneralPrimitiveOp::kNop);
//    statements.push_back(statement);
//    return Operand::MakeRefOperand(statement);
  }

  // Remember that: resolving operand must be from Stripped Type
  // E.g., resolving char* = resolving char AND THEN pass it using & operator
  const TypeWithModifier &twm_no_mods = target_type.StripParticularModifiers(
    {
      Modifier::kConst,
      Modifier::kConstOnPointer,
      Modifier::kPointer,
      Modifier::kArray,
      Modifier::kReference,
    });

  OperandMutator op_mut{context_};
  const Operand &operand = op_mut.MutateConstantOperand(twm_no_mods);

  bool is_reference = target_type.IsReference();
  bool is_array = target_type.IsArray();
  bool is_void = target_type.GetType() == PrimitiveType::kVoid;
  if (!is_void) {
    if (is_ptr || is_reference || is_array) {
      const std::shared_ptr<Statement> &statement =
        PrimitiveAssignmentStatement::MakeUnaryOpStatement(operand, GeneralPrimitiveOp::kNop);
      statements.push_back(statement);
      return Operand::MakeRefOperand(statement);
    }
  }

  return operand;
}

Operand OperandResolver::ResolveOperandClassType(const seqgen::ResolveOperandSpec &spec) {
  const TypeWithModifier &target_type = spec.GetType();
  std::vector<std::shared_ptr<Statement>> &statements = spec.GetStatements();
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = spec.GetTemplateTypeContext();
  bool force_avail_op = spec.IsForceAvailOp();

  const std::shared_ptr<Type> &strip_type = target_type.GetType();
  assert(strip_type->GetVariant() == TypeVariant::kClass);

  const std::shared_ptr<ClassType> &class_type = std::static_pointer_cast<ClassType>(strip_type);
  const std::shared_ptr<ClassTypeModel> &target_class_model = class_type->GetModel();

  const std::shared_ptr<InheritanceTreeModel> &itm = context_->GetInheritanceModel();
  const std::set<std::shared_ptr<ClassTypeModel>> &subclasses = itm->LookupSubClasses(target_class_model);

  const std::vector<std::shared_ptr<Creator>> &creators = context_->GetCreators();
  std::vector<std::shared_ptr<Creator>> type_creators;
  for (const auto &creator : creators) {
    const std::shared_ptr<ClassTypeModel> &creator_class = creator->GetTargetClass();
    if (creator_class == target_class_model || subclasses.count(creator_class) > 0) {
      CreatorVariant creator_variant = creator->GetCreatorVariant();
      switch (creator_variant) {
        case CreatorVariant::kConstructor: {
          type_creators.push_back(creator);
          break;
        }
        case CreatorVariant::kStaticFactory: {
          const clang::QualType &ret_type = *creator->GetReturnType();
          const TWMSpec &twm_spec = TWMSpec::ByClangType(ret_type, tt_ctx);
          const TypeWithModifier &ret_twm = TypeWithModifier::FromSpec(twm_spec);
          if (target_type.IsAssignableFrom(ret_twm, tt_ctx, itm)) {
            type_creators.push_back(creator);
          }
          break;
        }
        case CreatorVariant::kMethodWithReferenceArg: {
          assert(false); // NOT IMPLEMENTED
          break;
        }
      }
    }
  }

  const std::shared_ptr<Random> &r = Random::GetInstance();
  int idx = r->NextInt((int) type_creators.size());
  std::shared_ptr<Creator> &selected_creator = type_creators[idx];

  const std::vector<clang::QualType> &arguments = selected_creator->GetArguments();
  std::vector<Operand> operands;
  for (const auto &argument : arguments) {
    const TWMSpec &twm_spec = TWMSpec::ByClangType(argument, nullptr);
    const TypeWithModifier &argument_type = TypeWithModifier::FromSpec(twm_spec);
    const seqgen::ResolveOperandSpec &operand_spec =
      seqgen::ResolveOperandSpec{argument_type, statements, tt_ctx, force_avail_op};
    const Operand &result_operand = ResolveOperand(operand_spec);
    operands.push_back(result_operand);
  }

  const std::shared_ptr<Statement> &statement =
    CallStatement::MakeExecutableCall(selected_creator, operands, bpstd::nullopt, tt_ctx);
  statements.push_back(statement);
  const Operand &result = Operand::MakeRefOperand(statement);
  assert(AssertOperandType(target_type, result, tt_ctx, itm));
  return result;
}

Operand OperandResolver::ResolveOperandEnumType(const seqgen::ResolveOperandSpec &spec) {
  const TypeWithModifier &type_with_modifier = spec.GetType();
  std::vector<std::shared_ptr<Statement>> &statements = spec.GetStatements();
  const std::shared_ptr<Type> &type = type_with_modifier.GetType();
  assert(type->GetVariant() == TypeVariant::kEnum);

  const std::shared_ptr<EnumType> &enum_type = std::static_pointer_cast<EnumType>(type);
  const std::shared_ptr<EnumTypeModel> &enum_tm = enum_type->GetModel();
  const std::vector<std::string> &variants = enum_tm->GetVariants();

  OperandMutator op_mut{context_};
  const Operand &operand = op_mut.MutateConstantOperand(type_with_modifier);
  const std::shared_ptr<Statement> &statement =
    PrimitiveAssignmentStatement::MakeUnaryOpStatement(operand, GeneralPrimitiveOp::kNop);
  statements.push_back(statement);
  return Operand::MakeRefOperand(statement);
}

Operand OperandResolver::ResolveOperandTemplateTypenameType(const seqgen::ResolveOperandSpec &spec) {
  const TypeWithModifier &original_twm = spec.GetType();
  std::vector<std::shared_ptr<Statement>> &statements = spec.GetStatements();
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = spec.GetTemplateTypeContext();
  bool force_avail_op = spec.IsForceAvailOp();

  const std::shared_ptr<Type> &type = original_twm.GetType();
  const std::shared_ptr<TemplateTypenameType> &tt_type = std::static_pointer_cast<TemplateTypenameType>(type);
  const std::string &template_typename = tt_type->GetName();
  const TypeWithModifier &target_twm = tt_ctx->LookupOrResolve(template_typename)
    .WithAdditionalModifiers(original_twm.GetModifiers());

  const seqgen::ResolveOperandSpec &new_resolve_spec =
    seqgen::ResolveOperandSpec{target_twm, statements, tt_ctx, force_avail_op};
  return ResolveOperand(new_resolve_spec);
}

Operand OperandResolver::ResolveOperandTemplateTypenameSpcType(const seqgen::ResolveOperandSpec &spec) {
  const TypeWithModifier &twm = spec.GetType();
  std::vector<std::shared_ptr<Statement>> &statements = spec.GetStatements();
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = spec.GetTemplateTypeContext();
  bool force_avail_op = spec.IsForceAvailOp();

  const std::shared_ptr<Type> &type = twm.GetType();
  const std::shared_ptr<TemplateTypenameSpcType> &tt_spc_type = std::static_pointer_cast<TemplateTypenameSpcType>(type);

  const std::shared_ptr<Type> &target_type = tt_spc_type->GetTargetType();
  if (target_type->GetVariant() == TypeVariant::kSTL) {
    return ResolveOperandTemplateTypenameSpcTypeForSTL(spec);

  } else if (target_type->GetVariant() == TypeVariant::kClass) {
    const std::shared_ptr<ClassType> &class_type = std::static_pointer_cast<ClassType>(target_type);
    const std::shared_ptr<ClassTypeModel> &class_model = class_type->GetModel();
    const TemplateTypeParamList &template_param_list = class_model->GetTemplateParamList();
    const TemplateTypeInstList &tt_inst_list = tt_spc_type->GetInstList();

    const std::shared_ptr<TemplateTypeContext> &cloned_tt_ctx = TemplateTypeContext::Clone(tt_ctx);
    const std::vector<TemplateTypeParam> &tt_params = template_param_list.GetList();
    const std::vector<TemplateTypeInstantiation> &instantiations = tt_inst_list.GetInstantiations();

    assert(tt_params.size() == instantiations.size());
    int length = (int) tt_params.size();
    for (int i = 0; i < length; i++) {
      const TemplateTypeParam &param_i = tt_params[i];
      const TemplateTypeInstantiation &inst_i = instantiations[i];
      switch (inst_i.GetVariant()) {
        case TemplateTypeInstVariant::kType: {
          const TypeWithModifier &twm_i = inst_i.GetType();
          cloned_tt_ctx->Bind(param_i, twm_i);
          break;
        }
        case TemplateTypeInstVariant::kIntegral: {
          assert(false);
          break;
        }
        case TemplateTypeInstVariant::kNullptr: {
          assert(false);
          break;
        }
      }
    }

    const TWMSpec &twm_spec = TWMSpec::ByType(target_type, nullptr);
    const TypeWithModifier &target_twm = TypeWithModifier::FromSpec(twm_spec);

    const seqgen::ResolveOperandSpec &operand_spec = seqgen::ResolveOperandSpec{
      target_twm, statements, cloned_tt_ctx, force_avail_op,
    };
    return ResolveOperand(operand_spec);
  } else {
    assert(false);
    return Operand::MakeBottom();
  }
}

Operand OperandResolver::ResolveOperandTemplateTypenameSpcTypeForSTL(const seqgen::ResolveOperandSpec &spec) {
  STLOperandResolver stl_resolver{*this};
  return stl_resolver.Handle(spec);
}

Operand OperandResolver::ResolveOperand(const seqgen::ResolveOperandSpec &spec) {
  const TypeWithModifier &target_type = spec.GetType();
  TypeVariant type_variant = target_type.GetType()->GetVariant();
  bool force_avail_op = spec.IsForceAvailOp();

  if (target_type.IsVoidPtr()) {
    return Operand::MakeConstantOperand(target_type, "nullptr");
  } else if (target_type.IsVoidType()) {
    assert(false);
  }

  if (force_avail_op) {
    const bpstd::optional<Operand> &opt_operand = ResolveUsingAssignableStatements(spec);
    if (opt_operand.has_value())
      return opt_operand.value();
  }
  static const double kNullptrProb = 0.1;
  if (target_type.IsPointer()) {
    const std::shared_ptr<Random> &r = Random::GetInstance();
    double prob = r->NextGaussian();
    if (prob < kNullptrProb) {
      return Operand::MakeConstantOperand(target_type, "nullptr");
    }
  }

  Operand result = Operand::MakeBottom();
  switch (type_variant) {
    case TypeVariant::kPrimitive:
      result = ResolveOperandPrimitiveType(spec);
      break;
    case TypeVariant::kClass:
      result = ResolveOperandClassType(spec);
      break;
    case TypeVariant::kEnum:
      result = ResolveOperandEnumType(spec);
      break;
    case TypeVariant::kTemplateTypename:
      result = ResolveOperandTemplateTypenameType(spec);
      break;
    case TypeVariant::kTemplateTypenameSpc:
      result = ResolveOperandTemplateTypenameSpcType(spec);
      break;
    case TypeVariant::kSTL:
      break;
  }
  return result;
}
std::vector<std::shared_ptr<Statement>> OperandResolver::GetAssignableStatements(
  const TypeWithModifier &target_type,
  const std::shared_ptr<Statement> &op_stmt_ctx,
  const TestCase &tc_ctx
) {
  const std::vector<std::shared_ptr<Statement>> &stmts = tc_ctx.GetStatements();
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = tc_ctx.GetTemplateTypeContext();
  std::vector<std::shared_ptr<Statement>> assignable_stmts;
  for (const auto &stmt : stmts) {
    if (stmt == op_stmt_ctx)
      break;

    const TypeWithModifier &stmt_type = stmt->GetType();
    const std::shared_ptr<InheritanceTreeModel> &inheritance_model = context_->GetInheritanceModel();
    bool is_assignable = target_type.IsAssignableFrom(stmt_type, tt_ctx, inheritance_model);
    if (is_assignable)
      assignable_stmts.push_back(stmt);
  }
  return assignable_stmts;
}
bpstd::optional<Operand> OperandResolver::ResolveUsingAssignableStatements(const seqgen::ResolveOperandSpec &spec) {
  const TypeWithModifier &target_type = spec.GetType();
  const std::vector<std::shared_ptr<Statement>> &statements = spec.GetStatements();
  const std::shared_ptr<TemplateTypeContext> &tt_ctx = spec.GetTemplateTypeContext();
  TestCase tc{statements, tt_ctx};

  const std::vector<std::shared_ptr<Statement>> &assignable_stmts =
    OperandResolver::GetAssignableStatements(target_type, nullptr, tc);
  if (assignable_stmts.empty()) {
    return bpstd::nullopt;
  }

  const std::shared_ptr<Random> &r = Random::GetInstance();
  int choice = r->NextInt((int) assignable_stmts.size());
  const std::shared_ptr<Statement> &next = assignable_stmts[choice];
  return Operand::MakeRefOperand(next);
}

// ##########
// # CreatorCyclicChecker
// #####
CreatorCyclicChecker::CreatorCyclicChecker() = default;
int CreatorCyclicChecker::kCycleThreshold = 3;
bool CreatorCyclicChecker::IsCyclic(const std::shared_ptr<Creator> &creator) {
  unsigned long tmp_cnt = used_.count(creator);
  if (tmp_cnt >= kCycleThreshold) {
    return true;
  }
  used_.insert(creator);
  return false;
}
} // namespace cxxfoozz
