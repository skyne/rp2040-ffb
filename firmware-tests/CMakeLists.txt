cmake_minimum_required(VERSION 3.14)
project(firmware-tests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Download and configure GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include directories
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  ${CMAKE_CURRENT_SOURCE_DIR}/../firmware-base/src
  ${CMAKE_CURRENT_SOURCE_DIR}/../firmware-rim/src
  ${CMAKE_CURRENT_SOURCE_DIR}/../shared
)

# HAL mock library
add_library(hal_mocks STATIC
  mocks/hal_gpio.cpp
  mocks/hal_spi.cpp
  mocks/hal_i2c.cpp
  mocks/hal_serial.cpp
  mocks/hal_eeprom.cpp
  mocks/arduino_mock.cpp
)

target_include_directories(hal_mocks PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/mocks
)

# Firmware core logic library (testable units compiled for host)
add_library(firmware_core STATIC
  testable/settings_testable.cpp
  testable/ffb_testable.cpp
  testable/pedals_testable.cpp
  testable/link_protocol_testable.cpp
)

target_link_libraries(firmware_core
  hal_mocks
)

target_include_directories(firmware_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/testable
)

# Unit tests
add_executable(firmware_unit_tests
  tests/test_settings.cpp
  tests/test_ffb.cpp
  tests/test_pedals.cpp
  tests/test_link_protocol.cpp
  tests/test_profile_management.cpp
  tests/test_crc.cpp
)

target_link_libraries(firmware_unit_tests
  firmware_core
  hal_mocks
  GTest::gtest_main
  GTest::gmock_main
)

include(GoogleTest)
gtest_discover_tests(firmware_unit_tests)

# Coverage configuration (optional)
option(CODE_COVERAGE "Enable coverage reporting" OFF)
if(CODE_COVERAGE)
  target_compile_options(firmware_core PUBLIC --coverage -fprofile-arcs -ftest-coverage)
  target_compile_options(firmware_unit_tests PUBLIC --coverage -fprofile-arcs -ftest-coverage)
  target_link_options(firmware_unit_tests PUBLIC --coverage)
endif()
