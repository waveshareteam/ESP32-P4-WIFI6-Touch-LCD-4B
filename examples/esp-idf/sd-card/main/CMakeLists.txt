set(srcs "sd_card_example_main.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       REQUIRES fatfs sd_card esp_driver_sdmmc esp_hw_support
                       WHOLE_ARCHIVE)

if(NOT CONFIG_SOC_SDMMC_HOST_SUPPORTED)
    fail_at_build_time(sdmmc ""
                             "This target does not provide an SDMMC host."
                             "Please refer README.md for more details")
endif()
