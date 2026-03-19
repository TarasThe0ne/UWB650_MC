#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define UWB650_MAX_RESP_LEN     256
#define UWB650_CMD_TIMEOUT_MS   1000
#define UWB650_RANGE_TIMEOUT_MS 2000
#define UWB650_STARTUP_WAIT_MS  3000

// Forward declaration
typedef struct device_config_t device_config_t;

// Ranging result
typedef struct {
    float    distance_m;    // meters, -1.0 if failed
    float    rssi_dbm;      // dBm, 0.0 if failed
    bool     valid;         // true if distance >= 0
    uint32_t timestamp_ms;
    uint32_t seq;           // sequence number
} uwb650_range_result_t;

// Ranging state
typedef enum {
    UWB650_RANGING_IDLE = 0,
    UWB650_RANGING_RUNNING,
    UWB650_RANGING_ERROR,
} uwb650_ranging_state_t;

// Callback for each ranging measurement
typedef void (*uwb650_ranging_cb_t)(const uwb650_range_result_t *result);

// Driver statistics
typedef struct {
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t ok_count;
    uint32_t error_count;
    uint32_t timeout_count;
    uint32_t ranging_count;
    uint32_t ranging_ok;
    uint32_t ranging_fail;
} uwb650_stats_t;

// ---- Initialization ----
esp_err_t uwb650_init(void);
void      uwb650_deinit(void);
bool      uwb650_is_ready(void);

// ---- Low-level command ----
// Sends "UWBRFAT+{cmd}\r\n", waits for OK/ERROR.
// Response lines (before OK) are stored in resp_out.
esp_err_t uwb650_send_cmd(const char *cmd, char *resp_out, size_t resp_len,
                           uint32_t timeout_ms);

// ---- Configuration setters (each returns ESP_OK if module responds OK) ----
esp_err_t uwb650_set_baudrate(uint8_t rate);
esp_err_t uwb650_set_data_rate(uint8_t rate);
esp_err_t uwb650_set_device_id(uint16_t pan, uint16_t addr);
esp_err_t uwb650_set_power(uint8_t level);
esp_err_t uwb650_set_preamble_code(uint8_t code);
esp_err_t uwb650_set_cca(uint8_t en);
esp_err_t uwb650_set_ack(uint8_t en);
esp_err_t uwb650_set_security(uint8_t en, const char *key);
esp_err_t uwb650_set_tx_target(uint16_t pan, uint16_t addr);
esp_err_t uwb650_set_rx_show_src(uint8_t en);
esp_err_t uwb650_set_led(uint8_t en);
esp_err_t uwb650_set_rx_enable(uint8_t en);
esp_err_t uwb650_set_sniff(uint8_t en);
esp_err_t uwb650_set_ant_delay(uint16_t delay);
esp_err_t uwb650_set_dist_offset(int16_t offset_cm);
esp_err_t uwb650_set_coordinate(int32_t x, int32_t y, int32_t z);

// ---- Query (reads current value from module) ----
esp_err_t uwb650_query(const char *param, char *value, size_t value_len);

// ---- Module control ----
esp_err_t uwb650_flash_save(void);
esp_err_t uwb650_reset(void);
esp_err_t uwb650_factory_default(void);
esp_err_t uwb650_get_version(char *ver, size_t ver_len);

// ---- Apply full config from device_config ----
esp_err_t uwb650_apply_config(const void *cfg);

// ---- Ranging ----
esp_err_t uwb650_range_single(uint16_t target_pan, uint16_t target_addr,
                               uwb650_range_result_t *result);

esp_err_t uwb650_ranging_start(uint16_t target_pan, uint16_t target_addr,
                                uwb650_ranging_cb_t cb);
esp_err_t uwb650_ranging_stop(void);

uwb650_ranging_state_t uwb650_ranging_state(void);
void uwb650_get_last_result(uwb650_range_result_t *out);

// ---- Statistics ----
void uwb650_get_stats(uwb650_stats_t *out);
void uwb650_reset_stats(void);

// ---- Data transmission test ----

typedef enum {
    UWB650_DATA_IDLE = 0,
    UWB650_DATA_TRANSMITTING,
    UWB650_DATA_RECEIVING,
} uwb650_data_state_t;

typedef struct {
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    uint32_t last_seq_received;
    uint32_t start_time_ms;
    uint32_t last_rx_time_ms;
} uwb650_data_stats_t;

typedef void (*uwb650_data_rx_cb_t)(const char *data, int len);

// Start sending NMEA test data to target_addr at interval_ms
esp_err_t uwb650_data_tx_start(uint16_t target_addr, uint32_t interval_ms);

// Start receiving data (routes incoming non-AT lines to callback)
esp_err_t uwb650_data_rx_start(uwb650_data_rx_cb_t cb);

// Stop data test (either mode)
esp_err_t uwb650_data_stop(void);

uwb650_data_state_t uwb650_data_state(void);
void uwb650_data_get_stats(uwb650_data_stats_t *out);

// Write raw bytes to UART (bypasses AT prefix)
void uwb650_uart_write_raw(const void *data, size_t len);

// ---- UART Bridge mode ----
// In bridge mode, rx_task forwards raw bytes to callback instead of AT parsing
typedef void (*uwb650_bridge_rx_cb_t)(const uint8_t *data, size_t len);
esp_err_t uwb650_bridge_start(uwb650_bridge_rx_cb_t cb);
esp_err_t uwb650_bridge_stop(void);
bool uwb650_bridge_active(void);
