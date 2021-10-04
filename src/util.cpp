#include "util.hpp"
#include <sstream>

std::vector<std::string> SplitStringIntoVector(std::string target, const std::string &delimiter) {
  size_t pos = 0;
  std::vector<std::string> result;

  while ((pos = target.find(delimiter)) != std::string::npos) {
    const auto &token = target.substr(0, pos);
    result.push_back(token);
    target.erase(0, pos + delimiter.length());
  }
  result.push_back(target);
  return result;
}

std::string StringJoin(const std::vector<std::string> &tokens, const std::string &delimiter) {
  std::stringstream ss;
  bool first_elmt = true;
  for (const auto &item : tokens) {
    ss << (first_elmt ? "" : delimiter) << item;
    first_elmt = false;
  }
  return ss.str();
}

std::string StringStrip(const std::string &input_string) {
  if (input_string.empty())
    return input_string;
  auto start_it = input_string.begin();
  auto end_it = input_string.rbegin();
  while (std::isspace(*start_it))
    ++start_it;
  while (std::isspace(*end_it))
    ++end_it;
  return std::string(start_it, end_it.base());
}

std::string ReplaceFirstOccurrence(std::string input, const std::string &keyword, const std::string &repl) {
  std::size_t pos = input.find(keyword);
  if (pos == std::string::npos)
    return input;
  return input.replace(pos, keyword.length(), repl);
}