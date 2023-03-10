# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources 
    ${CMAKE_SOURCE_DIR}/src/*.*
    ${CMAKE_SOURCE_DIR}/drivers/*.*
    ${CMAKE_SOURCE_DIR}/app/*.*
    ${CMAKE_SOURCE_DIR}/app/ble/*.*
)

list(FILTER app_sources EXCLUDE REGEX ${CMAKE_SOURCE_DIR}/app/ble/esp-nimble-cpp/examples)

idf_component_register(SRCS ${app_sources})
