#include "clock.hpp"
#include "logger.hpp"

#include <ctime>
#include <utility>

namespace cxxfoozz {

// ##########
// # DebuggingClock
// #####

DebuggingClock::DebuggingClock() : DebuggingClock(false, "") {}
DebuggingClock::DebuggingClock(bool logging, std::string log_message)
  : logging_(logging), log_message_(std::move(log_message)) {}

// ##########
// # CPUClock
// #####

CPUClock::CPUClock() : DebuggingClock(), start_(std::clock()) {}
CPUClock::~CPUClock() {
  if (logging_) {
    long long int msec = MeasureElapsedInMsec();
    Logger::Info("[CPUClock]", log_message_ + " : " + std::to_string(msec) + " msec.");
  }
}
long long int CPUClock::MeasureElapsedInMsec() const {
  clock_t now = std::clock();
  clock_t diff = now - start_;
  return 1000ll * diff / CLOCKS_PER_SEC;
}
CPUClock CPUClock::ForLogging(const std::string &message) {
  CPUClock cpu_clock;
  cpu_clock.logging_ = true;
  cpu_clock.log_message_ = message;
  return cpu_clock;
}

// ##########
// # WallClock
// #####

WallClock::WallClock() : DebuggingClock(), start_(std::chrono::steady_clock::now()) {}
WallClock::~WallClock() {
  if (logging_) {
    long long int msec = MeasureElapsedInMsec();
    Logger::Info("[WallClock]", log_message_ + " : " + std::to_string(msec) + " msec.");
  }
}
long long int WallClock::MeasureElapsedInMsec() const {
  const auto now = std::chrono::steady_clock::now();
  auto diff = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - start_);
  return (long long int) diff.count();
}
WallClock WallClock::ForLogging(const std::string &message) {
  WallClock wall_clock;
  wall_clock.logging_ = true;
  wall_clock.log_message_ = message;
  return wall_clock;
}
long long int WallClock::GetCurrentMillis() {
  const auto now = std::chrono::system_clock::now();
  const auto the1970 = std::chrono::system_clock::from_time_t(0);
  auto diff = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - the1970);
  return (long long int) diff.count();
}

} // namespace cxxfoozz