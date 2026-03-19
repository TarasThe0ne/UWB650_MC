idf_component_register(
    SRCS "http_server.c" "ws_handler.c" "rest_api.c" "pages.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server log esp_timer
    PRIV_REQUIRES device_config uwb650_driver wifi_manager json esp_wifi esp_system app_update
)
