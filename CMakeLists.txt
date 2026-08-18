cmake_minimum_required(VERSION 3.20)
project(SmoothWheel VERSION 0.0.1 LANGUAGES CXX)
include(GNUInstallDirs)
option(SMOOTHWHEEL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(SMOOTHWHEEL_SYSTEMD_UNIT_DIR "${CMAKE_INSTALL_PREFIX}/lib/systemd/system" CACHE PATH "systemd system unit directory")

add_library(smoothwheel_core
  src/input.cpp
  src/experiment.cpp
  src/velocity.cpp
  src/transform.cpp
  src/relay.cpp
  src/config.cpp
  src/daemon.cpp)
target_compile_features(smoothwheel_core PUBLIC cxx_std_20)
target_include_directories(smoothwheel_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(smoothwheel_core PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)

add_executable(smoothwheel src/main.cpp)
target_link_libraries(smoothwheel PRIVATE smoothwheel_core)
target_compile_options(smoothwheel PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)
if(SMOOTHWHEEL_WARNINGS_AS_ERRORS)
  target_compile_options(smoothwheel_core PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>)
  target_compile_options(smoothwheel PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>)
endif()

configure_file(packaging/smoothwheel.service.in ${CMAKE_CURRENT_BINARY_DIR}/smoothwheel.service @ONLY)

include(CTest)
if(BUILD_TESTING)
  add_executable(smoothwheel_input_tests tests/input_tests.cpp)
  target_link_libraries(smoothwheel_input_tests PRIVATE smoothwheel_core)
  target_compile_definitions(smoothwheel_input_tests PRIVATE SMOOTHWHEEL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME cli_help COMMAND smoothwheel --help)
  add_test(NAME cli_version COMMAND smoothwheel --version)
  add_test(NAME cli_devices COMMAND smoothwheel devices)
  add_test(NAME cli_acceleration_profiles COMMAND smoothwheel accelerate --profiles)
  add_test(NAME cli_daemon_missing_config COMMAND smoothwheel daemon --config ${CMAKE_CURRENT_BINARY_DIR}/does-not-exist.conf)
  set_tests_properties(cli_daemon_missing_config PROPERTIES WILL_FAIL TRUE)
  add_test(NAME input_unit COMMAND smoothwheel_input_tests)

  add_executable(smoothwheel_experiment_tests tests/experiment_tests.cpp)
  target_link_libraries(smoothwheel_experiment_tests PRIVATE smoothwheel_core)
  add_test(NAME experiment_unit COMMAND smoothwheel_experiment_tests)

  add_executable(smoothwheel_velocity_tests tests/velocity_tests.cpp)
  target_link_libraries(smoothwheel_velocity_tests PRIVATE smoothwheel_core)
  add_test(NAME velocity_unit COMMAND smoothwheel_velocity_tests)

  add_executable(smoothwheel_transform_tests tests/transform_tests.cpp)
  target_link_libraries(smoothwheel_transform_tests PRIVATE smoothwheel_core)
  add_test(NAME transform_unit COMMAND smoothwheel_transform_tests)

  add_executable(smoothwheel_config_tests tests/config_tests.cpp)
  target_link_libraries(smoothwheel_config_tests PRIVATE smoothwheel_core)
  add_test(NAME config_unit COMMAND smoothwheel_config_tests)

  add_executable(smoothwheel_daemon_tests tests/daemon_tests.cpp)
  target_link_libraries(smoothwheel_daemon_tests PRIVATE smoothwheel_core)
  add_test(NAME daemon_lifecycle_unit COMMAND smoothwheel_daemon_tests)
endif()

install(TARGETS smoothwheel RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/smoothwheel.service DESTINATION ${SMOOTHWHEEL_SYSTEMD_UNIT_DIR})
install(FILES packaging/config.conf.example DESTINATION ${CMAKE_INSTALL_DATADIR}/smoothwheel)
