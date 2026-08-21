# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/../../../config/sdkconfig.defaults")
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
    list(APPEND SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_options(-Wno-format)
# BSP 3.0.1 exposes the LVGL 9 rotation spelling in its public display API.
# Map it to LVGL 8's equivalent without modifying the managed component.
add_compile_definitions(lv_disp_rotation_t=lv_disp_rot_t)
project(lvgl_demo_v8)
