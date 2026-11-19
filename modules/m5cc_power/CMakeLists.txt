idf_component_register(
    SRCS "m5cc_power.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        m5cc_board
        m5stack__m5pm1
        m5stack__m5ioe1
        esp_driver_gpio
        esp_hw_support
    PRIV_REQUIRES
        freertos
        log
)
