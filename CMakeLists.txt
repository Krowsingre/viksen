cmake_minimum_required(VERSION 3.22)

project(AtmosSuite VERSION 0.1.0 LANGUAGES C CXX)

# The product name is provisional and only used by application targets.
set(PRODUCT_NAME "Viksen" CACHE STRING "User-facing product name (apps only; never used by engine/core)")
set(PRODUCT_PROJECT_EXTENSION "viksen" CACHE STRING "Project package extension (apps only)")
set(PRODUCT_BUNDLE_ID "com.viksen.app" CACHE STRING "Bundle identifier prefix (apps only)")

option(ATMOS_BUILD_TESTS "Build engine/core tests" ON)
option(ATMOS_BUILD_DESKTOP "Build the JUCE desktop app (Windows/macOS; Linux for development)" OFF)
option(ATMOS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()

include(cmake/CompilerWarnings.cmake)

add_subdirectory(engine)
add_subdirectory(core)

if(ATMOS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(ATMOS_BUILD_DESKTOP)
  add_subdirectory(apps/desktop)
endif()
