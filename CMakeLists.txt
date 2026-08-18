cmake_minimum_required(VERSION 3.20)
project(ScrollShift VERSION 0.0.1 LANGUAGES CXX)
include(GNUInstallDirs)
option(SCROLLSHIFT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(SCROLLSHIFT_SYSTEMD_UNIT_DIR "${CMAKE_INSTALL_PREFIX}/lib/systemd/system" CACHE PATH "systemd system unit directory")

add_library(scrollshift_core
  src/input.cpp
  src/experiment.cpp
  src/velocity.cpp
  src/transform.cpp
  src/relay.cpp
  src/config.cpp
  src/daemon.cpp
  src/environment.cpp)
target_compile_features(scrollshift_core PUBLIC cxx_std_20)
target_include_directories(scrollshift_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(scrollshift_core PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)

add_executable(scrollshift src/main.cpp)
target_link_libraries(scrollshift PRIVATE scrollshift_core)
target_compile_options(scrollshift PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>)
if(SCROLLSHIFT_WARNINGS_AS_ERRORS)
  target_compile_options(scrollshift_core PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>)
  target_compile_options(scrollshift PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>)
endif()

configure_file(packaging/scrollshift.service.in ${CMAKE_CURRENT_BINARY_DIR}/scrollshift.service @ONLY)

include(CTest)
if(BUILD_TESTING)
  add_executable(scrollshift_input_tests tests/input_tests.cpp)
  target_link_libraries(scrollshift_input_tests PRIVATE scrollshift_core)
  target_compile_definitions(scrollshift_input_tests PRIVATE SCROLLSHIFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME cli_help COMMAND scrollshift --help)
  add_test(NAME cli_version COMMAND scrollshift --version)
  add_test(NAME cli_devices COMMAND scrollshift devices)
  add_test(NAME cli_environment COMMAND scrollshift environment)
  add_test(NAME cli_acceleration_profiles COMMAND scrollshift accelerate --profiles)
  add_test(NAME cli_daemon_missing_config COMMAND scrollshift daemon --config ${CMAKE_CURRENT_BINARY_DIR}/does-not-exist.conf)
  set_tests_properties(cli_daemon_missing_config PROPERTIES WILL_FAIL TRUE)
  add_test(NAME input_unit COMMAND scrollshift_input_tests)

  add_executable(scrollshift_experiment_tests tests/experiment_tests.cpp)
  target_link_libraries(scrollshift_experiment_tests PRIVATE scrollshift_core)
  add_test(NAME experiment_unit COMMAND scrollshift_experiment_tests)

  add_executable(scrollshift_velocity_tests tests/velocity_tests.cpp)
  target_link_libraries(scrollshift_velocity_tests PRIVATE scrollshift_core)
  add_test(NAME velocity_unit COMMAND scrollshift_velocity_tests)

  add_executable(scrollshift_transform_tests tests/transform_tests.cpp)
  target_link_libraries(scrollshift_transform_tests PRIVATE scrollshift_core)
  add_test(NAME transform_unit COMMAND scrollshift_transform_tests)

  add_executable(scrollshift_transform_property_tests tests/transform_property_tests.cpp)
  target_link_libraries(scrollshift_transform_property_tests PRIVATE scrollshift_core)
  add_test(NAME transform_property_unit COMMAND scrollshift_transform_property_tests)

  add_executable(scrollshift_config_tests tests/config_tests.cpp)
  target_link_libraries(scrollshift_config_tests PRIVATE scrollshift_core)
  add_test(NAME config_unit COMMAND scrollshift_config_tests)

  add_executable(scrollshift_daemon_tests tests/daemon_tests.cpp)
  target_link_libraries(scrollshift_daemon_tests PRIVATE scrollshift_core)
  add_test(NAME daemon_lifecycle_unit COMMAND scrollshift_daemon_tests)
endif()

install(TARGETS scrollshift RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/scrollshift.service DESTINATION ${SCROLLSHIFT_SYSTEMD_UNIT_DIR})
install(FILES packaging/config.conf.example DESTINATION ${CMAKE_INSTALL_DATADIR}/scrollshift)
