cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pet_tracker_rc)

target_sources(app PRIVATE
    src/main.c
    src/app_state.c
    src/state_machine/state_machine.c
    src/ble/ble_adv.c
    src/ble/tracker_service.c
    src/ble/data_transfer.c
    src/sensor/motion_sensor.c
    src/storage/flash_log.c
    src/power/power_mgmt.c
    src/ota/ota_stub.c
    src/drivers/haptic_stub.c
)

target_include_directories(app PRIVATE src)
