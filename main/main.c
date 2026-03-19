#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "device_config.h"
#include "wifi_manager.h"
#include "uwb650_driver.h"
#include "webserver.h"

static const char *TAG = "main";

#define FW_VERSION "0.3.1"

void app_main(void)
{
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "  UWB650 Module Tester v%s", FW_VERSION);
    ESP_LOGI(TAG, "  IDF: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "=================================");

    // 1. Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition corrupted, erasing...");
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG, "NVS initialized");

    // 2. Load device config
    device_config_load();
    const device_config_t *cfg = device_config_get();
    device_config_log(cfg);

    // 3. Initialize WiFi (AP + optional STA)
    wifi_manager_init(cfg->wifi_ssid, cfg->wifi_pass);
    ESP_LOGI(TAG, "WiFi initialized");

    // 4. Initialize web server
    webserver_init();
    ESP_LOGI(TAG, "Web server started");

    // 5. Initialize UWB650 driver
    err = uwb650_init();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "UWB650 UART initialized");

        // 6. Apply saved configuration to module
        err = uwb650_apply_config(cfg);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "UWB650 configuration applied successfully");
        } else {
            ESP_LOGW(TAG, "UWB650 configuration apply failed: %s", esp_err_to_name(err));
        }

        // 7. Diagnostic: query module version and test RANGING command formats
        char ver[64];
        if (uwb650_query("VERSION", ver, sizeof(ver)) == ESP_OK) {
            ESP_LOGI(TAG, "Module firmware version: %s", ver);
        }
        // Test DEVICEID readback
        char devid[64];
        if (uwb650_query("DEVICEID", devid, sizeof(devid)) == ESP_OK) {
            ESP_LOGI(TAG, "Module DEVICEID: %s", devid);
        }
        // Test RANGING with RXENABLE=0 first
        ESP_LOGI(TAG, "=== RANGING diagnostic ===");
        char resp[128];
        // Step 1: Disable RX
        ESP_LOGI(TAG, "Disabling RXENABLE for ranging test...");
        uwb650_set_rx_enable(0);
        vTaskDelay(pdMS_TO_TICKS(100));
        // Step 2: Try RANGING
        ESP_LOGI(TAG, "Testing: UWBRFAT+RANGING=1,0002 (with RXENABLE=0)");
        esp_err_t r = uwb650_send_cmd("RANGING=1,0002", resp, sizeof(resp), 3000);
        ESP_LOGI(TAG, "  Result: %s resp='%s'", esp_err_to_name(r), resp);
        // Step 3: Re-enable RX
        uwb650_set_rx_enable(1);
    } else {
        ESP_LOGE(TAG, "UWB650 init failed: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "=== Initialization complete ===");
    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());

    // Main loop: periodic status logging
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        ESP_LOGI(TAG, "Heap: %lu bytes free, min: %lu",
                 (unsigned long)esp_get_free_heap_size(),
                 (unsigned long)esp_get_minimum_free_heap_size());
    }
}
