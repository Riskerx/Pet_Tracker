
add_library(periph_drivers STATIC
  src/factory.c
  src/haptic_mock.c
  src/mag_mock.c
)

target_include_directories(periph_drivers PUBLIC include)

target_link_libraries(periph_drivers PUBLIC zephyr_interface)

if(CONFIG_PERIPH_HAPTIC_DRV2605)
  target_sources(periph_drivers PRIVATE src/haptic_drv2605.c)
endif()

if(CONFIG_PERIPH_MAG_BMM350)
  target_sources(periph_drivers PRIVATE src/mag_bmm350.c)
endif()

target_link_libraries(app PRIVATE periph_drivers)