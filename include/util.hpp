#ifndef CXXFOOZZ_INCLUDE_UTIL_HPP_
#define CXXFOOZZ_INCLUDE_UTIL_HPP_

#include <string>
#include <vector>

std::vector<std::string> SplitStringIntoVector(std::string target, const std::string &delimiter);
std::string StringJoin(const std::vector<std::string> &tokens, const std::string &delimiter = ", ");
std::string StringStrip(const std::string &input_string);

std::string ReplaceFirstOccurrence(std::string input, const std::string &keyword, const std::string &repl);

#endif //CXXFOOZZ_INCLUDE_UTIL_HPP_
