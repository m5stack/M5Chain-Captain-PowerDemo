idf_component_register(
    SRCS "m5cc_board.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        m5stack__m5pm1
        m5stack__m5ioe1
        esp_driver_gpio
        esp_driver_i2c
    PRIV_REQUIRES
        log
        m5stack_chain_captain
)
