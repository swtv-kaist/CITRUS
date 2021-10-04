#ifndef CXXFOOZZ_STATEMENT_HPP
#define CXXFOOZZ_STATEMENT_HPP

#include <utility>

#include "model.hpp"
#include "type.hpp"
#include "program-context.hpp"

/**
 * Classes in this file are used for random method-call/statement sequence generation
 * IMPORTANT: Must utilize data fields defined in model.hpp
 */

namespace cxxfoozz {

enum class StatementVariant {
  kPrimitiveAssignment = 0,
  kCall,
  kSTLConstruction,
  kArrayInitialization,
};

class Operand;

class Statement {
 public:
  explicit Statement(const TypeWithModifier &type);
  virtual ~Statement();
  const TypeWithModifier &GetType() const;
  void SetType(const TypeWithModifier &type);
  const bpstd::optional<std::string> &GetVarName() const;
  void SetVarName(const bpstd::optional<std::string> &var_name);
  void ClearVarName();
  virtual std::shared_ptr<Statement> Clone() = 0;
  virtual StatementVariant GetVariant() const = 0;
  virtual std::pair<std::shared_ptr<Statement>, int> ReplaceRefOperand(
    const std::map<std::shared_ptr<Statement>, std::shared_ptr<Statement>> &repl_map,
    const std::shared_ptr<TemplateTypeContext> &tt_ctx
  ) = 0;
  virtual std::vector<Operand> GetStatementOperands() const = 0;

 private:
  TypeWithModifier type_;
  bpstd::optional<std::string> var_name_;
};

enum class OperandType {
  kConstantOperand = 0,
  kRefOperand,
};

class Operand {
 public:
  Operand(
    TypeWithModifier type,
    std::shared_ptr<Statement> ref,
    bpstd::optional<std::string> constant_literal
  );
  Operand(const Operand &other);
  Operand &operator=(const Operand &other);
  bool operator==(const Operand &rhs) const;
  bool operator!=(const Operand &rhs) const;
  static Operand MakeRefOperand(const std::shared_ptr<Statement> &ref);
  static Operand MakeConstantOperand(const TypeWithModifier &type, const std::string &literal);
  static Operand MakeBottom();
  const TypeWithModifier &GetType() const;
  const std::shared_ptr<Statement> &GetRef() const;
  const bpstd::optional<std::string> &GetConstantLiteral() const;
  OperandType GetOperandType() const;
  std::string ToStringWithAutoCasting(
    const TypeWithModifier &type_rq,
    const std::shared_ptr<InheritanceTreeModel> &itm
  ) const;
  std::string ToStringWithAutoCasting(
    const TypeWithModifier &type_rq,
    const std::shared_ptr<TemplateTypeContext> &tt_ctx,
    const std::shared_ptr<InheritanceTreeModel> &itm
  ) const;
  bool IsNullPtr() const;
//  static void SetKLibFuzzerMode(bool k_lib_fuzzer_mode);
  static bool IsKLibFuzzerMode();
  class LibFuzzerModeHacker {
   public:
    LibFuzzerModeHacker();
    virtual ~LibFuzzerModeHacker();
  };

 private:
  std::string InternalToString() const;
  TypeWithModifier type_;
  std::shared_ptr<Statement> ref_;
  bpstd::optional<std::string> constant_literal_;
  static bool kLibFuzzerMode;
};

enum class GeneralPrimitiveOp {
  kNop = 0,
  kAdd,
  kSub,
  kMul,
  kDiv,
  kMod,
  kMinus,
};

enum class OpArity {
  kUnary = 0,
  kBinary
};

class PrimitiveAssignmentStatement : public Statement {
 public:
  PrimitiveAssignmentStatement(
    const TypeWithModifier &type,
    GeneralPrimitiveOp op,
    std::vector<Operand> operands
  );
  virtual ~PrimitiveAssignmentStatement();
  static std::shared_ptr<Statement> MakeUnaryOpStatement(const Operand &op, GeneralPrimitiveOp o);
  static std::shared_ptr<Statement> MakeBinOpStatement(const Operand &op1, const Operand &op2, GeneralPrimitiveOp o);

  GeneralPrimitiveOp GetOp() const;
  std::vector<Operand> &GetOperands();
  void SetOp(GeneralPrimitiveOp op);
  void SetOperands(const std::vector<Operand> &operands);

  std::shared_ptr<Statement> Clone() override;
  StatementVariant GetVariant() const override;
  OpArity GetOpArity() const;
  std::pair<std::shared_ptr<Statement>, int> ReplaceRefOperand(
    const std::map<std::shared_ptr<Statement>, std::shared_ptr<Statement>> &repl_map,
    const std::shared_ptr<TemplateTypeContext> &tt_ctx
  ) override;
  std::vector<Operand> GetStatementOperands() const override;

 private:
  GeneralPrimitiveOp op_;
  std::vector<Operand> operands_;
};

class ArrayInitStatement : public Statement {
 public:
  ArrayInitStatement(
    const TypeWithModifier &type,
    const bpstd::optional<int> &capacity,
    bpstd::optional<Operand> string_literal,
    bpstd::optional<std::vector<Operand>> elements
  );
  static std::shared_ptr<Statement> MakeCString(const Operand &op);
  static std::shared_ptr<Statement> MakeArrayInitialization(
    const TypeWithModifier &target_type,
    const std::vector<Operand> &operands
  );
  std::shared_ptr<Statement> Clone() override;
  StatementVariant GetVariant() const override;
  std::pair<std::shared_ptr<Statement>, int> ReplaceRefOperand(
    const std::map<std::shared_ptr<Statement>, std::shared_ptr<Statement>> &repl_map,
    const std::shared_ptr<TemplateTypeContext> &tt_ctx
  ) override;
  std::vector<Operand> GetStatementOperands() const override;

  const bpstd::optional<int> &GetCapacity() const;
  const bpstd::optional<Operand> &GetStringLiteral() const;
  const bpstd::optional<std::vector<Operand>> &GetElements() const;

  void SetCapacity(const bpstd::optional<int> &capacity);
  void SetStringLiteral(const bpstd::optional<Operand> &string_literal);
  void SetElements(const bpstd::optional<std::vector<Operand>> &elements);
 private:
  bpstd::optional<int> capacity_;
  bpstd::optional<Operand> string_literal_; // for char[]
  bpstd::optional<std::vector<Operand>> elements_; // for other types
};

class CallStatement : public Statement {
 public:
  CallStatement(
    const TypeWithModifier &type,
    std::shared_ptr<Executable> target,
    std::vector<Operand> operands,
    bpstd::optional<Operand> invoking_obj,
    const std::shared_ptr<TemplateTypeContext> &template_type_context
  );
  static std::shared_ptr<Statement> MakeExecutableCall(
    const std::shared_ptr<Executable> &target,
    const std::vector<Operand> &ops,
    const bpstd::optional<Operand> &invoking_obj,
    const std::shared_ptr<TemplateTypeContext> &tt_ctx
  );
  const std::shared_ptr<Executable> &GetTarget() const;
  std::vector<Operand> &GetOperands();
  const bpstd::optional<Operand> &GetInvokingObj() const;
  const std::shared_ptr<TemplateTypeContext> &GetTemplateTypeContext() const;
  void SetTarget(const std::shared_ptr<Executable> &target);
  void SetOperands(const std::vector<Operand> &operands);
  void SetInvokingObj(const bpstd::optional<Operand> &invoking_obj);

  std::shared_ptr<Statement> Clone() override;
  StatementVariant GetVariant() const override;
  std::pair<std::shared_ptr<Statement>, int> ReplaceRefOperand(
    const std::map<std::shared_ptr<Statement>, std::shared_ptr<Statement>> &repl_map,
    const std::shared_ptr<TemplateTypeContext> &tt_ctx
  ) override;
  std::vector<Operand> GetStatementOperands() const override;

 private:
  std::shared_ptr<Executable> target_;
  std::vector<Operand> operands_;
  bpstd::optional<Operand> invoking_obj_;
  std::shared_ptr<TemplateTypeContext> template_type_context_;
};

class STLStatement;

class STLElement {
 public:
  STLElement(
    bpstd::optional<std::vector<Operand>> reg_container_elmts,
    bpstd::optional<std::vector<std::pair<Operand, Operand>>> key_value_elmts
  );
  static STLElement ForRegularContainer(const std::vector<Operand> &operands);
  static STLElement ForKeyValueContainer(const std::vector<std::pair<Operand, Operand>> &operands);
  const std::vector<Operand> &GetRegContainerElmts() const;
  const std::vector<std::pair<Operand, Operand>> &GetKeyValueElmts() const;
  bool IsKeyValueElements() const;
  bool IsRegContainerElements() const;
  friend class STLStatement;

 private:
  bpstd::optional<std::vector<Operand>> reg_container_elmts_;
  bpstd::optional<std::vector<std::pair<Operand, Operand>>> key_value_elmts_;
};

class STLStatement : public Statement {
 public:
  STLStatement(
    const TypeWithModifier &type,
    std::shared_ptr<STLType> target,
    STLElement elements
  );
  static std::shared_ptr<Statement> MakeSTLStatement(
    const TypeWithModifier &type,
    std::shared_ptr<STLType> target,
    STLElement elements
  );
  const std::shared_ptr<STLType> &GetTarget() const;
  const STLElement &GetElements() const;
  std::shared_ptr<Statement> Clone() override;
  StatementVariant GetVariant() const override;
  std::pair<std::shared_ptr<Statement>, int> ReplaceRefOperand(
    const std::map<std::shared_ptr<Statement>, std::shared_ptr<Statement>> &repl_map,
    const std::shared_ptr<TemplateTypeContext> &tt_ctx
  ) override;
  std::vector<Operand> GetStatementOperands() const override;
  void SetElements(const STLElement &elements);
 private:
  std::shared_ptr<STLType> target_;
  STLElement elements_;

};

class StatementWriter {
 public:
  explicit StatementWriter(const std::shared_ptr<ProgramContext> &context);
  std::string StmtAsString(const std::shared_ptr<Statement> &stmt, unsigned int stmt_id);
 private:
  std::string PrimitiveAssStmtAsString(const std::shared_ptr<PrimitiveAssignmentStatement> &stmt, unsigned int stmt_id);
  std::string CallStmtAsString(const std::shared_ptr<CallStatement> &stmt, unsigned int stmt_id);
  std::string ArrayInitStmtAsString(const std::shared_ptr<ArrayInitStatement> &stmt, unsigned int stmt_id);
 private:
  const std::shared_ptr<ProgramContext> &context_;
};

class STLStatementWriter {
 public:
  STLStatementWriter(const std::shared_ptr<ProgramContext> &context);
  std::string STLStmtAsString(
    const std::shared_ptr<STLStatement> &stmt,
    unsigned int stmt_id,
    const std::string &force_varname = ""
  );
 private:
  void HandleStackAndQueue(
    const TemplateTypeInstList &inst_list,
    const STLElement &stl_elements,
    std::stringstream &ss
  );
  void HandlePriorityQueue(
    const TemplateTypeInstList &inst_list,
    const STLElement &stl_elements,
    unsigned int stmt_id,
    std::stringstream &ss,
    std::stringstream &prelim_ss
  );
  void HandleStandardRegContainer(
    const TemplateTypeInstList &inst_list,
    const STLElement &stl_elements,
    std::stringstream &ss
  );
  void HandleArray(
    const TemplateTypeInstList &inst_list,
    const STLElement &stl_elements,
    std::stringstream &ss
  );
  void HandleKeyValueContainer(
    const TemplateTypeInstList &inst_list,
    const STLElement &stl_elements,
    std::stringstream &ss
  );
  void HandlePair(
    const TemplateTypeInstList &inst_list,
    const STLElement &stl_elements,
    std::stringstream &ss
  );
  void HandleTuple(
    const TemplateTypeInstList &inst_list,
    const STLElement &stl_elements,
    std::stringstream &ss
  );
  void HandleSmartPointer(
    const TemplateTypeInstList &inst_list,
    const STLElement &stl_elements,
    std::stringstream &ss
  );
  void HandleString(
    const TemplateTypeInstList &inst_list,
    const STLElement &stl_elements,
    std::stringstream &ss
  );
 private:
  const std::shared_ptr<ProgramContext> &context_;
};

} // namespace cxxfoozz



#endif //CXXFOOZZ_STATEMENT_HPP
