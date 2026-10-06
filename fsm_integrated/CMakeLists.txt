cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(fsm_integrated)

target_sources(app PRIVATE
    src/main.c
    src/app/state_machine.c
    src/sensor/sensor_mock.c
    src/storage/storage_nvs.c
    src/ble/ble_service.c
    src/ble/ble_notify.c
)
target_include_directories(app PRIVATE src)
