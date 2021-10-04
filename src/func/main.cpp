#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Sema/Sema.h"

#include "func/action.hpp"
#include <iostream>
#include <sstream>

llvm::cl::OptionCategory kOptionCategory("CXXFOOZZ Function Complexity Dependency Options");
static llvm::cl::opt<std::string> kOptOutputFilename(
  "out",
  llvm::cl::desc("Specify output filename."),
  llvm::cl::value_desc("string"),
  llvm::cl::Required,
  llvm::cl::cat(kOptionCategory));

int main(int argc, const char *argv[]) {

  clang::tooling::CommonOptionsParser parser{argc, argv, kOptionCategory};
  const std::vector<std::string> &sources = parser.getSourcePathList();
  clang::tooling::CompilationDatabase &database = parser.getCompilations();

  for (const auto &file : sources) {
    const std::vector<clang::tooling::CompileCommand> &compile_cmds = database.getCompileCommands(file);
    if (compile_cmds.size() > 1) {
      for (const auto &compile_cmd : compile_cmds) {
        std::stringstream ss;
        for (const auto &cmd : compile_cmd.CommandLine)
          ss << cmd << ' ';
        std::cout << ss.str() << '\n';
      }
    }

    //    const std::vector<std::string> &path = SplitStringIntoVector(file, "/");
    //    const std::basic_string<char> &filename = path.back();

    std::vector<std::string> source{file};
    clang::tooling::ClangTool tool{parser.getCompilations(), source,};

    // Hard-coded for now :)
    clang::tooling::ArgumentsAdjuster adjuster1 =
      clang::tooling::getInsertArgumentAdjuster("-I/usr/lib/gcc/x86_64-linux-gnu/10/include");
    clang::tooling::ArgumentsAdjuster adjuster2 =
      clang::tooling::getInsertArgumentAdjuster("-I/usr/lib/gcc/x86_64-linux-gnu/9/include");
    tool.appendArgumentsAdjuster(adjuster1);
    tool.appendArgumentsAdjuster(adjuster2);

    const std::unique_ptr<clang::tooling::FrontendActionFactory> &action_factory =
      clang::tooling::newFrontendActionFactory<FuncAnalysisAction>();
    tool.run(action_factory.get());
  }

  ExportSummary(kOptOutputFilename.getValue());
  PrintFunctionSizeAverage();
  printf("#Classes: %d, #Structs: %d\n", kClassCount, kStructCount);
  return 0;
}