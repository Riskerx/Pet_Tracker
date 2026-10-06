cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(periph_drivers_test)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../lib/periph_drivers periph_drivers_build)

target_sources(app PRIVATE
  src/test_haptic.c
  src/test_mag.c
)
