#include "logger.hpp"
#include "random.hpp"

#include <cassert>
#include <sstream>

namespace cxxfoozz {

const int kMaxUses = 100000;
int Random::counter_ = 0;
std::shared_ptr<Random> Random::instance_ = nullptr;

int Random::NextInt() {
  ++counter_;
  std::uniform_int_distribution<int> dist;
  return dist(engine_);
}
int Random::NextInt(int bound) {
  assert(bound > 0);
  ++counter_;
  std::uniform_int_distribution<int> dist(0, bound - 1);
  return dist(engine_);
}
int Random::NextInt(int start, int exclusiveMax) {
  assert(exclusiveMax > start);
  ++counter_;
  std::uniform_int_distribution<int> dist(start, exclusiveMax - 1);
  return dist(engine_);
}
long long Random::NextLong() {
  ++counter_;
  std::uniform_int_distribution<long long> dist;
  return dist(engine_);
}
bool Random::NextBoolean() {
  ++counter_;
  std::uniform_int_distribution<int> dist(0, 1);
  return dist(engine_) == 1;
}
double Random::NextDouble() {
  ++counter_;
  std::uniform_real_distribution<double> dist;
  return dist(engine_);
}
double Random::NextDouble(double min, double max) {
  ++counter_;
  std::uniform_real_distribution<double> dist(min, max);
  return dist(engine_);
}
double Random::NextGaussian() {
  ++counter_;
  std::uniform_real_distribution<double> dist(0.0, 1.0);
  return dist(engine_);
}
Random::Random() {
  unsigned int seed = std::random_device()();
//  seed = 498553585;
  Logger::Info("Just in case of emergency, here's your reproducible seed: " + std::to_string(seed));
  engine_ = std::default_random_engine(seed);
}
const std::shared_ptr<Random> &Random::GetInstance() {
  if (instance_ == nullptr || counter_ > kMaxUses) {
    instance_ = std::make_shared<Random>();
    counter_ = 0;
  }
  return instance_;
}
std::string Random::NextString(int minLen, int exclusiveMaxLen) {
  assert(minLen < exclusiveMaxLen);
  static const char alphanum[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";
  static const size_t alphanum_len = sizeof(alphanum) - 1;
  std::stringstream ss;
  int length = NextInt(minLen, exclusiveMaxLen);
  for (int i = 0; i < length; i++) {
    int choice_idx = NextInt(alphanum_len);
    ss << alphanum[choice_idx];
  }
  return ss.str();
}

} // namespace cxxfoozz

static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_int_distribution<> dis(0, 15);
static std::uniform_int_distribution<> dis2(8, 11);

std::string uuid::generate_uuid_v4() {
  std::stringstream ss;
  int i;
  ss << std::hex;
  for (i = 0; i < 8; i++) ss << dis(gen);
  ss << "-";
  for (i = 0; i < 4; i++) ss << dis(gen);
  ss << "-4";
  for (i = 0; i < 3; i++) ss << dis(gen);
  ss << "-";
  ss << dis2(gen);
  for (i = 0; i < 3; i++) ss << dis(gen);
  ss << "-";
  for (i = 0; i < 12; i++) ss << dis(gen);
  return ss.str();
}
