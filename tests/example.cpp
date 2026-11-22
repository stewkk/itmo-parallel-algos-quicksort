#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
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

int64_t Up(std::span<int64_t> data, std::function<int64_t(int64_t)> pred, std::vector<int64_t>& result, int32_t v = 1) {
    if (data.empty()) {
        return 0;
    }
    if (data.size() == 1) {
        result[v] = pred(data.front());
        return result[v];
    }
    int32_t m = data.size() / 2;
    auto left = data.first(m);
    auto right = data.subspan(m);
    int64_t result_left, result_right;
    parlay::par_do(
        [&] { result_left = Up(left, pred, result, 2 * v); },
        [&] { result_right = Up(right, pred, result, 2 * v + 1); });
    result[v] = result_left + result_right;
    return result[v];
}

TEST(ScanTest, BuildSegmentTree) {
    std::vector<int64_t> example = {2, 3, 7, 6, 5, 4, 9};

    std::vector<int64_t> result(4*example.size());

    auto got = Up(example, [](int64_t x) { return x < 5 ? 1 : 0; }, result);

    std::vector<int64_t> expected = {0, 3, 2, 1, 1, 1, 0, 1,
                                    0, 0, 1, 0, 0, 0, 1, 0};
    expected.resize(result.size());

    ASSERT_EQ(result, expected);
    ASSERT_EQ(got, 3);
}

void Down(std::span<int64_t> p, const std::vector<int64_t>& b, int64_t left = 0, int32_t v = 1) {
    if (p.empty()) {
        return;
    }
    if (p.size() == 1) {
        if (b[v] == 0) {
            p.front() = -1;
            return;
        }
        p.front() = left;
        return;
    }
    int m = p.size() / 2;
    parlay::par_do([&] {
        Down(p.first(m), b, left, 2*v);
    }, [&] {
        Down(p.subspan(m), b, left + b[2*v], 2*v+1);
    });
}

TEST(ScanTest, FindPrefsums) {
    std::vector<int64_t> example = {2, 3, 7, 6, 5, 4, 9, 1};
    std::vector<int64_t> segtree(4*example.size());
    Up(example, [](int64_t x) { return x < 5 ? 1 : 0; }, segtree);

    std::vector<int64_t> prefsum(example.size());
    Down(prefsum, segtree);

    const std::vector<int64_t> expected = {0, 1, -1, -1, -1, 2, -1, 3};
    ASSERT_EQ(prefsum, expected);
}

std::vector<int64_t> Scan(std::span<int64_t> data, std::function<int64_t(int64_t)> pred) {
    std::vector<int64_t> segtree(4*data.size());
    Up(data, pred, segtree);
    std::vector<int64_t> prefsum(data.size());
    Down(prefsum, segtree);
    return prefsum;
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

  auto left = Scan(data, [pivot_value](int64_t x) { return x < pivot_value ? 1 : 0; });
  auto middle = Scan(data, [pivot_value](int64_t x) { return x == pivot_value ? 1 : 0; });
  auto right = Scan(data, [pivot_value](int64_t x) { return x > pivot_value ? 1 : 0; });



  parlay::par_do([data, prefix] { QuicksortParallel(data.first(prefix)); },
                 [data, suffix] { QuicksortParallel(data.subspan(suffix)); });
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

TEST(QuicksortBenchTest, DISABLED_Main) {
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
