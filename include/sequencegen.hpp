#ifndef CXXFOOZZ_INCLUDE_SEQUENCEGEN_HPP_
#define CXXFOOZZ_INCLUDE_SEQUENCEGEN_HPP_

#include "statement.hpp"
#include "type.hpp"
#include "program-context.hpp"

namespace cxxfoozz {

class TestCase {
 public:
  TestCase(
    std::vector<std::shared_ptr<Statement>> statements,
    std::shared_ptr<TemplateTypeContext> template_type_context
  );
  const std::vector<std::shared_ptr<Statement>> &GetStatements() const;
  std::vector<std::shared_ptr<Statement>> &GetMutableStatements();
  const std::shared_ptr<TemplateTypeContext> &GetTemplateTypeContext() const;
  std::string DebugString(const std::shared_ptr<ProgramContext> &prog_ctx) const;
  bool Verify() const;
 private:
  std::vector<std::shared_ptr<Statement>> statements_;
  std::shared_ptr<TemplateTypeContext> template_type_context_; // TODO: Do we need tt_ctx in TC level?
};

namespace seqgen {
class ResolveOperandSpec {
 public:
  ResolveOperandSpec(
    const TypeWithModifier &type,
    std::vector<std::shared_ptr<Statement>> &statements,
    std::shared_ptr<TemplateTypeContext> template_type_context,
    bool force_avail_op
  );
  const TypeWithModifier &GetType() const;
  std::vector<std::shared_ptr<Statement>> &GetStatements() const;
  const std::shared_ptr<TemplateTypeContext> &GetTemplateTypeContext() const;
  bool IsForceAvailOp() const;

 private:
  const TypeWithModifier &type_;
  std::vector<std::shared_ptr<Statement>> &statements_;
  std::shared_ptr<TemplateTypeContext> template_type_context_;
  bool force_avail_op_;
};

class GenTCForMethodSpec {
 public:
  GenTCForMethodSpec(
    std::shared_ptr<Executable> target,
    std::shared_ptr<TemplateTypeContext> template_type_context,
    bool force_avail_op = false
  );
  GenTCForMethodSpec(
    std::shared_ptr<Executable> target,
    std::shared_ptr<TemplateTypeContext> template_type_context,
    std::vector<std::shared_ptr<Statement>> context,
    int placement_idx = 0,
    bool force_avail_op = false
  );
  const std::shared_ptr<Executable> &GetTarget() const;
  const std::shared_ptr<TemplateTypeContext> &GetTemplateTypeContext() const;
  const std::vector<std::shared_ptr<Statement>> &GetStatementContext() const;
  int GetPlacementIdx() const;
  bool IsForceAvailOp() const;

 private:
  std::shared_ptr<Executable> target_;
  std::shared_ptr<TemplateTypeContext> template_type_context_;
  std::vector<std::shared_ptr<Statement>> statement_context_;
  int placement_idx_ = 0; // placement_idx == 0 simply means cannot utilize context as ref :)
  bool force_avail_op_ = false;
};
} // namespace seqgen

class TestCaseGenerator {
 public:
  TestCaseGenerator(std::shared_ptr<ClassType> cut, std::shared_ptr<ProgramContext> context);
  TestCase GenForMethod(const seqgen::GenTCForMethodSpec &target) const;
 private:
  std::shared_ptr<ClassType> cut_;
  std::shared_ptr<ProgramContext> context_;
};

class CreatorCyclicChecker {
 public:
  CreatorCyclicChecker();
  bool IsCyclic(const std::shared_ptr<Creator> &creator);
 private:
  std::multiset<std::shared_ptr<Creator>> used_;
  static int kCycleThreshold;
};

class OperandResolver {
 public:
  explicit OperandResolver(std::shared_ptr<ProgramContext> context);
  Operand ResolveOperand(const seqgen::ResolveOperandSpec &spec);
  std::vector<std::shared_ptr<Statement>> GetAssignableStatements(
    const TypeWithModifier &target_type,
    const std::shared_ptr<Statement> &op_stmt_ctx, // pass nullptr for non-mutation
    const TestCase &tc_ctx
  );
  bpstd::optional<Operand> ResolveUsingAssignableStatements(const seqgen::ResolveOperandSpec &spec);
  friend class STLOperandResolver;
 private:
  Operand ResolveOperandPrimitiveType(const seqgen::ResolveOperandSpec &spec);
  Operand ResolveOperandClassType(const seqgen::ResolveOperandSpec &spec);
  Operand ResolveOperandTemplateTypenameType(const seqgen::ResolveOperandSpec &spec);
  Operand ResolveOperandEnumType(const seqgen::ResolveOperandSpec &spec);
  Operand ResolveOperandTemplateTypenameSpcType(const seqgen::ResolveOperandSpec &spec);
  Operand ResolveOperandTemplateTypenameSpcTypeForSTL(const seqgen::ResolveOperandSpec &spec);
  std::shared_ptr<ProgramContext> context_;
};

class STLOperandResolver {
 public:
  explicit STLOperandResolver(OperandResolver &operand_resolver);
  Operand Handle(const seqgen::ResolveOperandSpec &spec);
 private:
  Operand ResolveRegularContainer(
    const seqgen::ResolveOperandSpec &spec,
    const std::shared_ptr<STLType> &stl_type,
    const std::vector<TemplateTypeInstantiation> &tt_insts
  );
  Operand ResolveArray(const seqgen::ResolveOperandSpec &spec, const std::vector<TemplateTypeInstantiation> &tt_insts);
  Operand ResolveKeyValueContainer(
    const seqgen::ResolveOperandSpec &spec,
    const std::shared_ptr<STLType> &stl_type,
    const std::vector<TemplateTypeInstantiation> &tt_insts
  );
  Operand ResolvePair(const seqgen::ResolveOperandSpec &spec, const std::vector<TemplateTypeInstantiation> &tt_insts);
  Operand ResolveTuple(
    const seqgen::ResolveOperandSpec &spec,
    const std::vector<TemplateTypeInstantiation> &tt_insts
  );
  Operand ResolveSmartPointer(
    const seqgen::ResolveOperandSpec &spec,
    const std::shared_ptr<STLType> &stl_type,
    const std::vector<TemplateTypeInstantiation> &tt_insts
  );
  Operand ResolveString(const seqgen::ResolveOperandSpec &spec, const std::vector<TemplateTypeInstantiation> &tt_insts);
 private:
  static const int kMaxElementsExclusive;
  static const int kMaxElementsForStringExclusive;
  OperandResolver &operand_resolver_;
};

}

#endif //CXXFOOZZ_INCLUDE_SEQUENCEGEN_HPP_
