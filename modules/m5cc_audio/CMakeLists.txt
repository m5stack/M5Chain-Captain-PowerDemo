idf_component_register(
    SRCS "m5cc_audio.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        m5cc_board
        m5stack_chain_captain
        esp_codec_dev
    PRIV_REQUIRES
        log
)
