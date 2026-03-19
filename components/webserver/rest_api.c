#include "webserver.h"
#include "device_config.h"
#include "uwb650_driver.h"
#include "wifi_manager.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "cJSON.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "rest";

#define FW_VERSION "0.2.1"

// ---- Helpers ----

static esp_err_t send_json(httpd_req_t *req, cJSON *root)
{
    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t send_ok(httpd_req_t *req, const char *msg)
{
    cJSON *r = cJSON_CreateObject();
    cJSON_AddBoolToObject(r, "success", true);
    cJSON_AddStringToObject(r, "message", msg);
    return send_json(req, r);
}

static esp_err_t send_err(httpd_req_t *req, int status, const char *msg)
{
    httpd_resp_set_status(req, status == 400 ? "400 Bad Request" : "500 Internal Server Error");
    cJSON *r = cJSON_CreateObject();
    cJSON_AddBoolToObject(r, "success", false);
    cJSON_AddStringToObject(r, "error", msg);
    return send_json(req, r);
}

static cJSON *read_body_json(httpd_req_t *req)
{
    int len = req->content_len;
    if (len <= 0 || len > 2048) return NULL;

    char *buf = malloc(len + 1);
    if (!buf) return NULL;

    int received = httpd_req_recv(req, buf, len);
    if (received <= 0) { free(buf); return NULL; }
    buf[received] = '\0';

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    return json;
}

// ---- GET /api/status ----

static esp_err_t status_handler(httpd_req_t *req)
{
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "version", FW_VERSION);
    cJSON_AddNumberToObject(r, "uptime", (double)(esp_timer_get_time() / 1000000));
    cJSON_AddNumberToObject(r, "freeHeap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(r, "minHeap", esp_get_minimum_free_heap_size());
    cJSON_AddBoolToObject(r, "uwbReady", uwb650_is_ready());

    const char *ranging_str = "idle";
    if (uwb650_ranging_state() == UWB650_RANGING_RUNNING) ranging_str = "running";
    else if (uwb650_ranging_state() == UWB650_RANGING_ERROR) ranging_str = "error";
    cJSON_AddStringToObject(r, "rangingState", ranging_str);

    cJSON_AddBoolToObject(r, "wifiSta", wifi_manager_sta_is_connected());
    cJSON_AddStringToObject(r, "apSsid", wifi_manager_get_ap_ssid());
    cJSON_AddStringToObject(r, "apIp", wifi_manager_get_ap_ip());
    const char *sta_ip = wifi_manager_get_sta_ip();
    cJSON_AddStringToObject(r, "staIp", sta_ip ? sta_ip : "");
    cJSON_AddNumberToObject(r, "wsClients", webserver_ws_client_count());

    return send_json(req, r);
}

// ---- GET /api/config ----

static esp_err_t config_get_handler(httpd_req_t *req)
{
    const device_config_t *cfg = device_config_get();

    cJSON *r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "baudrate", cfg->baudrate);
    cJSON_AddNumberToObject(r, "dataRate", cfg->data_rate);
    // PAN and addresses as hex strings
    char hex[8];
    snprintf(hex, sizeof(hex), "%04X", cfg->pan_id);
    cJSON_AddStringToObject(r, "panId", hex);
    snprintf(hex, sizeof(hex), "%04X", cfg->node_addr);
    cJSON_AddStringToObject(r, "nodeAddr", hex);
    cJSON_AddNumberToObject(r, "txPower", cfg->tx_power);
    cJSON_AddNumberToObject(r, "preambleCode", cfg->preamble_code);
    cJSON_AddNumberToObject(r, "ccaEnable", cfg->cca_enable);
    cJSON_AddNumberToObject(r, "ackEnable", cfg->ack_enable);
    cJSON_AddNumberToObject(r, "securityEnable", cfg->security_enable);
    cJSON_AddStringToObject(r, "securityKey", cfg->security_key);
    cJSON_AddNumberToObject(r, "rxShowSrc", cfg->rx_show_src);
    cJSON_AddNumberToObject(r, "ledStatus", cfg->led_status);
    cJSON_AddNumberToObject(r, "rxEnable", cfg->rx_enable);
    cJSON_AddNumberToObject(r, "sniffEnable", cfg->sniff_enable);
    cJSON_AddNumberToObject(r, "antDelay", cfg->ant_delay);
    cJSON_AddNumberToObject(r, "distOffsetCm", cfg->dist_offset_cm);
    cJSON_AddNumberToObject(r, "coordX", cfg->coord_x_cm);
    cJSON_AddNumberToObject(r, "coordY", cfg->coord_y_cm);
    cJSON_AddNumberToObject(r, "coordZ", cfg->coord_z_cm);
    snprintf(hex, sizeof(hex), "%04X", cfg->target_pan_id);
    cJSON_AddStringToObject(r, "targetPanId", hex);
    snprintf(hex, sizeof(hex), "%04X", cfg->target_addr);
    cJSON_AddStringToObject(r, "targetAddr", hex);
    cJSON_AddStringToObject(r, "wifiSsid", cfg->wifi_ssid);

    return send_json(req, r);
}

// ---- POST /api/config ----

static esp_err_t config_post_handler(httpd_req_t *req)
{
    cJSON *json = read_body_json(req);
    if (!json) return send_err(req, 400, "Invalid JSON");

    device_config_t *cfg = device_config_get_mut();

    // Update fields present in JSON
    cJSON *item;
    if ((item = cJSON_GetObjectItem(json, "baudrate")))     cfg->baudrate = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "dataRate")))     cfg->data_rate = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "panId")))        cfg->pan_id = (uint16_t)strtol(item->valuestring, NULL, 16);
    if ((item = cJSON_GetObjectItem(json, "nodeAddr")))     cfg->node_addr = (uint16_t)strtol(item->valuestring, NULL, 16);
    if ((item = cJSON_GetObjectItem(json, "txPower")))      cfg->tx_power = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "preambleCode"))) cfg->preamble_code = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "ccaEnable")))    cfg->cca_enable = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "ackEnable")))    cfg->ack_enable = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "securityEnable"))) cfg->security_enable = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "securityKey")))  strncpy(cfg->security_key, item->valuestring, sizeof(cfg->security_key)-1);
    if ((item = cJSON_GetObjectItem(json, "rxShowSrc")))    cfg->rx_show_src = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "ledStatus")))    cfg->led_status = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "rxEnable")))     cfg->rx_enable = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "sniffEnable")))  cfg->sniff_enable = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "antDelay")))     cfg->ant_delay = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "distOffsetCm"))) cfg->dist_offset_cm = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "coordX")))       cfg->coord_x_cm = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "coordY")))       cfg->coord_y_cm = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "coordZ")))       cfg->coord_z_cm = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "targetPanId")))  cfg->target_pan_id = (uint16_t)strtol(item->valuestring, NULL, 16);
    if ((item = cJSON_GetObjectItem(json, "targetAddr")))   cfg->target_addr = (uint16_t)strtol(item->valuestring, NULL, 16);
    if ((item = cJSON_GetObjectItem(json, "wifiSsid")))     strncpy(cfg->wifi_ssid, item->valuestring, sizeof(cfg->wifi_ssid)-1);
    if ((item = cJSON_GetObjectItem(json, "wifiPass")))     strncpy(cfg->wifi_pass, item->valuestring, sizeof(cfg->wifi_pass)-1);

    cJSON_Delete(json);

    esp_err_t err = device_config_save();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Config saved to NVS");
        device_config_log(cfg);
        return send_ok(req, "Config saved to NVS");
    }
    return send_err(req, 500, "Failed to save config");
}

// ---- POST /api/config/apply ----

static esp_err_t config_apply_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Apply config requested via REST API");
    const device_config_t *cfg = device_config_get();
    esp_err_t err = uwb650_apply_config(cfg);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Apply config: SUCCESS");
        return send_ok(req, "Configuration applied to UWB650 module");
    }
    ESP_LOGW(TAG, "Apply config: PARTIAL FAILURE (%s)", esp_err_to_name(err));
    return send_err(req, 500, "Some configuration commands failed");
}

// ---- GET /api/ranging ----

static esp_err_t ranging_get_handler(httpd_req_t *req)
{
    uwb650_range_result_t result;
    uwb650_get_last_result(&result);

    cJSON *r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "distance", result.distance_m);
    cJSON_AddNumberToObject(r, "rssi", result.rssi_dbm);
    cJSON_AddBoolToObject(r, "valid", result.valid);
    cJSON_AddNumberToObject(r, "seq", result.seq);
    cJSON_AddNumberToObject(r, "timestamp", result.timestamp_ms);

    const char *state = "idle";
    if (uwb650_ranging_state() == UWB650_RANGING_RUNNING) state = "running";
    cJSON_AddStringToObject(r, "state", state);

    return send_json(req, r);
}

// ---- Ranging callback for WS broadcast ----

static void ranging_ws_cb(const uwb650_range_result_t *result)
{
    webserver_ws_broadcast_ranging(result);
}

// ---- POST /api/ranging/start ----

static esp_err_t ranging_start_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Ranging start requested");
    if (uwb650_ranging_state() == UWB650_RANGING_RUNNING) {
        ESP_LOGW(TAG, "Ranging already running, rejecting");
        return send_err(req, 400, "Ranging already running");
    }

    const device_config_t *cfg = device_config_get();
    ESP_LOGI(TAG, "Starting ranging: target_pan=%04X target_addr=%04X module_ready=%d",
             cfg->target_pan_id, cfg->target_addr, uwb650_is_ready());
    esp_err_t err = uwb650_ranging_start(cfg->target_pan_id, cfg->target_addr, ranging_ws_cb);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Ranging started OK -> target=%04X", cfg->target_addr);
        return send_ok(req, "Ranging started");
    }
    ESP_LOGE(TAG, "Ranging start FAILED: %s", esp_err_to_name(err));
    return send_err(req, 500, "Failed to start ranging");
}

// ---- POST /api/ranging/stop ----

static esp_err_t ranging_stop_handler(httpd_req_t *req)
{
    uwb650_ranging_stop();
    ESP_LOGI(TAG, "Ranging stopped");
    return send_ok(req, "Ranging stopped");
}

// ---- GET /api/uwb/stats ----

static esp_err_t stats_handler(httpd_req_t *req)
{
    uwb650_stats_t st;
    uwb650_get_stats(&st);

    cJSON *r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "txCount", st.tx_count);
    cJSON_AddNumberToObject(r, "rxCount", st.rx_count);
    cJSON_AddNumberToObject(r, "okCount", st.ok_count);
    cJSON_AddNumberToObject(r, "errorCount", st.error_count);
    cJSON_AddNumberToObject(r, "timeoutCount", st.timeout_count);
    cJSON_AddNumberToObject(r, "rangingCount", st.ranging_count);
    cJSON_AddNumberToObject(r, "rangingOk", st.ranging_ok);
    cJSON_AddNumberToObject(r, "rangingFail", st.ranging_fail);

    return send_json(req, r);
}

// ---- POST /api/uwb/stats/reset ----

static esp_err_t stats_reset_handler(httpd_req_t *req)
{
    uwb650_reset_stats();
    return send_ok(req, "Statistics reset");
}

// ---- POST /api/uwb/query ----

static esp_err_t uwb_query_handler(httpd_req_t *req)
{
    cJSON *json = read_body_json(req);
    if (!json) return send_err(req, 400, "Invalid JSON");

    cJSON *param = cJSON_GetObjectItem(json, "param");
    if (!param || !param->valuestring) {
        cJSON_Delete(json);
        return send_err(req, 400, "Missing 'param' field");
    }

    char value[128];
    esp_err_t err = uwb650_query(param->valuestring, value, sizeof(value));

    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "param", param->valuestring);
    cJSON_AddStringToObject(r, "value", value);
    cJSON_AddBoolToObject(r, "success", err == ESP_OK);

    cJSON_Delete(json);
    return send_json(req, r);
}

// ---- POST /api/uwb/command ----

static esp_err_t uwb_command_handler(httpd_req_t *req)
{
    cJSON *json = read_body_json(req);
    if (!json) return send_err(req, 400, "Invalid JSON");

    cJSON *cmd = cJSON_GetObjectItem(json, "cmd");
    if (!cmd || !cmd->valuestring) {
        cJSON_Delete(json);
        return send_err(req, 400, "Missing 'cmd' field");
    }

    char response[UWB650_MAX_RESP_LEN];
    esp_err_t err = uwb650_send_cmd(cmd->valuestring, response, sizeof(response),
                                     UWB650_CMD_TIMEOUT_MS);

    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "cmd", cmd->valuestring);
    cJSON_AddStringToObject(r, "response", response);
    cJSON_AddBoolToObject(r, "success", err == ESP_OK);

    cJSON_Delete(json);
    return send_json(req, r);
}

// ---- POST /api/system/reboot ----

static esp_err_t reboot_handler(httpd_req_t *req)
{
    send_ok(req, "Rebooting...");
    ESP_LOGW(TAG, "Rebooting by user request");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

// ---- POST /api/system/reset ----

static esp_err_t factory_reset_handler(httpd_req_t *req)
{
    device_config_factory_reset();
    send_ok(req, "Factory reset done, rebooting...");
    ESP_LOGW(TAG, "Factory reset, rebooting");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

// ---- GET /api/wifi/scan ----

static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    wifi_manager_scan_start();

    wifi_ap_record_t records[20];
    int count = wifi_manager_get_scan_results(records, 20);

    cJSON *r = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid", (char *)records[i].ssid);
        cJSON_AddNumberToObject(ap, "rssi", records[i].rssi);
        cJSON_AddNumberToObject(ap, "channel", records[i].primary);
        cJSON_AddNumberToObject(ap, "auth", records[i].authmode);
        cJSON_AddItemToArray(r, ap);
    }

    char *json = cJSON_PrintUnformatted(r);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    cJSON_free(json);
    cJSON_Delete(r);
    return ESP_OK;
}

// ---- POST /api/wifi/connect ----

static esp_err_t wifi_connect_handler(httpd_req_t *req)
{
    cJSON *json = read_body_json(req);
    if (!json) return send_err(req, 400, "Invalid JSON");

    cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
    cJSON *pass = cJSON_GetObjectItem(json, "password");

    if (!ssid || !ssid->valuestring) {
        cJSON_Delete(json);
        return send_err(req, 400, "Missing 'ssid'");
    }

    const char *pw = (pass && pass->valuestring) ? pass->valuestring : "";
    esp_err_t err = wifi_manager_sta_connect(ssid->valuestring, pw);

    // Also save to config
    if (err == ESP_OK) {
        device_config_t *cfg = device_config_get_mut();
        strncpy(cfg->wifi_ssid, ssid->valuestring, sizeof(cfg->wifi_ssid) - 1);
        strncpy(cfg->wifi_pass, pw, sizeof(cfg->wifi_pass) - 1);
        device_config_save();
    }

    cJSON_Delete(json);
    return (err == ESP_OK) ? send_ok(req, "Connecting...") : send_err(req, 500, "Connect failed");
}

// ---- POST /api/wifi/disconnect ----

static esp_err_t wifi_disconnect_handler(httpd_req_t *req)
{
    wifi_manager_sta_disconnect();
    return send_ok(req, "Disconnected");
}

// ---- POST /api/system/ota ----

static esp_err_t ota_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "OTA update started, firmware size=%d bytes", req->content_len);

    if (req->content_len <= 0) {
        return send_err(req, 400, "No firmware data");
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "OTA: no update partition found");
        return send_err(req, 500, "No OTA partition available");
    }

    ESP_LOGI(TAG, "OTA: writing to partition '%s' at offset 0x%lx",
             update_partition->label, (unsigned long)update_partition->address);

    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update_partition, req->content_len, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        return send_err(req, 500, "OTA begin failed");
    }

    char *buf = malloc(4096);
    if (!buf) {
        esp_ota_abort(ota_handle);
        return send_err(req, 500, "Out of memory");
    }

    int remaining = req->content_len;
    int received_total = 0;
    bool failed = false;

    while (remaining > 0) {
        int to_read = remaining < 4096 ? remaining : 4096;
        int received = httpd_req_recv(req, buf, to_read);

        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;  // retry on timeout
            }
            ESP_LOGE(TAG, "OTA: receive error at %d/%d bytes", received_total, req->content_len);
            failed = true;
            break;
        }

        err = esp_ota_write(ota_handle, buf, received);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OTA write failed: %s", esp_err_to_name(err));
            failed = true;
            break;
        }

        received_total += received;
        remaining -= received;

        // Log progress every ~100KB
        if ((received_total % (100 * 1024)) < 4096) {
            ESP_LOGI(TAG, "OTA progress: %d / %d bytes (%d%%)",
                     received_total, req->content_len,
                     received_total * 100 / req->content_len);
        }
    }

    free(buf);

    if (failed) {
        esp_ota_abort(ota_handle);
        return send_err(req, 500, "OTA receive/write failed");
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA end failed (image invalid?): %s", esp_err_to_name(err));
        return send_err(req, 500, "OTA validation failed — bad firmware image");
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA set boot partition failed: %s", esp_err_to_name(err));
        return send_err(req, 500, "Failed to set boot partition");
    }

    ESP_LOGI(TAG, "OTA update successful (%d bytes), rebooting...", received_total);
    send_ok(req, "OTA update successful, rebooting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

// ---- Register all REST API endpoints ----

void rest_api_init(httpd_handle_t server)
{
    #define REG(method_, uri_, handler_) do { \
        static const httpd_uri_t u = { .uri = uri_, .method = method_, .handler = handler_ }; \
        httpd_register_uri_handler(server, &u); \
    } while(0)

    REG(HTTP_GET,  "/api/status",          status_handler);
    REG(HTTP_GET,  "/api/config",          config_get_handler);
    REG(HTTP_POST, "/api/config",          config_post_handler);
    REG(HTTP_POST, "/api/config/apply",    config_apply_handler);
    REG(HTTP_GET,  "/api/ranging",         ranging_get_handler);
    REG(HTTP_POST, "/api/ranging/start",   ranging_start_handler);
    REG(HTTP_POST, "/api/ranging/stop",    ranging_stop_handler);
    REG(HTTP_GET,  "/api/uwb/stats",       stats_handler);
    REG(HTTP_POST, "/api/uwb/stats/reset", stats_reset_handler);
    REG(HTTP_POST, "/api/uwb/query",       uwb_query_handler);
    REG(HTTP_POST, "/api/uwb/command",     uwb_command_handler);
    REG(HTTP_POST, "/api/system/reboot",   reboot_handler);
    REG(HTTP_POST, "/api/system/reset",    factory_reset_handler);
    REG(HTTP_POST, "/api/system/ota",      ota_handler);
    REG(HTTP_GET,  "/api/wifi/scan",       wifi_scan_handler);
    REG(HTTP_POST, "/api/wifi/connect",    wifi_connect_handler);
    REG(HTTP_POST, "/api/wifi/disconnect", wifi_disconnect_handler);

    #undef REG

    ESP_LOGI(TAG, "REST API: 17 endpoints registered");
}
