#include "uwb650_driver.h"
#include "device_config.h"
#include "board_config.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "uwb650";

#define CMD_PREFIX      "UWBRFAT+"
#define CMD_SUFFIX      "\r\n"
#define RESP_OK         "OK"
#define RESP_ERROR      "ERROR"
#define RESP_RANGING    "+RANGING="
#define RESP_STARTUP    "Finished startup"

#define RX_TASK_STACK   4096
#define RX_TASK_PRIO    5
#define RANGE_TASK_STACK 4096
#define RANGE_TASK_PRIO  6
#define CMD_DELAY_MS    50      // delay between sequential commands

// ---- Internal state ----
static struct {
    bool            initialized;
    bool            module_ready;

    // UART RX
    TaskHandle_t    rx_task;
    char            line_buf[UWB650_MAX_RESP_LEN];
    int             line_pos;

    // Command/response synchronization
    SemaphoreHandle_t cmd_mutex;    // serialize commands
    SemaphoreHandle_t resp_sem;     // response received
    char            resp_buf[UWB650_MAX_RESP_LEN];
    bool            resp_ok;        // true=OK, false=ERROR

    // Ranging
    SemaphoreHandle_t range_sem;    // +RANGING= received
    char            range_buf[UWB650_MAX_RESP_LEN];

    // Continuous ranging
    bool            ranging_active;
    TaskHandle_t    range_task;
    uint16_t        target_pan;
    uint16_t        target_addr;
    uwb650_ranging_cb_t ranging_cb;

    // Shared results
    SemaphoreHandle_t data_mutex;
    uwb650_range_result_t last_result;
    uwb650_stats_t  stats;
    uint32_t        range_seq;
} s_drv;

// ---- Forward declarations ----
static void rx_task(void *arg);
static void process_line(const char *line);
static void ranging_task(void *arg);
static bool parse_ranging(const char *line, float *dist, float *rssi);

// ---- Initialization ----

esp_err_t uwb650_init(void)
{
    if (s_drv.initialized) return ESP_OK;

    memset(&s_drv, 0, sizeof(s_drv));

    // Create synchronization primitives
    s_drv.cmd_mutex  = xSemaphoreCreateMutex();
    s_drv.resp_sem   = xSemaphoreCreateBinary();
    s_drv.range_sem  = xSemaphoreCreateBinary();
    s_drv.data_mutex = xSemaphoreCreateMutex();

    if (!s_drv.cmd_mutex || !s_drv.resp_sem || !s_drv.range_sem || !s_drv.data_mutex) {
        ESP_LOGE(TAG, "Failed to create semaphores");
        return ESP_ERR_NO_MEM;
    }

    // Configure UART
    uart_config_t uart_cfg = {
        .baud_rate  = UWB_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(UWB_UART_NUM, UWB_UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(err));
        return err;
    }

    uart_param_config(UWB_UART_NUM, &uart_cfg);
    uart_set_pin(UWB_UART_NUM, UWB_UART_TX_PIN, UWB_UART_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Flush any stale data
    uart_flush_input(UWB_UART_NUM);

    // Start RX task
    BaseType_t ok = xTaskCreate(rx_task, "uwb_rx", RX_TASK_STACK, NULL,
                                RX_TASK_PRIO, &s_drv.rx_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RX task");
        uart_driver_delete(UWB_UART_NUM);
        return ESP_FAIL;
    }

    s_drv.initialized = true;
    ESP_LOGI(TAG, "UART%d initialized (TX=%d, RX=%d, baud=%d)",
             UWB_UART_NUM, UWB_UART_TX_PIN, UWB_UART_RX_PIN, UWB_UART_BAUD);
    return ESP_OK;
}

void uwb650_deinit(void)
{
    if (!s_drv.initialized) return;

    uwb650_ranging_stop();

    if (s_drv.rx_task) {
        vTaskDelete(s_drv.rx_task);
        s_drv.rx_task = NULL;
    }

    uart_driver_delete(UWB_UART_NUM);
    s_drv.initialized = false;
    ESP_LOGI(TAG, "Driver deinitialized");
}

bool uwb650_is_ready(void)
{
    return s_drv.initialized && s_drv.module_ready;
}

// ---- UART RX task ----

static void rx_task(void *arg)
{
    uint8_t byte;
    ESP_LOGI(TAG, "RX task started on UART%d", UWB_UART_NUM);
    uint32_t idle_count = 0;

    while (1) {
        int len = uart_read_bytes(UWB_UART_NUM, &byte, 1, pdMS_TO_TICKS(100));
        if (len <= 0) {
            idle_count++;
            // Log every 30 seconds of silence to show RX task is alive
            if (idle_count == 300) {
                ESP_LOGI(TAG, "RX: no data for 30s (UART%d alive, rx_bytes=%lu)",
                         UWB_UART_NUM, (unsigned long)s_drv.stats.rx_count);
                idle_count = 0;
            }
            continue;
        }
        idle_count = 0;

        s_drv.stats.rx_count++;

        // Accumulate line until \n
        if (byte == '\n') {
            // Strip trailing \r
            if (s_drv.line_pos > 0 && s_drv.line_buf[s_drv.line_pos - 1] == '\r') {
                s_drv.line_pos--;
            }
            s_drv.line_buf[s_drv.line_pos] = '\0';

            if (s_drv.line_pos > 0) {
                process_line(s_drv.line_buf);
            }
            s_drv.line_pos = 0;
        } else if (s_drv.line_pos < (int)(sizeof(s_drv.line_buf) - 1)) {
            s_drv.line_buf[s_drv.line_pos++] = (char)byte;
        }
    }
}

static void process_line(const char *line)
{
    ESP_LOGI(TAG, "RX: %s", line);

    // Check for "Finished Startup"
    if (strstr(line, RESP_STARTUP)) {
        s_drv.module_ready = true;
        ESP_LOGI(TAG, "Module startup complete");
        return;
    }

    // Check for +RANGING= response
    if (strncmp(line, RESP_RANGING, strlen(RESP_RANGING)) == 0) {
        strncpy(s_drv.range_buf, line, sizeof(s_drv.range_buf) - 1);
        s_drv.range_buf[sizeof(s_drv.range_buf) - 1] = '\0';
        xSemaphoreGive(s_drv.range_sem);
        return;
    }

    // Store response line (parameter value, status message etc.)
    // OK and ERROR signal command completion
    if (strcmp(line, RESP_OK) == 0) {
        s_drv.resp_ok = true;
        s_drv.stats.ok_count++;
        xSemaphoreGive(s_drv.resp_sem);
    } else if (strcmp(line, RESP_ERROR) == 0) {
        s_drv.resp_ok = false;
        s_drv.stats.error_count++;
        xSemaphoreGive(s_drv.resp_sem);
    } else {
        // Data line before OK/ERROR — store it as response content
        strncpy(s_drv.resp_buf, line, sizeof(s_drv.resp_buf) - 1);
        s_drv.resp_buf[sizeof(s_drv.resp_buf) - 1] = '\0';
    }
}

// ---- Low-level command ----

esp_err_t uwb650_send_cmd(const char *cmd, char *resp_out, size_t resp_len,
                           uint32_t timeout_ms)
{
    if (!s_drv.initialized) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_drv.cmd_mutex, portMAX_DELAY);

    // Clear stale state
    s_drv.resp_buf[0] = '\0';
    s_drv.resp_ok = false;
    xSemaphoreTake(s_drv.resp_sem, 0);  // drain

    // Build and send command
    char tx[UWB650_MAX_RESP_LEN];
    int n = snprintf(tx, sizeof(tx), CMD_PREFIX "%s" CMD_SUFFIX, cmd);
    s_drv.stats.tx_count++;

    ESP_LOGI(TAG, "TX: %.*s", n - 2, tx);  // log without \r\n
    uart_write_bytes(UWB_UART_NUM, tx, n);

    // Wait for OK or ERROR
    bool got = xSemaphoreTake(s_drv.resp_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;

    esp_err_t ret;
    if (!got) {
        ESP_LOGW(TAG, "CMD timeout (%lums): %s", (unsigned long)timeout_ms, cmd);
        s_drv.stats.timeout_count++;
        ret = ESP_ERR_TIMEOUT;
    } else if (s_drv.resp_ok) {
        ESP_LOGI(TAG, "CMD OK: %s (resp: '%s')", cmd, s_drv.resp_buf);
        ret = ESP_OK;
    } else {
        ESP_LOGW(TAG, "CMD error: %s (resp: '%s')", cmd, s_drv.resp_buf);
        ret = ESP_FAIL;
    }

    // Copy response data
    if (resp_out && resp_len > 0) {
        strncpy(resp_out, s_drv.resp_buf, resp_len - 1);
        resp_out[resp_len - 1] = '\0';
    }

    xSemaphoreGive(s_drv.cmd_mutex);
    return ret;
}

// ---- Helper: send command and log result ----

static esp_err_t send_and_log(const char *cmd, const char *label)
{
    char resp[64];
    esp_err_t err = uwb650_send_cmd(cmd, resp, sizeof(resp), UWB650_CMD_TIMEOUT_MS);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "  %s: OK", label);
    } else {
        ESP_LOGW(TAG, "  %s: FAILED (%s)", label, esp_err_to_name(err));
    }
    vTaskDelay(pdMS_TO_TICKS(CMD_DELAY_MS));
    return err;
}

// ---- Configuration setters ----

esp_err_t uwb650_set_baudrate(uint8_t rate) {
    char cmd[32]; snprintf(cmd, sizeof(cmd), "BAUDRATE=%d", rate);
    return send_and_log(cmd, "BAUDRATE");
}

esp_err_t uwb650_set_data_rate(uint8_t rate) {
    char cmd[32]; snprintf(cmd, sizeof(cmd), "DATARATE=%d", rate);
    return send_and_log(cmd, "DATARATE");
}

esp_err_t uwb650_set_device_id(uint16_t pan, uint16_t addr) {
    char cmd[32]; snprintf(cmd, sizeof(cmd), "DEVICEID=%04X,%04X", pan, addr);
    return send_and_log(cmd, "DEVICEID");
}

esp_err_t uwb650_set_power(uint8_t level) {
    char cmd[32]; snprintf(cmd, sizeof(cmd), "POWER=%d", level);
    return send_and_log(cmd, "POWER");
}

esp_err_t uwb650_set_preamble_code(uint8_t code) {
    char cmd[32]; snprintf(cmd, sizeof(cmd), "PREAMBLECODE=%d", code);
    return send_and_log(cmd, "PREAMBLECODE");
}

esp_err_t uwb650_set_cca(uint8_t en) {
    char cmd[32]; snprintf(cmd, sizeof(cmd), "CCAENABLE=%d", en);
    return send_and_log(cmd, "CCA");
}

esp_err_t uwb650_set_ack(uint8_t en) {
    char cmd[32]; snprintf(cmd, sizeof(cmd), "ACKENABLE=%d", en);
    return send_and_log(cmd, "ACK");
}

esp_err_t uwb650_set_security(uint8_t en, const char *key) {
    char cmd[80];
    if (en && key && key[0]) {
        snprintf(cmd, sizeof(cmd), "SECURITY=%d,%s", en, key);
    } else {
        snprintf(cmd, sizeof(cmd), "SECURITY=%d", en);
    }
    return send_and_log(cmd, "SECURITY");
}

esp_err_t uwb650_set_tx_target(uint16_t pan, uint16_t addr) {
    (void)pan;  // TXTARGET only takes address per UWB650 docs
    char cmd[32]; snprintf(cmd, sizeof(cmd), "TXTARGET=%04X", addr);
    return send_and_log(cmd, "TXTARGET");
}

esp_err_t uwb650_set_rx_show_src(uint8_t en) {
    char cmd[32]; snprintf(cmd, sizeof(cmd), "RXSHOWSRC=%d", en);
    return send_and_log(cmd, "RXSHOWSRC");
}

esp_err_t uwb650_set_led(uint8_t en) {
    char cmd[32]; snprintf(cmd, sizeof(cmd), "LEDSTATUS=%d", en);
    return send_and_log(cmd, "LED");
}

esp_err_t uwb650_set_rx_enable(uint8_t en) {
    char cmd[32]; snprintf(cmd, sizeof(cmd), "RXENABLE=%d", en);
    return send_and_log(cmd, "RXENABLE");
}

esp_err_t uwb650_set_sniff(uint8_t en) {
    char cmd[32]; snprintf(cmd, sizeof(cmd), "SNIFFEN=%d", en);
    return send_and_log(cmd, "SNIFFEN");
}

esp_err_t uwb650_set_ant_delay(uint16_t delay) {
    char cmd[32]; snprintf(cmd, sizeof(cmd), "ANTDELAY=%u", delay);
    return send_and_log(cmd, "ANTDELAY");
}

esp_err_t uwb650_set_dist_offset(int16_t offset_cm) {
    char cmd[32]; snprintf(cmd, sizeof(cmd), "DISTOFFSET=%d", offset_cm);
    return send_and_log(cmd, "DISTOFFSET");
}

esp_err_t uwb650_set_coordinate(int32_t x, int32_t y, int32_t z) {
    char cmd[64]; snprintf(cmd, sizeof(cmd), "COORDINATE=%ld,%ld,%ld",
                           (long)x, (long)y, (long)z);
    return send_and_log(cmd, "COORDINATE");
}

// ---- Query ----

esp_err_t uwb650_query(const char *param, char *value, size_t value_len)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "%s?", param);
    return uwb650_send_cmd(cmd, value, value_len, UWB650_CMD_TIMEOUT_MS);
}

// ---- Module control ----

esp_err_t uwb650_flash_save(void)
{
    return send_and_log("FLASH", "FLASH SAVE");
}

esp_err_t uwb650_reset(void)
{
    char resp[64];
    s_drv.module_ready = false;
    esp_err_t err = uwb650_send_cmd("RESET", resp, sizeof(resp), UWB650_CMD_TIMEOUT_MS);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Module resetting, waiting for startup...");
        // Wait for "Finished Startup" (module_ready flag set by RX task)
        for (int i = 0; i < UWB650_STARTUP_WAIT_MS / 100; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
            if (s_drv.module_ready) {
                ESP_LOGI(TAG, "Module ready after reset");
                return ESP_OK;
            }
        }
        ESP_LOGW(TAG, "Module did not report startup within %d ms", UWB650_STARTUP_WAIT_MS);
        // Assume it's ready anyway
        s_drv.module_ready = true;
    }
    return err;
}

esp_err_t uwb650_factory_default(void)
{
    s_drv.module_ready = false;
    esp_err_t err = send_and_log("DEFAULT", "FACTORY DEFAULT");
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Module defaulting, waiting for startup...");
        for (int i = 0; i < UWB650_STARTUP_WAIT_MS / 100; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
            if (s_drv.module_ready) break;
        }
        s_drv.module_ready = true;
    }
    return err;
}

esp_err_t uwb650_get_version(char *ver, size_t ver_len)
{
    return uwb650_query("VERSION", ver, ver_len);
}

// ---- Apply full configuration ----

esp_err_t uwb650_apply_config(const void *cfg_ptr)
{
    const device_config_t *cfg = (const device_config_t *)cfg_ptr;
    int ok_count = 0, fail_count = 0;

    ESP_LOGI(TAG, "=== Applying configuration to module ===");
    ESP_LOGI(TAG, "  PAN=%04X ADDR=%04X DataRate=%d Power=%d Preamble=%d",
             cfg->pan_id, cfg->node_addr, cfg->data_rate, cfg->tx_power, cfg->preamble_code);
    ESP_LOGI(TAG, "  CCA=%d ACK=%d Security=%d LED=%d RxEn=%d Sniff=%d",
             cfg->cca_enable, cfg->ack_enable, cfg->security_enable,
             cfg->led_status, cfg->rx_enable, cfg->sniff_enable);
    ESP_LOGI(TAG, "  Target=%04X:%04X AntDelay=%u DistOffset=%d",
             cfg->target_pan_id, cfg->target_addr, cfg->ant_delay, cfg->dist_offset_cm);

    // Reset module first to ensure clean state
    esp_err_t err = uwb650_reset();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Module reset failed, trying to configure anyway");
    }

    // Apply each parameter (order matters: BAUDRATE last to avoid losing comms)
    #define APPLY(fn, ...) do { \
        if ((fn)(__VA_ARGS__) == ESP_OK) ok_count++; else fail_count++; \
    } while(0)

    APPLY(uwb650_set_data_rate, cfg->data_rate);
    APPLY(uwb650_set_device_id, cfg->pan_id, cfg->node_addr);
    APPLY(uwb650_set_power, cfg->tx_power);
    APPLY(uwb650_set_preamble_code, cfg->preamble_code);
    APPLY(uwb650_set_cca, cfg->cca_enable);
    APPLY(uwb650_set_ack, cfg->ack_enable);
    APPLY(uwb650_set_security, cfg->security_enable, cfg->security_key);
    APPLY(uwb650_set_tx_target, cfg->target_pan_id, cfg->target_addr);
    APPLY(uwb650_set_rx_show_src, cfg->rx_show_src);
    APPLY(uwb650_set_led, cfg->led_status);
    APPLY(uwb650_set_rx_enable, cfg->rx_enable);
    APPLY(uwb650_set_sniff, cfg->sniff_enable);
    APPLY(uwb650_set_ant_delay, cfg->ant_delay);
    APPLY(uwb650_set_dist_offset, cfg->dist_offset_cm);
    APPLY(uwb650_set_coordinate, cfg->coord_x_cm, cfg->coord_y_cm, cfg->coord_z_cm);

    #undef APPLY

    // Save to module flash
    uwb650_flash_save();

    ESP_LOGI(TAG, "Configuration applied: %d OK, %d FAILED", ok_count, fail_count);

    // NOTE: we do NOT change BAUDRATE here — that would require
    // reconfiguring the ESP32 UART too, which is a risky operation.

    return (fail_count == 0) ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

// ---- Ranging ----

static bool parse_ranging(const char *line, float *dist, float *rssi)
{
    // Format: +RANGING=(distance),(rssi)
    // Example: +RANGING=(3.46),(-67.50)
    // Failed:  +RANGING=(-1),(0.00)
    const char *p = line + strlen(RESP_RANGING);

    // Parse distance: (value)
    if (*p != '(') return false;
    p++;
    *dist = strtof(p, (char **)&p);
    if (*p != ')') return false;
    p++;

    // Skip comma
    if (*p == ',') p++;

    // Parse RSSI: (value)
    if (*p != '(') return false;
    p++;
    *rssi = strtof(p, (char **)&p);
    if (*p != ')') return false;

    return true;
}

esp_err_t uwb650_range_single(uint16_t target_pan, uint16_t target_addr,
                               uwb650_range_result_t *result)
{
    if (!s_drv.initialized) {
        ESP_LOGE(TAG, "RANGE: driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_drv.module_ready) {
        ESP_LOGW(TAG, "RANGE: module not ready (module_ready=false)");
    }

    xSemaphoreTake(s_drv.cmd_mutex, portMAX_DELAY);

    // Drain stale semaphores
    xSemaphoreTake(s_drv.range_sem, 0);
    xSemaphoreTake(s_drv.resp_sem, 0);
    s_drv.range_buf[0] = '\0';
    s_drv.resp_buf[0] = '\0';
    s_drv.resp_ok = false;

    // Send ranging command
    char tx[80];
    int n = snprintf(tx, sizeof(tx), CMD_PREFIX "RANGING=1,%04X" CMD_SUFFIX,
                     target_addr);
    s_drv.stats.tx_count++;
    uart_write_bytes(UWB_UART_NUM, tx, n);
    ESP_LOGI(TAG, "TX: %.*s", n - 2, tx);

    // Poll for either +RANGING= (range_sem) or ERROR (resp_sem)
    bool got_range = false;
    bool got_error = false;
    for (int wait = 0; wait < UWB650_RANGE_TIMEOUT_MS / 50; wait++) {
        if (xSemaphoreTake(s_drv.range_sem, 0) == pdTRUE) {
            got_range = true;
            break;
        }
        if (xSemaphoreTake(s_drv.resp_sem, 0) == pdTRUE) {
            got_error = true;  // module responded with OK or ERROR instead of +RANGING=
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // Consume trailing OK/ERROR if we got +RANGING=
    if (got_range) {
        xSemaphoreTake(s_drv.resp_sem, pdMS_TO_TICKS(200));
    }

    xSemaphoreGive(s_drv.cmd_mutex);

    if (got_error) {
        ESP_LOGE(TAG, "RANGE: module rejected RANGING command with %s (target=%04X)",
                 s_drv.resp_ok ? "OK (unexpected)" : "ERROR", target_addr);
        result->valid = false;
        result->distance_m = -1.0f;
        result->rssi_dbm = 0.0f;
        result->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
        return ESP_FAIL;
    }

    if (!got_range) {
        s_drv.stats.timeout_count++;
        ESP_LOGW(TAG, "RANGE: TIMEOUT (%dms) no response from module (target=%04X)",
                 UWB650_RANGE_TIMEOUT_MS, target_addr);
        result->valid = false;
        result->distance_m = -1.0f;
        result->rssi_dbm = 0.0f;
        result->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "RANGE: response: '%s'", s_drv.range_buf);

    // Parse response
    float dist, rssi;
    if (!parse_ranging(s_drv.range_buf, &dist, &rssi)) {
        ESP_LOGW(TAG, "RANGE: parse FAILED for: '%s'", s_drv.range_buf);
        result->valid = false;
        result->distance_m = -1.0f;
        result->rssi_dbm = 0.0f;
        result->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "RANGE: dist=%.2fm rssi=%.1fdBm valid=%d",
             dist, rssi, (dist >= 0.0f));
    result->distance_m = dist;
    result->rssi_dbm = rssi;
    result->valid = (dist >= 0.0f);
    result->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);

    return ESP_OK;
}

// ---- Continuous ranging task ----

static void ranging_task(void *arg)
{
    const TickType_t period = pdMS_TO_TICKS(100); // 10 Hz
    TickType_t last_wake = xTaskGetTickCount();

    ESP_LOGI(TAG, "Continuous ranging started (target=%04X:%04X, 10Hz)",
             s_drv.target_pan, s_drv.target_addr);

    while (s_drv.ranging_active) {
        uwb650_range_result_t result;
        esp_err_t err = uwb650_range_single(s_drv.target_pan, s_drv.target_addr, &result);

        xSemaphoreTake(s_drv.data_mutex, portMAX_DELAY);
        result.seq = ++s_drv.range_seq;
        s_drv.last_result = result;
        s_drv.stats.ranging_count++;
        if (err == ESP_OK && result.valid) {
            s_drv.stats.ranging_ok++;
        } else {
            s_drv.stats.ranging_fail++;
        }
        xSemaphoreGive(s_drv.data_mutex);

        // Call user callback (outside mutex)
        if (s_drv.ranging_cb) {
            s_drv.ranging_cb(&result);
        }

        vTaskDelayUntil(&last_wake, period);
    }

    ESP_LOGI(TAG, "Continuous ranging stopped");
    s_drv.range_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t uwb650_ranging_start(uint16_t target_pan, uint16_t target_addr,
                                uwb650_ranging_cb_t cb)
{
    if (!s_drv.initialized) return ESP_ERR_INVALID_STATE;
    if (s_drv.ranging_active) return ESP_ERR_INVALID_STATE;

    // Disable continuous RX — module can't range while receiving
    ESP_LOGI(TAG, "Disabling RXENABLE before ranging...");
    uwb650_set_rx_enable(0);

    s_drv.target_pan  = target_pan;
    s_drv.target_addr = target_addr;
    s_drv.ranging_cb  = cb;
    s_drv.ranging_active = true;
    s_drv.range_seq = 0;

    BaseType_t ok = xTaskCreate(ranging_task, "uwb_range", RANGE_TASK_STACK,
                                NULL, RANGE_TASK_PRIO, &s_drv.range_task);
    if (ok != pdPASS) {
        s_drv.ranging_active = false;
        ESP_LOGE(TAG, "Failed to create ranging task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t uwb650_ranging_stop(void)
{
    if (!s_drv.ranging_active) return ESP_OK;

    s_drv.ranging_active = false;
    // Task will self-delete after detecting ranging_active=false
    // Wait for it to finish
    for (int i = 0; i < 30 && s_drv.range_task != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Re-enable continuous RX after ranging
    ESP_LOGI(TAG, "Re-enabling RXENABLE after ranging...");
    uwb650_set_rx_enable(1);

    return ESP_OK;
}

uwb650_ranging_state_t uwb650_ranging_state(void)
{
    if (!s_drv.initialized) return UWB650_RANGING_ERROR;
    if (s_drv.ranging_active) return UWB650_RANGING_RUNNING;
    return UWB650_RANGING_IDLE;
}

void uwb650_get_last_result(uwb650_range_result_t *out)
{
    xSemaphoreTake(s_drv.data_mutex, portMAX_DELAY);
    *out = s_drv.last_result;
    xSemaphoreGive(s_drv.data_mutex);
}

// ---- Statistics ----

void uwb650_get_stats(uwb650_stats_t *out)
{
    xSemaphoreTake(s_drv.data_mutex, portMAX_DELAY);
    *out = s_drv.stats;
    xSemaphoreGive(s_drv.data_mutex);
}

void uwb650_reset_stats(void)
{
    xSemaphoreTake(s_drv.data_mutex, portMAX_DELAY);
    memset(&s_drv.stats, 0, sizeof(s_drv.stats));
    xSemaphoreGive(s_drv.data_mutex);
    ESP_LOGI(TAG, "Statistics reset");
}
