#include "webserver.h"
#include "uwb650_driver.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <errno.h>

static const char *TAG = "bridge";

#define BRIDGE_PORT     3333
#define BRIDGE_STACK    4096
#define BRIDGE_PRIO     5

static int s_server_fd = -1;
static int s_client_fd = -1;
static TaskHandle_t s_bridge_task = NULL;
static bool s_running = false;

// Called by driver rx_task when bridge mode is on — forward UART data to TCP client
static void bridge_uart_rx_cb(const uint8_t *data, size_t len)
{
    if (s_client_fd >= 0) {
        send(s_client_fd, data, len, MSG_DONTWAIT);
    }
}

static void bridge_task(void *arg)
{
    ESP_LOGI(TAG, "TCP bridge task started on port %d", BRIDGE_PORT);

    // Create TCP server socket
    s_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_server_fd < 0) {
        ESP_LOGE(TAG, "Failed to create socket: %d", errno);
        s_running = false;
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(s_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(BRIDGE_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(s_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Bind failed: %d", errno);
        close(s_server_fd);
        s_server_fd = -1;
        s_running = false;
        vTaskDelete(NULL);
        return;
    }

    listen(s_server_fd, 1);
    ESP_LOGI(TAG, "Listening on port %d...", BRIDGE_PORT);

    while (s_running) {
        // Wait for client connection (with timeout via select)
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(s_server_fd, &fds);
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };

        int sel = select(s_server_fd + 1, &fds, NULL, NULL, &tv);
        if (sel <= 0) continue;
        if (!s_running) break;

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        s_client_fd = accept(s_server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (s_client_fd < 0) continue;

        ESP_LOGI(TAG, "Client connected (fd=%d)", s_client_fd);

        // Enable bridge mode in driver
        uwb650_bridge_start(bridge_uart_rx_cb);

        // Read from TCP, write to UART
        uint8_t buf[256];
        while (s_running) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(s_client_fd, &rfds);
            struct timeval rtv = { .tv_sec = 1, .tv_usec = 0 };

            int rsel = select(s_client_fd + 1, &rfds, NULL, NULL, &rtv);
            if (rsel < 0) break;
            if (rsel == 0) continue;

            int n = recv(s_client_fd, buf, sizeof(buf), 0);
            if (n <= 0) {
                ESP_LOGI(TAG, "Client disconnected");
                break;
            }

            // Forward TCP data to UART
            uwb650_uart_write_raw(buf, n);
        }

        // Disable bridge mode
        uwb650_bridge_stop();
        close(s_client_fd);
        s_client_fd = -1;
    }

    close(s_server_fd);
    s_server_fd = -1;
    s_bridge_task = NULL;
    ESP_LOGI(TAG, "TCP bridge task stopped");
    vTaskDelete(NULL);
}

esp_err_t uart_bridge_start(void)
{
    if (s_running) return ESP_OK;

    s_running = true;
    BaseType_t ok = xTaskCreate(bridge_task, "bridge", BRIDGE_STACK, NULL,
                                 BRIDGE_PRIO, &s_bridge_task);
    if (ok != pdPASS) {
        s_running = false;
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t uart_bridge_stop(void)
{
    if (!s_running) return ESP_OK;

    s_running = false;

    // Close client connection to unblock recv
    if (s_client_fd >= 0) {
        shutdown(s_client_fd, SHUT_RDWR);
        close(s_client_fd);
        s_client_fd = -1;
    }

    // Wait for task to finish
    for (int i = 0; i < 30 && s_bridge_task != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    uwb650_bridge_stop();
    return ESP_OK;
}

bool uart_bridge_is_active(void)
{
    return s_running;
}

bool uart_bridge_client_connected(void)
{
    return s_client_fd >= 0;
}
