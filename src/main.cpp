#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Sema/Sema.h"
#include <experimental/filesystem>

#include "cli.hpp"
#include "logger.hpp"
#include "traversal.hpp"
#include "util.hpp"

#include <sstream>
#include <iostream>

int main(int argc, const char *argv[]) {
  cxxfoozz::CLIArgumentParser argument_parser{argc, argv};
  const cxxfoozz::CLIParsedArgs &parsed_args = argument_parser.ParseProgramOpt();
  const std::shared_ptr<cxxfoozz::CLIParsedArgs> &ptr_parsed_args =
    std::make_shared<cxxfoozz::CLIParsedArgs>(parsed_args);
  cxxfoozz::MainFuzzingAction::SetCLIArgs(ptr_parsed_args);

  clang::tooling::CommonOptionsParser &parser = argument_parser.GetClangToolingParser();
  const std::vector<std::string> &sources = parser.getSourcePathList();
  clang::tooling::CompilationDatabase &database = parser.getCompilations();

  for (const auto &file : sources) {
    const std::vector<clang::tooling::CompileCommand> &compile_cmds = database.getCompileCommands(file);
    if (compile_cmds.size() > 1) {
      cxxfoozz::Logger::Warn("File has > 1 compile commands: " + file);
      for (const auto &compile_cmd : compile_cmds) {
        std::stringstream ss;
        for (const auto &cmd : compile_cmd.CommandLine)
          ss << cmd << ' ';
        std::cout << ss.str() << '\n';
      }
//      continue;
    }
    cxxfoozz::MainFuzzingAction::SetCompileCmds(compile_cmds);

    const std::vector<std::string> &path = SplitStringIntoVector(file, "/");
    const std::basic_string<char> &filename = path.back();

    std::vector<std::string> source{file};
    clang::tooling::ClangTool tool{parser.getCompilations(), source,};

    const auto &dir_it = std::experimental::filesystem::directory_iterator("/usr/lib/gcc/x86_64-linux-gnu/");
    for (const auto &entry : dir_it) {
      const char *filepath = entry.path().c_str();
      std::string add_flag = "-I";
      add_flag += filepath;
      add_flag += "/include";
      clang::tooling::ArgumentsAdjuster arg_adjuster = clang::tooling::getInsertArgumentAdjuster(add_flag.c_str());
      tool.appendArgumentsAdjuster(arg_adjuster);
    }

    const std::unique_ptr<clang::tooling::FrontendActionFactory> &action_factory =
      clang::tooling::newFrontendActionFactory<cxxfoozz::MainFuzzingAction>();
    tool.run(action_factory.get());
  }

  return 0;
}

