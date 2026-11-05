# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Keep the silicon selection explicit and bind it to one build directory.  A
# generated sdkconfig is not safe to reuse between the pre-v3 and post-v3 ROM
# families.
set(FACTORY_SILICON_PROFILE "rev1_x" CACHE STRING
    "Factory silicon profile: rev1_x or rev3_x")
set_property(CACHE FACTORY_SILICON_PROFILE PROPERTY STRINGS rev1_x rev3_x)

if(FACTORY_SILICON_PROFILE STREQUAL "rev1_x")
    set(FACTORY_PROFILE_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.rev1_3")
    set(FACTORY_PROFILE_VERSION "factory-rev1x")
elseif(FACTORY_SILICON_PROFILE STREQUAL "rev3_x")
    set(FACTORY_PROFILE_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.rev3_x")
    set(FACTORY_PROFILE_VERSION "factory-rev3x")
else()
    message(FATAL_ERROR
        "Invalid FACTORY_SILICON_PROFILE='${FACTORY_SILICON_PROFILE}'. "
        "Use rev1_x or rev3_x.")
endif()

if(DEFINED FACTORY_SILICON_PROFILE_BUILD_GUARD AND
   NOT FACTORY_SILICON_PROFILE_BUILD_GUARD STREQUAL FACTORY_SILICON_PROFILE)
    message(FATAL_ERROR
        "This build directory is configured for ${FACTORY_SILICON_PROFILE_BUILD_GUARD}, "
        "not ${FACTORY_SILICON_PROFILE}. Use a separate build directory (and SDKCONFIG) "
        "for each factory silicon profile.")
endif()

set(FACTORY_BOARD_MANAGER_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/components/gen_bmgr_codes/board_manager.defaults")
if(NOT EXISTS "${FACTORY_BOARD_MANAGER_DEFAULTS}")
    message(FATAL_ERROR
        "Board Manager defaults are missing. Run 'idf.py bmgr -b esp32_p4_wifi6_touch_lcd_4b' first.")
endif()

# Retain an explicit SDKCONFIG supplied by the caller; otherwise isolate the
# generated file inside this build directory.
if(NOT DEFINED SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig" CACHE FILEPATH
        "Generated SDK configuration for this factory profile")
endif()

# The generated board defaults contain memory settings, so the silicon overlay
# must remain last.
set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults;${FACTORY_BOARD_MANAGER_DEFAULTS};${FACTORY_PROFILE_DEFAULTS}"
    CACHE STRING "Factory SDK configuration defaults" FORCE)
set(PROJECT_VER "${FACTORY_PROFILE_VERSION}" CACHE STRING
    "Factory application version" FORCE)
set(FACTORY_SILICON_PROFILE_BUILD_GUARD "${FACTORY_SILICON_PROFILE}" CACHE INTERNAL
    "Do not change factory silicon profile in an existing build directory")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-brookesia)

if(TARGET __idf_espressif__mcp-c-sdk AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(__idf_espressif__mcp-c-sdk PRIVATE
        -Wno-error=stringop-truncation
    )
endif()

if(TARGET __idf_espressif__esp-boost AND
   (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
    (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "15")))
    target_compile_options(__idf_espressif__esp-boost PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-error=deprecated-variadic-comma-omission>
    )
endif()
