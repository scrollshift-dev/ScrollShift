cmake_minimum_required(VERSION 3.20)
project(SmoothWheel VERSION 0.0.1 LANGUAGES CXX)

option(SMOOTHWHEEL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_executable(smoothwheel src/main.cpp)
target_compile_features(smoothwheel PRIVATE cxx_std_20)
target_include_directories(smoothwheel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_options(smoothwheel PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)
if(SMOOTHWHEEL_WARNINGS_AS_ERRORS)
  target_compile_options(smoothwheel PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
  )
endif()

include(CTest)
if(BUILD_TESTING)
  add_test(NAME cli_help COMMAND smoothwheel --help)
  add_test(NAME cli_version COMMAND smoothwheel --version)
endif()

install(TARGETS smoothwheel RUNTIME DESTINATION bin)
