idf_component_register(
    SRCS "m5cc_test_support.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_hw_support
        esp_system
    PRIV_REQUIRES
        esp_app_format
        freertos
        log
)
