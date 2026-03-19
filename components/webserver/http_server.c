#include "webserver.h"
#include "pages.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "httpd";

static httpd_handle_t s_server = NULL;

// Forward declarations from other files
extern void ws_handler_init(httpd_handle_t server);
extern void rest_api_init(httpd_handle_t server);

// ---- Page handlers ----

static esp_err_t page_handler(httpd_req_t *req)
{
    const char *page = (const char *)req->user_ctx;
    httpd_resp_set_type(req, "text/html; charset=UTF-8");
    return httpd_resp_send(req, page, strlen(page));
}

static esp_err_t favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/svg+xml");
    return httpd_resp_send(req, PAGE_FAVICON, strlen(PAGE_FAVICON));
}

// ---- Public API ----

esp_err_t webserver_init(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers  = 32;
    config.max_open_sockets  = 8;
    config.stack_size        = 8192;
    config.lru_purge_enable  = true;
    config.uri_match_fn      = httpd_uri_match_wildcard;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    // Register page handlers
    static const httpd_uri_t pages[] = {
        { .uri = "/",         .method = HTTP_GET, .handler = page_handler, .user_ctx = (void *)PAGE_DASHBOARD },
        { .uri = "/settings", .method = HTTP_GET, .handler = page_handler, .user_ctx = (void *)PAGE_SETTINGS },
        { .uri = "/wifi",     .method = HTTP_GET, .handler = page_handler, .user_ctx = (void *)PAGE_WIFI },
        { .uri = "/datatest", .method = HTTP_GET, .handler = page_handler, .user_ctx = (void *)PAGE_DATATEST },
    };
    for (int i = 0; i < sizeof(pages) / sizeof(pages[0]); i++) {
        httpd_register_uri_handler(s_server, &pages[i]);
    }

    // Favicon
    static const httpd_uri_t fav = {
        .uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_handler
    };
    httpd_register_uri_handler(s_server, &fav);

    // Initialize WebSocket and REST API
    ws_handler_init(s_server);
    rest_api_init(s_server);

    ESP_LOGI(TAG, "HTTP server started on port %d (%d URI handlers max)",
             config.server_port, config.max_uri_handlers);
    return ESP_OK;
}

void webserver_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}
