#include "device_config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "config";

static device_config_t s_config;

// Simple CRC32 (same as reference project)
static uint32_t crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

static uint32_t calc_crc(const device_config_t *cfg)
{
    // CRC over everything except the crc field itself
    size_t len = offsetof(device_config_t, crc);
    return crc32((const uint8_t *)cfg, len);
}

const device_config_t *device_config_get(void)
{
    return &s_config;
}

device_config_t *device_config_get_mut(void)
{
    return &s_config;
}

void device_config_set_defaults(device_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->magic   = CONFIG_MAGIC;
    cfg->version = CONFIG_VERSION;

    // UWB650 defaults (from V2.2 documentation)
    cfg->baudrate       = UWB_BAUD_115200;  // 1 = 115200
    cfg->data_rate      = UWB_RATE_6M8;     // 1 = 6.8 Mbps
    cfg->pan_id         = 0x0000;
    cfg->node_addr      = 0x0000;
    cfg->tx_power       = UWB_POWER_MAX;    // 10 = 27.7 dBm (maximum)
    cfg->preamble_code  = 9;                // default
    cfg->cca_enable     = 0;
    cfg->ack_enable     = 0;
    cfg->security_enable = 0;
    memset(cfg->security_key, 0, sizeof(cfg->security_key));
    cfg->rx_show_src    = 1;
    cfg->led_status     = 1;
    cfg->rx_enable      = 1;
    cfg->sniff_enable   = 0;
    cfg->ant_delay      = UWB_ANTDELAY_DEF; // 16400
    cfg->dist_offset_cm = 0;
    cfg->coord_x_cm     = 0;
    cfg->coord_y_cm     = 0;
    cfg->coord_z_cm     = 0;

    // Ranging target
    cfg->target_pan_id  = 0x1111;
    cfg->target_addr    = 0x0001;

    // WiFi
    memset(cfg->wifi_ssid, 0, sizeof(cfg->wifi_ssid));
    memset(cfg->wifi_pass, 0, sizeof(cfg->wifi_pass));

    cfg->crc = calc_crc(cfg);
}

esp_err_t device_config_load(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CONFIG_NVS_NS, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found, using defaults");
        device_config_set_defaults(&s_config);
        return ESP_ERR_NOT_FOUND;
    }

    size_t len = sizeof(s_config);
    err = nvs_get_blob(nvs, CONFIG_NVS_KEY, &s_config, &len);
    nvs_close(nvs);

    if (err != ESP_OK || len != sizeof(s_config)) {
        ESP_LOGW(TAG, "Config blob read failed (err=%d, len=%u), using defaults", err, (unsigned)len);
        device_config_set_defaults(&s_config);
        return ESP_ERR_NOT_FOUND;
    }

    // Validate magic and CRC
    if (s_config.magic != CONFIG_MAGIC) {
        ESP_LOGW(TAG, "Config magic mismatch (0x%08lX), using defaults", (unsigned long)s_config.magic);
        device_config_set_defaults(&s_config);
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t expected_crc = calc_crc(&s_config);
    if (s_config.crc != expected_crc) {
        ESP_LOGW(TAG, "Config CRC mismatch (stored=0x%08lX, calc=0x%08lX), using defaults",
                 (unsigned long)s_config.crc, (unsigned long)expected_crc);
        device_config_set_defaults(&s_config);
        return ESP_ERR_INVALID_CRC;
    }

    ESP_LOGI(TAG, "Config loaded from NVS (version=%lu)", (unsigned long)s_config.version);
    return ESP_OK;
}

esp_err_t device_config_save(void)
{
    s_config.crc = calc_crc(&s_config);

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CONFIG_NVS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs, CONFIG_NVS_KEY, &s_config, sizeof(s_config));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS write failed: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return err;
    }

    err = nvs_commit(nvs);
    nvs_close(nvs);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Config saved to NVS (%u bytes)", (unsigned)sizeof(s_config));
    }
    return err;
}

esp_err_t device_config_factory_reset(void)
{
    ESP_LOGW(TAG, "Factory reset: erasing NVS namespace '%s'", CONFIG_NVS_NS);

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CONFIG_NVS_NS, NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        nvs_erase_all(nvs);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    device_config_set_defaults(&s_config);
    ESP_LOGI(TAG, "Config reset to factory defaults");
    return ESP_OK;
}

void device_config_log(const device_config_t *cfg)
{
    static const int baud_table[] = {230400, 115200, 57600, 38400, 19200, 9600};
    static const float power_table[] = {-5.0f, -2.5f, 0.0f, 2.5f, 5.0f, 7.5f, 10.0f, 12.5f, 15.0f, 20.0f, 27.7f};

    ESP_LOGI(TAG, "=== Device Configuration ===");
    ESP_LOGI(TAG, "  Baud rate:    %d (%d bps)", cfg->baudrate,
             cfg->baudrate <= 5 ? baud_table[cfg->baudrate] : 0);
    ESP_LOGI(TAG, "  Data rate:    %s", cfg->data_rate == 0 ? "850 Kbps" : "6.8 Mbps");
    ESP_LOGI(TAG, "  PAN ID:       0x%04X", cfg->pan_id);
    ESP_LOGI(TAG, "  Node address: 0x%04X", cfg->node_addr);
    ESP_LOGI(TAG, "  TX Power:     %d (%.1f dBm)", cfg->tx_power,
             cfg->tx_power <= 10 ? power_table[cfg->tx_power] : 0.0f);
    ESP_LOGI(TAG, "  Preamble:     %d", cfg->preamble_code);
    ESP_LOGI(TAG, "  CCA:          %s", cfg->cca_enable ? "ON" : "OFF");
    ESP_LOGI(TAG, "  ACK:          %s", cfg->ack_enable ? "ON" : "OFF");
    ESP_LOGI(TAG, "  Security:     %s", cfg->security_enable ? "ON" : "OFF");
    ESP_LOGI(TAG, "  RX Show Src:  %s", cfg->rx_show_src ? "ON" : "OFF");
    ESP_LOGI(TAG, "  LED:          %s", cfg->led_status ? "ON" : "OFF");
    ESP_LOGI(TAG, "  RX Enable:    %s", cfg->rx_enable ? "ON" : "OFF");
    ESP_LOGI(TAG, "  Sniff:        %s", cfg->sniff_enable ? "ON" : "OFF");
    ESP_LOGI(TAG, "  Ant delay:    %u", cfg->ant_delay);
    ESP_LOGI(TAG, "  Dist offset:  %d cm", cfg->dist_offset_cm);
    ESP_LOGI(TAG, "  Coordinates:  (%ld, %ld, %ld) cm",
             (long)cfg->coord_x_cm, (long)cfg->coord_y_cm, (long)cfg->coord_z_cm);
    ESP_LOGI(TAG, "  Target PAN:   0x%04X", cfg->target_pan_id);
    ESP_LOGI(TAG, "  Target addr:  0x%04X", cfg->target_addr);
    ESP_LOGI(TAG, "  WiFi SSID:    %s", cfg->wifi_ssid[0] ? cfg->wifi_ssid : "(none)");
    ESP_LOGI(TAG, "============================");
}
