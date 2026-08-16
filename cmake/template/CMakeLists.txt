cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED SDK_PATH)
        if(DEFINED ENV{HEXACORE_SDK})
                set(SDK_PATH $ENV{HEXACORE_SDK})
        else()
                message(FATAL_ERROR "Установите SDK_PATH или HEXACORE_SDK переменную окружения")
        endif()
endif()

set(CMAKE_TOOLCHAIN_FILE ${SDK_PATH}/cmake/toolchain.cmake)

project(my_app C ASM)

include(${SDK_PATH}/cmake/sdk.cmake)

sdk_add_executable(${PROJECT_NAME} main.c)
