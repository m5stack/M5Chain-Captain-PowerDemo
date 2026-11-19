idf_component_register(
    SRCS "m5cc_imu.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        m5cc_board
        sensor_bmi270
    PRIV_REQUIRES
        freertos
        log
)
