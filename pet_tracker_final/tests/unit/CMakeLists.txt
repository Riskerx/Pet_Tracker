cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pet_tracker_unit_tests)

# Points back at the real source tree being tested - nothing here is
# copied, every file below is the actual production file (or, for
# fakes.c, the file that replaces one specific real file).
set(PT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)

target_include_directories(app PRIVATE ${PT_SRC})

target_sources(app PRIVATE
    # --- Real production code under test ---
    ${PT_SRC}/app_state.c
    ${PT_SRC}/state_machine/state_machine.c
    ${PT_SRC}/storage/flash_log.c
    ${PT_SRC}/power/power_mgmt.c
    ${PT_SRC}/ota/ota_stub.c
    ${PT_SRC}/drivers/haptic_stub.c

    # --- Test doubles ---
    # Replaces ble/tracker_service.c (needs a real BLE stack) and the
    # motion_sensor_get_last_sample() half of sensor/motion_sensor.c
    # (needs real/simulated hardware timing) with deterministic fakes.
    src/fakes.c

    # --- Test suites ---
    src/test_state_machine.c
    src/test_flash_log.c
    src/test_power_ota.c
    src/test_ota_stub.c
    src/test_haptic_stub.c
)
