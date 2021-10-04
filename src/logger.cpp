#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>

#include "logger.hpp"
#include "random.hpp"

namespace cxxfoozz {

bool Logger::debug_mode = true;
long long Logger::message_id = 0ll;
void Logger::Error(const std::string &msg, bool recover) {
  std::cerr << "[ERROR] " << msg << '\n';
  if (!recover)
    exit(1);
}
void Logger::Debug(const std::string &msg) {
  if (debug_mode) {
    std::clog << '[' << message_id << ']' << "[DEBUG] " << msg << '\n';
    ++message_id;
  }
}
void Logger::Info(const std::string &msg) {
  std::cout << "[INFO] " << msg << '\n';
}
void Logger::Warn(const std::string &msg) {
  std::clog << "[WARNING] " << msg << '\n';
}
void Logger::Error(const std::string &tag, const std::string &msg, bool recover) {
  Error(tag + ' ' + msg, recover);
}
void Logger::Debug(const std::string &tag, const std::string &msg) {
  Debug(tag + ' ' + msg);
}
void Logger::Info(const std::string &tag, const std::string &msg) {
  Info(tag + ' ' + msg);
}
void Logger::Warn(const std::string &tag, const std::string &msg) {
  Warn(tag + ' ' + msg);
}
long long int Logger::GetMessageId() {
  return message_id;
}
void Logger::InfoSection(const std::string &message) {
  Info("");
  Info(" ##########");
  Info(" # " + message);
  Info(" #####");
  Info("");
}

// ##########
// # CoverageLoggingEntry
// #####

CoverageLoggingEntry::CoverageLoggingEntry(
  long long int timestamp,
  int line_cov,
  int branch_cov,
  int line_tot,
  int branch_tot,
  int func_cov,
  int func_tot
)
  : line_cov_(line_cov),
    branch_cov_(branch_cov),
    line_tot_(line_tot),
    branch_tot_(branch_tot),
    timestamp_(timestamp),
    func_cov_(func_cov),
    func_tot_(func_tot) {}
int CoverageLoggingEntry::GetLineCov() const {
  return line_cov_;
}
int CoverageLoggingEntry::GetBranchCov() const {
  return branch_cov_;
}
int CoverageLoggingEntry::GetLineTot() const {
  return line_tot_;
}
int CoverageLoggingEntry::GetBranchTot() const {
  return branch_tot_;
}
long long int CoverageLoggingEntry::GetTimestamp() const {
  return timestamp_;
}
std::tuple<double, double, double> CoverageLoggingEntry::GetCoverage() const {
  double lcov = line_tot_ != 0 ? 100.0 * line_cov_ / line_tot_ : 0.0;
  double bcov = branch_tot_ != 0 ? 100.0 * branch_cov_ / branch_tot_ : 0.0;
  double fcov = func_tot_ != 0 ? 100.0 * func_cov_ / func_tot_ : 0.0;
  return std::make_tuple(lcov, bcov, fcov);
}
std::string CoverageLoggingEntry::ToPrettyString() const {
  const std::tuple<double, double, double> &cov = GetCoverage();
  std::stringstream ss;
  ss << "L: " << line_cov_ << '/' << line_tot_ << ", "
     << "B: " << branch_cov_ << '/' << branch_tot_ << ", "
     << "F: " << func_cov_ << '/' << func_tot_ << ", "
     << "%: " << std::get<0>(cov) << ", " << std::get<1>(cov) << ", " << std::get<2>(cov);
  return ss.str();
}
std::string CoverageLoggingEntry::ToString(char sep) const {
  const std::tuple<double, double, double> &cov = GetCoverage();
  std::stringstream ss;
  ss << line_cov_ << sep << line_tot_ << sep
     << branch_cov_ << sep << branch_tot_ << sep
     << func_cov_ << sep << func_tot_ << sep
     << std::get<0>(cov) << sep << std::get<1>(cov) << sep << std::get<2>(cov);
  return ss.str();
}
int CoverageLoggingEntry::GetFuncCov() const {
  return func_cov_;
}
int CoverageLoggingEntry::GetFuncTot() const {
  return func_tot_;
}

// ##########
// # CoverageLogger
// #####

const std::vector<CoverageLoggingEntry> &CoverageLogger::GetEntries() const {
  return entries_;
}
void CoverageLogger::AppendEntry(
  long long int timestamp,
  int line_cov,
  int branch_cov,
  int line_tot,
  int branch_tot,
  int func_cov,
  int func_tot
) {
  entries_.emplace_back(timestamp, line_cov, branch_cov, line_tot, branch_tot, func_cov, func_tot);
}

void CoverageLogger::PrintSummary() {
  for (const auto &entry : entries_) {
    Logger::Info(std::to_string(entry.GetTimestamp()), entry.ToPrettyString());
  }
}
void CoverageLogger::PrintForPlotting(
  const std::string &output_dir,
  long long int max_timestamp_in_sec,
  int time_gap,
  TestCaseQueue &queue
) {
  const std::string &filename = output_dir + "/out_report.csv";
  if (std::ofstream target{filename}) {
    target << "time, line, linetot, branch, branchtot, func, functot, linecov, branchcov, funccov\n";
    for (const auto &entry : entries_) {
      target << std::to_string(entry.GetTimestamp()) << ',' << entry.ToString(',') << '\n';
    }
    if (!entries_.empty()) {
      const CoverageLoggingEntry &last = entries_[entries_.size() - 1];
      target << std::to_string(max_timestamp_in_sec) << ',' << last.ToString(',') << '\n';
    }
    target << "valid, crash, uncompilable\n";
    target << queue.GetValid().size() << ',' << queue.GetCrashes().size() << ',' << queue.GetIncompilable().size()
           << '\n';
  }
}

//    int idx = 0, len = (int) entries_.size();
//    for (long long it = 0; it <= max_timeout && idx < len; it += time_gap) {
//      long long int ts = entries_[idx].GetTimestamp();
//      if (it > ts) {
//        if (idx == 0) target << ts << ',' << "0,0,0,0,0,0\n";
//        else {
//          target << ts << ',' << entries_[idx].ToString(',') << '\n';
//        }
//      } else {
//        long long int max_ts = it + time_gap;
//        while (idx < len) {
//          long long int tmp_ts = entries_[idx].GetTimestamp();
//          if (tmp_ts >= max_ts) break;
//          else idx++;
//        } // idx >= len || tmp_ts >= max_ts
//
//        if (idx < len) {
//          target << ts << ',' << entries_[idx].ToString(',') << '\n';
//          ++idx;
//        }
//      }
//    }
};