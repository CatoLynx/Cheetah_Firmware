# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS           ${app_sources}
                       REQUIRES       esp_http_server)

target_add_binary_data(${COMPONENT_TARGET} "static/favicon.ico" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "static/jquery.min.js" TEXT)