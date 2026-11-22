#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <parlay/parallel.h>

constexpr size_t kBlockSize = 2048;
constexpr auto kSequentialFallback = 262144;

void QuicksortSequential(std::span<int32_t> data) {
  if (data.size() <= 1) {
    return;
  }
  auto pivot_value = data[data.size() / 2];

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
  QuicksortSequential(data.subspan(suffix + 1));
}

int32_t Up(std::span<int32_t> data, std::function<int32_t(int32_t)> pred, std::vector<int32_t>& result, int32_t v = 1) {
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
    int32_t result_left, result_right;
    parlay::par_do(
        [&] { result_left = Up(left, pred, result, 2 * v); },
        [&] { result_right = Up(right, pred, result, 2 * v + 1); });
    result[v] = result_left + result_right;
    return result[v];
}

TEST(ScanTest, BuildSegmentTree) {
    std::vector<int32_t> example = {2, 3, 7, 6, 5, 4, 9};

    std::vector<int32_t> result(4*example.size());

    auto got = Up(example, [](int32_t x) { return x < 5; }, result);

    std::vector<int32_t> expected = {0, 3, 2, 1, 1, 1, 0, 1,
                                    0, 0, 1, 0, 0, 0, 1, 0};
    expected.resize(result.size());

    ASSERT_EQ(result, expected);
    ASSERT_EQ(got, 3);
}

void Down(std::span<int32_t> p, const std::vector<int32_t>& b, int32_t offset, int32_t left = 0, int32_t v = 1) {
    if (p.empty()) {
        return;
    }
    if (p.size() == 1) {
        if (b[v] == 0) {
            return;
        }
        p.front() = left + offset;
        return;
    }
    int m = p.size() / 2;
    parlay::par_do([&] {
        Down(p.first(m), b, offset, left, 2*v);
    }, [&] {
        Down(p.subspan(m), b, offset, left + b[2*v], 2*v+1);
    });
}

TEST(ScanTest, FindPrefsums) {
    std::vector<int32_t> example = {2, 3, 7, 6, 5, 4, 9, 1};
    std::vector<int32_t> segtree(4*example.size());
    Up(example, [](int32_t x) { return x < 5; }, segtree);

    std::vector<int32_t> prefsum(example.size());
    Down(prefsum, segtree, 0);

    const std::vector<int32_t> expected = {0, 1, 0, 0, 0, 2, 0, 3};
    ASSERT_EQ(prefsum, expected);
}

template <typename Func>
void Map(std::span<int32_t> target, Func func, int l = 0,
         size_t block_size = kBlockSize) {
    if (target.size() <= block_size) {
        for (auto [i, _] : target | std::views::enumerate) {
            func(l+i);
        }
        return;
    }
    int32_t m = target.size() / 2;
    parlay::par_do([=] {
        Map(target.first(m), func, l, block_size);
    }, [=] {
        Map(target.subspan(m), func, l+m, block_size);
    });
}

TEST(ScanTest, Map) {
    std::vector<int32_t> arr(4);
    std::vector<int32_t> expected = {0, 1, 2, 3};

    Map(arr, [&](int32_t i) {
        arr[i] = i;
    });

    ASSERT_EQ(arr, expected);
}

void QuicksortParallel(std::span<int32_t> data) {
  if (data.size() <= kSequentialFallback) {
      QuicksortSequential(data);
      return;
  }
  auto pivot_value = data[data.size() / 2];
  auto left = 0, right = 0;
  {
    auto blocks = (data.size() + kBlockSize - 1) / kBlockSize;
    std::vector<int32_t> less(blocks), greater(blocks);
    std::vector<int32_t> scratch(data.size());
    Map(less, [&](int32_t block) {
      auto l = block * kBlockSize;
      auto r = std::min(l + kBlockSize, data.size());
      auto min = l, max = r;
      for (auto i = l; i < r; ++i) {
        auto value = data[i];
        scratch[min] = value;
        scratch[max - 1] = value;
        min += value < pivot_value;
        max -= value > pivot_value;
      }
      less[block] = min - l;
      greater[block] = r - max;
    }, 0, 1);

    for (size_t b = 0; b < blocks; ++b) {
      left += less[b];
      right += greater[b];
      less[b] = left;
      greater[b] = right;
    }
    Map(less, [&](int32_t b) {
      auto prev_less = 0, prev_greater = 0;
      if (b > 0) {
        prev_less = less[b - 1];
        prev_greater = greater[b - 1];
      }
      auto l = b * kBlockSize;
      auto r = std::min(l + kBlockSize, data.size());
      for (auto i = prev_less; i < less[b]; ++i) {
        data[i] = scratch[l + (i - prev_less)];
      }
      for (auto i = prev_greater; i < greater[b]; ++i) {
        data[data.size() - right + i] = scratch[r - (greater[b] - i)];
      }
      for (auto i = l - prev_less - prev_greater; i < r - less[b] - greater[b]; ++i) {
        data[left + i] = pivot_value;
      }
    }, 0, 1);
  }
  auto mid = data.size() - left - right;

  parlay::par_do([data, left] { QuicksortParallel(data.first(left)); },
                 [data, left, mid] { QuicksortParallel(data.subspan(left+mid)); });
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
  std::uniform_int_distribution<std::int32_t> distribution(-100, 100);
  for (int it = 0; it < 1000; it++) {
    std::vector<std::int32_t> data(distribution(input_rng) + 1001);
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
  std::vector<std::int32_t> data(0);
  auto expected = data;

  TypeParam::value(data);

  ASSERT_EQ(data, expected);
}

TYPED_TEST(QuicksortTest, OneElement) {
  std::vector<std::int32_t> data(1, 42);
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
  std::uniform_int_distribution<int32_t> distribution(std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max());

  std::vector<std::int32_t> data(100000000);
  for (auto &value : data) {
    value = distribution(input_rng);
  }
  for (int it = 0; it < runs; it++) {
    auto copy1 = data;
    auto copy2 = data;

    auto sequential_start = Clock::now();
    QuicksortSequential(copy1);
    const Milliseconds sequential_time = Clock::now() - sequential_start;

    auto parallel_start = Clock::now();
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
