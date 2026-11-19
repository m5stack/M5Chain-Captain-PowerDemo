idf_component_register(
    SRCS "m5cc_display.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        m5cc_board
        m5stack_chain_captain
        esp_lvgl_port
        lvgl__lvgl
    PRIV_REQUIRES
        freertos
        log
)
