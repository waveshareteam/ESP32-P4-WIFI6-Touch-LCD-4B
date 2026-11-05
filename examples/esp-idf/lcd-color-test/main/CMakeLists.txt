idf_component_register(SRCS "test_esp_lcd_st7703.c"
                       REQUIRES gen_bmgr_codes esp_board_manager
                       PRIV_REQUIRES esp_timer unity)
