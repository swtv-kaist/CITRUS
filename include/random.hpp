#ifndef CXXFOOZZ_INCLUDE_RANDOM_HPP_
#define CXXFOOZZ_INCLUDE_RANDOM_HPP_

#include <memory>
#include <random>
#include <set>

namespace uuid {
std::string generate_uuid_v4();
}

namespace cxxfoozz {

class Random {
 public:
  Random();
  static const std::shared_ptr<Random> &GetInstance();
  int NextInt();
  int NextInt(int bound);
  int NextInt(int start, int exclusiveMax);
  long long NextLong();
  bool NextBoolean();
  double NextDouble();
  double NextDouble(double min, double max);
  double NextGaussian();
  std::string NextString(int minLen = 0, int exclusiveMaxLen = 11);

  template<typename T>
  std::string NextIntGen();
  template<typename T>
  std::string NextRealGen();

 private:
  static int counter_;
  static std::shared_ptr<Random> instance_;
  std::default_random_engine engine_;

  static constexpr double kSpecialValueThreshold = 0.02;
  template<typename T>
  T GetSpecialValue();
};

template<typename T>
std::string Random::NextIntGen() {
  ++counter_;
  double gauss = NextGaussian();
  if (gauss < kSpecialValueThreshold)
    return std::to_string((T) GetSpecialValue<T>());

  bool is_unsigned = bpstd::is_unsigned_v<T>;
  if (is_unsigned) {
    std::uniform_int_distribution<T> dist(0, std::numeric_limits<unsigned char>::max());
    return std::to_string((T) dist(engine_));
  } else {
    std::uniform_int_distribution<T>
      dist(std::numeric_limits<signed char>::min(), std::numeric_limits<signed char>::max());
    return std::to_string((T) dist(engine_));
  }
}
template<typename T>
std::string Random::NextRealGen() {
  ++counter_;
  double gauss = NextGaussian();
  if (gauss < kSpecialValueThreshold)
    return std::to_string((T) GetSpecialValue<T>());

  std::uniform_real_distribution<T> dist;
  return std::to_string((T) dist(engine_));
}

template<typename T>
T Random::GetSpecialValue() {
  static std::array<T, 13> special_values{
    std::numeric_limits<T>::min(), std::numeric_limits<T>::max(),
    (T) -5, (T) -4, (T) -3, (T) -2, (T) -1, (T) 0, (T) 1, (T) 2, (T) 3, (T) 4, (T) 5,
  };
  int choice = NextInt(13);
  return special_values[choice];
}

} // namespace cxxfoozz

#endif //CXXFOOZZ_INCLUDE_RANDOM_HPP_
