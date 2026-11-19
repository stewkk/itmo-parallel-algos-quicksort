#include <iostream>

#include <parlay/parallel.h>

int main() {
  int left_square = 0;
  int right_square = 0;
  parlay::par_do([&] { left_square = 3 * 3; },
                 [&] { right_square = 4 * 4; });

  std::cout << left_square + right_square << '\n';
}
