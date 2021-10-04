#ifndef CXXFOOZZ_INCLUDE_LOGGER_HPP_
#define CXXFOOZZ_INCLUDE_LOGGER_HPP_

#include <string>
#include <vector>
#include "fuzzer.hpp"

namespace cxxfoozz {
class Logger {
 public:
  static void Error(const std::string &tag, const std::string &msg, bool recover = false);
  static void Error(const std::string &msg, bool recover = false);
  static void Debug(const std::string &tag, const std::string &msg);
  static void Debug(const std::string &msg);
  static void Info(const std::string &tag, const std::string &msg);
  static void Info(const std::string &msg);
  static void Warn(const std::string &tag, const std::string &msg);
  static void Warn(const std::string &msg);
  static long long int GetMessageId();
  static void InfoSection(const std::string &message);
 private:
  static bool debug_mode;
  static long long message_id;
};

class CoverageLoggingEntry {
 public:
  CoverageLoggingEntry(
    long long int timestamp,
    int line_cov,
    int branch_cov,
    int line_tot,
    int branch_tot,
    int func_cov,
    int func_tot
  );
  int GetLineCov() const;
  int GetBranchCov() const;
  int GetLineTot() const;
  int GetBranchTot() const;
  int GetFuncCov() const;
  int GetFuncTot() const;
  long long int GetTimestamp() const;
  std::tuple<double, double, double> GetCoverage() const;
  std::string ToPrettyString() const;
  std::string ToString(char sep) const;
 private:
  int line_cov_, branch_cov_, line_tot_, branch_tot_, func_cov_, func_tot_;
  long long int timestamp_;
};

class CoverageLogger {
 public:
  const std::vector<CoverageLoggingEntry> &GetEntries() const;
  void AppendEntry(
    long long int timestamp,
    int line_cov,
    int branch_cov,
    int line_tot,
    int branch_tot,
    int func_cov,
    int func_tot
  );
  void PrintSummary();
  void PrintForPlotting(
    const std::string &output_dir,
    long long int max_timestamp_in_sec,
    int time_gap,
    TestCaseQueue &queue
  );

 private:
  std::vector<CoverageLoggingEntry> entries_;
};
} // namespace cxxfoozz


#endif //CXXFOOZZ_INCLUDE_LOGGER_HPP_
