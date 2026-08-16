add_library(mik32_freertos STATIC)

set(FREERTOS_PATH ${CMAKE_CURRENT_LIST_DIR}/../freertos/FreeRTOS-Kernel)
set(BSP_PATH ${CMAKE_CURRENT_LIST_DIR}/../bsp/freertos)

file(GLOB FREERTOS_SOURCES
        "${FREERTOS_PATH}/tasks.c"
        "${FREERTOS_PATH}/queue.c"
        "${FREERTOS_PATH}/list.c"
        "${FREERTOS_PATH}/timers.c"
        "${FREERTOS_PATH}/portable/GCC/RISC-V/port.c"
        "${FREERTOS_PATH}/portable/MemMang/heap_4.c"
        "${FREERTOS_PATH}/portable/GCC/RISC-V/portASM.S"
)

target_sources(mik32_freertos PRIVATE
        ${FREERTOS_SOURCES}
)

target_include_directories(mik32_freertos PUBLIC
        ${FREERTOS_PATH}/include
        ${BSP_PATH}
        ${FREERTOS_PATH}/portable/GCC/RISC-V/
)

target_link_libraries(mik32_freertos PUBLIC mik32_shared)
