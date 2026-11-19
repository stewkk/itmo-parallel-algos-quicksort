#include <algorithm>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

#include <gtest/gtest.h>

std::mt19937 rng(42);

void QuicksortSequential(std::span<int64_t> data) {
  if (data.size() <= 1) {
    return;
  }
  auto pivot_index =
      std::uniform_int_distribution<std::int64_t>(0, data.size() - 1)(rng);
  auto pivot_value = data[pivot_index];

  size_t prefix = 0;
  for (int i = 0; i < data.size(); i++) {
    if (data[i] < pivot_value) {
      std::swap(data[i], data[prefix]);
      prefix++;
    }
  }

  size_t suffix = data.size() - 1;
  for (int i = data.size() - 1; i >= 0; i--) {
    if (data[i] > pivot_value) {
      std::swap(data[i], data[suffix]);
      suffix--;
    }
  }

  QuicksortSequential(data.first(prefix));
  QuicksortSequential(data.subspan(prefix));
}

TEST(QuicksortTest, Sequential) {
  std::vector<std::int64_t> data(10);
  std::uniform_int_distribution<std::int64_t> distribution(-100, 100);
  for (auto &value : data) {
    value = distribution(rng);
  }
  auto expected = data;
  std::sort(expected.begin(), expected.end());

  QuicksortSequential(data);

  ASSERT_EQ(data, expected);
}
