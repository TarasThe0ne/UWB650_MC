#include "webserver.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

static const char *TAG = "ws";

#define MAX_WS_CLIENTS  4

typedef struct {
    int  fd;
    bool active;
} ws_client_t;

static ws_client_t   s_clients[MAX_WS_CLIENTS];
static SemaphoreHandle_t s_mutex;
static httpd_handle_t    s_server;

// ---- Internal helpers ----

static void add_client(int fd)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (!s_clients[i].active) {
            s_clients[i].fd = fd;
            s_clients[i].active = true;
            ESP_LOGI(TAG, "Client connected (fd=%d, slot=%d)", fd, i);
            xSemaphoreGive(s_mutex);
            return;
        }
    }
    ESP_LOGW(TAG, "No free WS slots for fd=%d", fd);
    xSemaphoreGive(s_mutex);
}

static void remove_client(int fd)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_clients[i].active && s_clients[i].fd == fd) {
            s_clients[i].active = false;
            ESP_LOGI(TAG, "Client disconnected (fd=%d, slot=%d)", fd, i);
            break;
        }
    }
    xSemaphoreGive(s_mutex);
}

static void broadcast(const char *data, size_t len)
{
    if (!s_server) return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (!s_clients[i].active) continue;

        httpd_ws_frame_t frame = {
            .type    = HTTPD_WS_TYPE_TEXT,
            .payload = (uint8_t *)data,
            .len     = len,
        };

        esp_err_t err = httpd_ws_send_frame_async(s_server, s_clients[i].fd, &frame);
        if (err != ESP_OK) {
            ESP_LOGD(TAG, "Send failed to fd=%d: %s", s_clients[i].fd, esp_err_to_name(err));
            s_clients[i].active = false;
        }
    }
    xSemaphoreGive(s_mutex);
}

// ---- WebSocket URI handler ----

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        // New WS connection handshake
        add_client(httpd_req_to_sockfd(req));
        return ESP_OK;
    }

    // Receive frame
    httpd_ws_frame_t frame = { .type = HTTPD_WS_TYPE_TEXT };
    esp_err_t err = httpd_ws_recv_frame(req, &frame, 0);
    if (err != ESP_OK) return err;

    if (frame.len == 0) return ESP_OK;

    uint8_t *buf = calloc(1, frame.len + 1);
    if (!buf) return ESP_ERR_NO_MEM;

    frame.payload = buf;
    err = httpd_ws_recv_frame(req, &frame, frame.len);
    if (err != ESP_OK) {
        free(buf);
        return err;
    }

    // Handle close
    if (frame.type == HTTPD_WS_TYPE_CLOSE) {
        remove_client(httpd_req_to_sockfd(req));
        free(buf);
        return ESP_OK;
    }

    // Handle text: respond to "ping" with "pong"
    if (frame.type == HTTPD_WS_TYPE_TEXT) {
        if (strstr((char *)buf, "ping")) {
            const char *pong = "{\"type\":\"pong\"}";
            httpd_ws_frame_t resp = {
                .type    = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)pong,
                .len     = strlen(pong),
            };
            httpd_ws_send_frame(req, &resp);
        }
    }

    free(buf);
    return ESP_OK;
}

// ---- Close callback to detect disconnections ----

static void ws_close_cb(httpd_handle_t hd, int sockfd)
{
    remove_client(sockfd);
    close(sockfd);
}

// ---- Public API ----

void ws_handler_init(httpd_handle_t server)
{
    s_server = server;
    s_mutex = xSemaphoreCreateMutex();
    memset(s_clients, 0, sizeof(s_clients));

    static const httpd_uri_t ws_uri = {
        .uri       = "/ws",
        .method    = HTTP_GET,
        .handler   = ws_handler,
        .is_websocket = true,
    };
    httpd_register_uri_handler(server, &ws_uri);

    // Register close callback
    httpd_register_err_handler(server, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);

    ESP_LOGI(TAG, "WebSocket handler registered at /ws");
}

void webserver_ws_broadcast_ranging(const uwb650_range_result_t *r)
{
    char buf[192];
    int n = snprintf(buf, sizeof(buf),
        "{\"type\":\"ranging\",\"data\":{"
        "\"distance\":%.3f,\"rssi\":%.2f,\"valid\":%s,"
        "\"seq\":%lu,\"timestamp\":%lu}}",
        r->distance_m, r->rssi_dbm,
        r->valid ? "true" : "false",
        (unsigned long)r->seq,
        (unsigned long)r->timestamp_ms);
    broadcast(buf, n);
}

void webserver_ws_broadcast_status(void)
{
    extern bool uwb650_is_ready(void);
    extern const char *wifi_manager_get_sta_ip(void);
    extern bool wifi_manager_sta_is_connected(void);
    extern uwb650_ranging_state_t uwb650_ranging_state(void);

    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "{\"type\":\"status\",\"data\":{"
        "\"uwb\":%s,\"wifi\":%s,"
        "\"ranging\":\"%s\","
        "\"heap\":%lu,\"uptime\":%lu}}",
        uwb650_is_ready() ? "true" : "false",
        wifi_manager_sta_is_connected() ? "true" : "false",
        uwb650_ranging_state() == UWB650_RANGING_RUNNING ? "running" :
            (uwb650_ranging_state() == UWB650_RANGING_ERROR ? "error" : "idle"),
        (unsigned long)esp_get_free_heap_size(),
        (unsigned long)(esp_timer_get_time() / 1000000));
    broadcast(buf, n);
}

void webserver_ws_broadcast_log(const char *level, const char *message)
{
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "{\"type\":\"log\",\"data\":{\"level\":\"%s\",\"message\":\"%s\"}}",
        level, message);
    if (n > 0 && n < (int)sizeof(buf)) {
        broadcast(buf, n);
    }
}

void webserver_ws_broadcast_data(const char *data, int len)
{
    char buf[384];
    int n = snprintf(buf, sizeof(buf),
        "{\"type\":\"datatest\",\"data\":\"%.*s\"}", len, data);
    if (n > 0 && n < (int)sizeof(buf)) {
        broadcast(buf, n);
    }
}

int webserver_ws_client_count(void)
{
    int count = 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_clients[i].active) count++;
    }
    xSemaphoreGive(s_mutex);
    return count;
}
