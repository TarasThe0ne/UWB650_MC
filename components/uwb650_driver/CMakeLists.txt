idf_component_register(
    SRCS "uwb650_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES driver log esp_timer device_config
)
