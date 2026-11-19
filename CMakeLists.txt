cmake_minimum_required(VERSION 3.20)
project(parlay_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(parlaylib
  GIT_REPOSITORY https://github.com/cmuparlay/parlaylib.git
  GIT_TAG 51017699dcc421f80479cdb238d3092233ad0d26
)
FetchContent_MakeAvailable(parlaylib)

add_executable(parlay_example main.cpp)
target_link_libraries(parlay_example PRIVATE parlay)

include(CTest)
if(BUILD_TESTING)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.16.0
    GIT_SHALLOW TRUE
  )
  set(BUILD_GMOCK OFF)
  set(INSTALL_GTEST OFF)
  FetchContent_MakeAvailable(googletest)

  add_executable(example_tests tests/example.cpp)
  target_link_libraries(example_tests PRIVATE GTest::gtest_main)

  include(GoogleTest)
  gtest_discover_tests(example_tests)
endif()
