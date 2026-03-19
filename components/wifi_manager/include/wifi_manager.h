#pragma once

#include "esp_err.h"
#include <stdbool.h>

// Initialize WiFi in AP+STA mode
// AP is always on. If sta_ssid is non-empty, also connects as STA.
esp_err_t wifi_manager_init(const char *sta_ssid, const char *sta_pass);

// Connect STA to a network (saves nothing to NVS)
esp_err_t wifi_manager_sta_connect(const char *ssid, const char *password);

// Disconnect STA
esp_err_t wifi_manager_sta_disconnect(void);

// Start WiFi scan, results via wifi_manager_get_scan_results()
esp_err_t wifi_manager_scan_start(void);

// Get scan results. Returns number of APs found. Caller must free *out_records.
int wifi_manager_get_scan_results(void *out_records, int max_records);

// Status queries
bool        wifi_manager_sta_is_connected(void);
const char *wifi_manager_get_ap_ssid(void);
const char *wifi_manager_get_ap_ip(void);
const char *wifi_manager_get_sta_ip(void);
