

#ifndef CXXFOOZZ_INCLUDE_FUNCTION_SELECTOR_HPP_
#define CXXFOOZZ_INCLUDE_FUNCTION_SELECTOR_HPP_

#include <vector>
#include <memory>
#include "model.hpp"

namespace cxxfoozz {

enum class FunctionSelectorMode {
  kRandom = 0,
  kComplexityBased
};

class FunctionSelector {
 public:
  FunctionSelector(std::vector<std::shared_ptr<Executable>> executables, FunctionSelectorMode mode);
  std::shared_ptr<Executable> NextExecutable();

 private:
  std::vector<std::shared_ptr<Executable>> executables_;
  FunctionSelectorMode mode_;
};

}

#endif //CXXFOOZZ_INCLUDE_FUNCTION_SELECTOR_HPP_
