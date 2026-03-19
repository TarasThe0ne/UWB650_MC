#pragma once

// UWB650 UART connection (UART2)
#define UWB_UART_NUM        UART_NUM_2
#define UWB_UART_TX_PIN     17      // ESP32 TX -> UWB650 RX
#define UWB_UART_RX_PIN     18      // ESP32 RX <- UWB650 TX
#define UWB_UART_BAUD       115200
#define UWB_UART_BUF_SIZE   1024
