idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash log
             device_config uwb650_driver webserver wifi_manager
)
