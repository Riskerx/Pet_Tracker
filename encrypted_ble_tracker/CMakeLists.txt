 # Minimum version of CMake required to build this project
cmake_minimum_required(VERSION 3.20.0)
 
# Load the Zephyr build system
# ZEPHYR_BASE is an environment variable set during SDK installation
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
 
# Give your project a name
project(ble_tracker)
 
# Tell CMake which source files to include in the build
# Add a new line here every time you create a new .c file
target_sources(app PRIVATE
    src/main.c
    src/tracker_service.c
    src/ble/ble_security.c
)
