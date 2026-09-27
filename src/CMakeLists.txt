# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources 
    ${CMAKE_SOURCE_DIR}/src/*.*
    ${CMAKE_SOURCE_DIR}/drivers/*.*
    ${CMAKE_SOURCE_DIR}/app/*.*
    ${CMAKE_SOURCE_DIR}/app/ble/*.*
    ${CMAKE_SOURCE_DIR}/app/ble/esp-nimble-cpp/src/*.*
)

list(FILTER app_sources EXCLUDE REGEX ${CMAKE_SOURCE_DIR}/app/ble/esp-nimble-cpp/examples)

# NimBLEStream (esp-nimble-cpp 2.x) checks the return of ble_npl_callout_init(), which is
# void in the NimBLE port shipped with the pinned IDF 5.0.1 and only became int later.
# The class is a standalone Arduino-Stream adapter -- nothing else in the library links
# against it, and this firmware never uses it -- so dropping the translation unit is
# enough. Revisit if the platform pin in platformio.ini is ever raised.
list(FILTER app_sources EXCLUDE REGEX ${CMAKE_SOURCE_DIR}/app/ble/esp-nimble-cpp/src/NimBLEStream.cpp)

idf_component_register(SRCS ${app_sources})
