add_library(mik32_hal STATIC)

set(HAL_PATH ${CMAKE_CURRENT_LIST_DIR}/../hal)
set(SHARED_PATH ${CMAKE_CURRENT_LIST_DIR}/../shared)

file(GLOB HAL_SOURCES
        "${HAL_PATH}/core/Source/*.c"
        "${HAL_PATH}/peripherals/Source/*.c"
        "${HAL_PATH}/utilities/Source/*.c"
)


target_sources(mik32_hal PRIVATE
        ${HAL_SOURCES}
        ${SHARED_PATH}/libs/dma_lib.c
        ${SHARED_PATH}/libs/rtc_lib.c
        ${SHARED_PATH}/libs/spi_lib.c
        ${SHARED_PATH}/libs/uart_lib.c
        ${SHARED_PATH}/libs/xprintf.c
)

target_include_directories(mik32_hal PUBLIC
        ${HAL_PATH}/core/Include
        ${HAL_PATH}/peripherals/Include
        ${HAL_PATH}/utilities/Include
)

target_link_libraries(mik32_hal PUBLIC mik32_shared)
