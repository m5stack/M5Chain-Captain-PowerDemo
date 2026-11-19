idf_component_register(
    SRCS "m5cc_rtc.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        m5cc_board
        rx8130
        esp_driver_gpio
)
