add_library(hexa_drivers STATIC)

set(DRIVERS_PATH ${CMAKE_CURRENT_LIST_DIR}/../drivers)
set(BOARD_PATH ${CMAKE_CURRENT_LIST_DIR}/../board)
set(SHARED_PATH ${CMAKE_CURRENT_LIST_DIR}/../shared)

file(GLOB DRIVERS_SOURCES
        "${DRIVERS_PATH}/src/*.c"
        "${BOARD_PATH}/*.c"
)


target_sources(hexa_drivers PRIVATE
        ${DRIVERS_SOURCES}
        ${SHARED_PATH}/libs/xprintf.c
)

target_include_directories(hexa_drivers PUBLIC
        ${DRIVERS_PATH}/include
        ${BOARD_PATH}
)

target_link_libraries(hexa_drivers PUBLIC mik32_hal)

if(SDK_FREERTOS)
        target_compile_definitions(hexa_drivers PUBLIC SDK_FREERTOS=1)
        target_link_libraries(hexa_drivers PUBLIC mik32_freertos)
endif()