# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources 
    ${CMAKE_SOURCE_DIR}/src/*.*
    ${CMAKE_SOURCE_DIR}/drivers/*.*
    ${CMAKE_SOURCE_DIR}/app/*.*
    ${CMAKE_SOURCE_DIR}/app/wifi/*.cpp
)

idf_component_register(SRCS ${app_sources})
