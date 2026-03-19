#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define CONFIG_MAGIC        0x55574230  // "UWB0"
#define CONFIG_VERSION      1
#define CONFIG_NVS_NS       "uwb650mc"
#define CONFIG_NVS_KEY      "config"

// UWB650 baud rate codes
#define UWB_BAUD_230400     0
#define UWB_BAUD_115200     1   // default
#define UWB_BAUD_57600      2
#define UWB_BAUD_38400      3
#define UWB_BAUD_19200      4
#define UWB_BAUD_9600       5

// UWB650 data rate codes
#define UWB_RATE_850K       0
#define UWB_RATE_6M8        1   // default

// UWB650 TX power levels (index -> dBm)
// 0=-5, 1=-2.5, 2=0, 3=2.5, 4=5, 5=7.5, 6=10, 7=12.5, 8=15, 9=20, 10=27.7
#define UWB_POWER_MIN       0
#define UWB_POWER_MAX       10  // 27.7 dBm

// Preamble code range
#define UWB_PREAMBLE_MIN    9
#define UWB_PREAMBLE_MAX    24

// Antenna delay range
#define UWB_ANTDELAY_MIN    0
#define UWB_ANTDELAY_MAX    65535
#define UWB_ANTDELAY_DEF    16400

// Distance offset range (cm)
#define UWB_DISTOFFSET_MIN  (-500)
#define UWB_DISTOFFSET_MAX  500

typedef struct device_config_t {
    uint32_t magic;
    uint32_t version;

    // UWB650 parameters
    uint8_t  baudrate;          // 0-5 (default 1 = 115200)
    uint8_t  data_rate;         // 0=850K, 1=6.8M (default 1)
    uint16_t pan_id;            // 0x0000-0xFFFE (default 0x0000)
    uint16_t node_addr;         // 0x0000-0xFFFE (default 0x0000)
    uint8_t  tx_power;          // 0-10 (default 10 = 27.7 dBm)
    uint8_t  preamble_code;     // 9-24 (default 9)
    uint8_t  cca_enable;        // 0/1 (default 0)
    uint8_t  ack_enable;        // 0/1 (default 0)
    uint8_t  security_enable;   // 0/1 (default 0)
    char     security_key[33];  // 32 hex chars + null
    uint8_t  rx_show_src;       // 0/1 (default 1)
    uint8_t  led_status;        // 0/1 (default 1)
    uint8_t  rx_enable;         // 0/1 (default 1)
    uint8_t  sniff_enable;      // 0/1 (default 0)
    uint16_t ant_delay;         // 0-65535 (default 16400)
    int16_t  dist_offset_cm;    // -500..500 (default 0)
    int32_t  coord_x_cm;       // 0-100000 (default 0)
    int32_t  coord_y_cm;       // 0-100000 (default 0)
    int32_t  coord_z_cm;       // 0-100000 (default 0)

    // Ranging target
    uint16_t target_pan_id;     // 0x0000-0xFFFF (default 0x1111)
    uint16_t target_addr;       // 0x0000-0xFFFF (default 0x0001)

    // WiFi STA credentials
    char     wifi_ssid[33];
    char     wifi_pass[65];

    uint32_t crc;
} device_config_t;

// Get pointer to global config (read-only, always valid after load)
const device_config_t *device_config_get(void);

// Get mutable pointer for modification
device_config_t *device_config_get_mut(void);

// Load from NVS (or set defaults if not found / corrupted)
esp_err_t device_config_load(void);

// Save current config to NVS
esp_err_t device_config_save(void);

// Reset config to factory defaults (does NOT save to NVS)
void device_config_set_defaults(device_config_t *cfg);

// Erase NVS and reset to defaults
esp_err_t device_config_factory_reset(void);

// Log current config to serial monitor
void device_config_log(const device_config_t *cfg);
