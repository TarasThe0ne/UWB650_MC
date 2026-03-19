#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <string.h>

static const char *TAG = "wifi";

#define AP_CHANNEL      1
#define AP_MAX_CONN     4
#define AP_PASSWORD     "12345678"

static char s_ap_ssid[32]  = {0};
static char s_ap_ip[16]    = "192.168.4.1";
static char s_sta_ip[16]   = {0};
static bool s_sta_connected = false;
static esp_netif_t *s_netif_ap  = NULL;
static esp_netif_t *s_netif_sta = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "STA started");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            s_sta_connected = false;
            memset(s_sta_ip, 0, sizeof(s_sta_ip));
            ESP_LOGW(TAG, "STA disconnected, retrying in 5s...");
            vTaskDelay(pdMS_TO_TICKS(5000));
            esp_wifi_connect();
            break;
        case WIFI_EVENT_AP_STACONNECTED: {
            wifi_event_ap_staconnected_t *ev = data;
            ESP_LOGI(TAG, "AP: client connected (MAC=" MACSTR ")", MAC2STR(ev->mac));
            break;
        }
        case WIFI_EVENT_AP_STADISCONNECTED: {
            wifi_event_ap_stadisconnected_t *ev = data;
            ESP_LOGI(TAG, "AP: client disconnected (MAC=" MACSTR ")", MAC2STR(ev->mac));
            break;
        }
        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = data;
        snprintf(s_sta_ip, sizeof(s_sta_ip), IPSTR, IP2STR(&ev->ip_info.ip));
        s_sta_connected = true;
        ESP_LOGI(TAG, "STA got IP: %s", s_sta_ip);
    }
}

esp_err_t wifi_manager_init(const char *sta_ssid, const char *sta_pass)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create AP and STA netifs
    s_netif_ap  = esp_netif_create_default_wifi_ap();
    s_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

    // Generate AP SSID from MAC
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "UWB650MC-%02X%02X", mac[4], mac[5]);

    // Configure AP
    wifi_config_t ap_cfg = {
        .ap = {
            .channel        = AP_CHANNEL,
            .max_connection = AP_MAX_CONN,
            .authmode       = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg.required = false,
        },
    };
    strlcpy((char *)ap_cfg.ap.ssid, s_ap_ssid, sizeof(ap_cfg.ap.ssid));
    strlcpy((char *)ap_cfg.ap.password, AP_PASSWORD, sizeof(ap_cfg.ap.password));
    ap_cfg.ap.ssid_len = strlen(s_ap_ssid);

    bool has_sta = (sta_ssid && sta_ssid[0]);

    if (has_sta) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));

    if (has_sta) {
        wifi_config_t sta_cfg = {0};
        strlcpy((char *)sta_cfg.sta.ssid, sta_ssid, sizeof(sta_cfg.sta.ssid));
        if (sta_pass && sta_pass[0]) {
            strlcpy((char *)sta_cfg.sta.password, sta_pass, sizeof(sta_cfg.sta.password));
        }
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    }

    ESP_ERROR_CHECK(esp_wifi_start());

    if (has_sta) {
        esp_wifi_connect();
    }

    ESP_LOGI(TAG, "WiFi AP started: %s (pass: %s)", s_ap_ssid, AP_PASSWORD);
    ESP_LOGI(TAG, "AP IP: %s", s_ap_ip);

    return ESP_OK;
}

esp_err_t wifi_manager_sta_connect(const char *ssid, const char *password)
{
    if (!ssid || !ssid[0]) return ESP_ERR_INVALID_ARG;

    // Switch to APSTA mode if not already
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_AP) {
        esp_wifi_set_mode(WIFI_MODE_APSTA);
    }

    wifi_config_t sta_cfg = {0};
    strlcpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid));
    if (password && password[0]) {
        strlcpy((char *)sta_cfg.sta.password, password, sizeof(sta_cfg.sta.password));
    }

    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    ESP_LOGI(TAG, "STA connecting to: %s", ssid);
    return esp_wifi_connect();
}

esp_err_t wifi_manager_sta_disconnect(void)
{
    s_sta_connected = false;
    memset(s_sta_ip, 0, sizeof(s_sta_ip));
    return esp_wifi_disconnect();
}

esp_err_t wifi_manager_scan_start(void)
{
    // Scan requires STA (or APSTA) mode
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_AP) {
        esp_wifi_set_mode(WIFI_MODE_APSTA);
        ESP_LOGI(TAG, "Switched to APSTA mode for scan");
    }

    wifi_scan_config_t scan_cfg = {
        .show_hidden = false,
        .scan_type   = WIFI_SCAN_TYPE_ACTIVE,
    };
    return esp_wifi_scan_start(&scan_cfg, true);  // blocking
}

int wifi_manager_get_scan_results(void *out_records, int max_records)
{
    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    if (count == 0) return 0;

    uint16_t fetch = (count < max_records) ? count : max_records;
    esp_wifi_scan_get_ap_records(&fetch, (wifi_ap_record_t *)out_records);
    return (int)fetch;
}

bool wifi_manager_sta_is_connected(void)
{
    return s_sta_connected;
}

const char *wifi_manager_get_ap_ssid(void)
{
    return s_ap_ssid;
}

const char *wifi_manager_get_ap_ip(void)
{
    return s_ap_ip;
}

const char *wifi_manager_get_sta_ip(void)
{
    return s_sta_ip[0] ? s_sta_ip : NULL;
}
