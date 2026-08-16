add_library(mik32_shared INTERFACE)

set(SHARED_PATH ${CMAKE_CURRENT_LIST_DIR}/../shared)

target_include_directories(mik32_shared INTERFACE
        ${SHARED_PATH}/include
        ${SHARED_PATH}/libs
        ${SHARED_PATH}/periphery
)
