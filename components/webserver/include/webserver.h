#pragma once

#include "esp_err.h"
#include "uwb650_driver.h"

// Initialize HTTP server with all handlers
esp_err_t webserver_init(void);

// Stop HTTP server
void webserver_stop(void);

// WebSocket broadcast functions (called from driver/other components)
void webserver_ws_broadcast_ranging(const uwb650_range_result_t *result);
void webserver_ws_broadcast_status(void);
void webserver_ws_broadcast_log(const char *level, const char *message);

// WebSocket client count
int webserver_ws_client_count(void);
