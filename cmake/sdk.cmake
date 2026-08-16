set(SDK_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_CURRENT_LIST_DIR}/shared.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/hal.cmake)

if(SDK_FREERTOS)
        include(${CMAKE_CURRENT_LIST_DIR}/freertos.cmake)
endif()

function(sdk_add_executable target)
        add_executable(${target}
                ${ARGN}
                ${SDK_ROOT}/shared/runtime/crt0.S
        )

        if(SDK_FREERTOS)
                target_sources(${target} PRIVATE
                        ${SDK_ROOT}/bsp/freertos/bsp_main.c
                        ${SDK_ROOT}/bsp/freertos/mik32_freertos_glue.c
                )
                target_link_libraries(${target} PRIVATE mik32_hal mik32_freertos c gcc)
        else()
                target_sources(${target} PRIVATE ${SDK_ROOT}/bsp/bsp_main.c)
                target_link_libraries(${target} PRIVATE mik32_hal c gcc)
        endif()

        target_link_options(${target} PRIVATE
                -T${SDK_ROOT}/shared/ldscripts/spifi.ld
                -L${SDK_ROOT}/shared/ldscripts
                -Wl,-Map=${target}.map
        )
        add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${target}> ${target}.hex
                COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${target}> ${target}.bin
        )
endfunction()
