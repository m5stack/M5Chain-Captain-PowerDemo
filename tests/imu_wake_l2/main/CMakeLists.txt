idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        m5cc_board
        m5cc_imu
        m5cc_power
        m5cc_test_support
)
