cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(my_app)

add_subdirectory(lib/periph_drivers)

target_sources(app PRIVATE src/main.c)