#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <span>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include <parlay/parallel.h>

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
  QuicksortSequential(data.subspan(suffix));
}

void QuicksortParallel(std::span<int64_t> data) {
  if (data.size() <= 1000) {
      QuicksortSequential(data);
      return;
  }
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

  parlay::par_do([&] { QuicksortSequential(data.first(prefix)); },
                 [&] { QuicksortSequential(data.subspan(suffix)); });
}

template <typename Sort>
class QuicksortTest : public ::testing::Test {};

template <auto... Sorts>
using SortFunctions =
    ::testing::Types<std::integral_constant<decltype(Sorts), Sorts>...>;

using QuicksortImplementations =
    SortFunctions<QuicksortSequential, QuicksortParallel>;
TYPED_TEST_SUITE(QuicksortTest, QuicksortImplementations);

TYPED_TEST(QuicksortTest, Stress) {
  std::mt19937 input_rng(42);
  std::uniform_int_distribution<std::int64_t> distribution(-100, 100);
  for (int it = 0; it < 1000; it++) {
    std::vector<std::int64_t> data(distribution(input_rng) + 100);
    for (auto &value : data) {
      value = distribution(input_rng);
    }
    auto expected = data;
    std::sort(expected.begin(), expected.end());

    TypeParam::value(data);

    ASSERT_EQ(data, expected);
  }
}

TYPED_TEST(QuicksortTest, Empty) {
  std::vector<std::int64_t> data(0);
  auto expected = data;

  TypeParam::value(data);

  ASSERT_EQ(data, expected);
}

TYPED_TEST(QuicksortTest, OneElement) {
  std::vector<std::int64_t> data(1, 42);
  auto expected = data;

  TypeParam::value(data);

  ASSERT_EQ(data, expected);
}

TEST(QuicksortBenchTest, Main) {
  using Clock = std::chrono::steady_clock;
  using Milliseconds = std::chrono::duration<double, std::milli>;
  constexpr int runs = 5;
  Milliseconds sequential_total{};
  Milliseconds parallel_total{};

  std::mt19937 input_rng(42);
  std::uniform_int_distribution<std::int64_t> distribution(-1000000, 1000000);

  std::vector<std::int64_t> data(10000000);
  for (auto &value : data) {
    value = distribution(input_rng);
  }
  for (int it = 0; it < runs; it++) {
    auto copy1 = data;
    auto copy2 = data;

    const auto sequential_start = Clock::now();
    QuicksortSequential(copy1);
    const Milliseconds sequential_time = Clock::now() - sequential_start;

    const auto parallel_start = Clock::now();
    QuicksortParallel(copy2);
    const Milliseconds parallel_time = Clock::now() - parallel_start;

    sequential_total += sequential_time;
    parallel_total += parallel_time;
    std::cout << "Run " << it + 1 << ": sequential " << sequential_time.count()
              << " ms, parallel " << parallel_time.count() << " ms\n";
  }

  std::cout << "Average: sequential " << sequential_total.count() / runs
            << " ms, parallel " << parallel_total.count() / runs << " ms\n";
}
