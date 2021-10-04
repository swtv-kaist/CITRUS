#ifndef CXXFOOZZ_SRC_CPUCLOCK_HPP_
#define CXXFOOZZ_SRC_CPUCLOCK_HPP_

#include <chrono>
#include <ctime>
#include <string>

namespace cxxfoozz {

class DebuggingClock {
 protected:
  DebuggingClock();
  DebuggingClock(bool logging, std::string log_message);
 protected:
  bool logging_ = false;
  std::string log_message_;
};

class CPUClock : public DebuggingClock {
 public:
  CPUClock();
  ~CPUClock();
  static CPUClock ForLogging(const std::string& message);
  long long int MeasureElapsedInMsec() const;
 private:
  clock_t start_;
};

class WallClock : public DebuggingClock {
 public:
  WallClock();
  ~WallClock();
  static WallClock ForLogging(const std::string& message);
  long long int MeasureElapsedInMsec() const;
  static long long int GetCurrentMillis();
 private:
  std::chrono::steady_clock::time_point start_;
};
} // namespace cxxfoozz

#endif //CXXFOOZZ_SRC_CPUCLOCK_HPP_
