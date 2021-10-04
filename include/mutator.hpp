#ifndef CXXFOOZZ_INCLUDE_MUTATOR_HPP_
#define CXXFOOZZ_INCLUDE_MUTATOR_HPP_

#include "sequencegen.hpp"
#include "statement.hpp"
#include "type.hpp"
#include "program-context.hpp"

namespace cxxfoozz {

class OperandMutator {
 public:
  explicit OperandMutator(const std::shared_ptr<ProgramContext> &context);
  Operand MutateOperand(
    const Operand &op,
    const std::shared_ptr<Statement> &op_stmt_ctx,
    const TestCase &tc_ctx,
    const bpstd::optional<TypeWithModifier> &type_rq = bpstd::nullopt // required for cross-variant
  );
  Operand MutateConstantOperand(const Operand &op);
  Operand MutateConstantOperand(const TypeWithModifier &type);
  Operand MutateRefOperand(
    const Operand &op,
    const std::shared_ptr<Statement> &op_stmt_ctx,
    const TestCase &tc_ctx,
    const bpstd::optional<TypeWithModifier> &type_rq
  );
  Operand MutateOperandCrossVariant(
    const Operand &op,
    const std::shared_ptr<Statement> &op_stmt_ctx,
    const TestCase &tc_ctx,
    const bpstd::optional<TypeWithModifier> &type_rq
  );
 private:
  const std::shared_ptr<ProgramContext> &context_;
};

class StatementMutator {
 public:
  StatementMutator(std::shared_ptr<ClassType> cut, std::shared_ptr<ProgramContext> context);
  std::shared_ptr<Statement> MutateStatement(const std::shared_ptr<Statement> &stmt, const TestCase &tc_ctx);
  std::shared_ptr<Statement> MutatePrimitiveAssignment(const std::shared_ptr<Statement> &stmt, const TestCase &tc_ctx);
  std::shared_ptr<Statement> MutateCall(const std::shared_ptr<Statement> &stmt, const TestCase &tc_ctx);
  std::shared_ptr<Statement> MutateSTLConstruction(const std::shared_ptr<Statement> &stmt, const TestCase &tc_ctx);
  std::shared_ptr<Statement> MutateArrayInit(const std::shared_ptr<Statement> &stmt, const TestCase &tc_ctx);
  const std::shared_ptr<ClassType> &GetCut() const;
  const std::shared_ptr<ProgramContext> &GetContext() const;
 private:
  std::shared_ptr<ClassType> cut_;
  std::shared_ptr<ProgramContext> context_;
};

class TestCaseMutator {
 public:
  TestCaseMutator(std::shared_ptr<ClassType> cut, std::shared_ptr<ProgramContext> context);
  TestCase MutateTestCase(const TestCase &tc, int maxHavoc);
  void InplaceMutationByInsertion(TestCase &tc);
  void InplaceMutationByUpdate(TestCase &tc);
  void InplaceMutationByCleanup(TestCase &tc);
  const std::shared_ptr<ClassType> &GetCut() const;
  const std::shared_ptr<ProgramContext> &GetContext() const;
 private:
  std::shared_ptr<ClassType> cut_;
  std::shared_ptr<ProgramContext> context_;
};

} // namespace cxxfoozz

#endif //CXXFOOZZ_INCLUDE_MUTATOR_HPP_
