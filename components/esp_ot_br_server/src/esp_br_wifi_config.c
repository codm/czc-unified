/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Wi-Fi Configuration Web server for ESP Thread Border Router
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "cJSON.h"
#include "esp_br_web.h"
#include "esp_br_wifi_config.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"

#define WIFI_CONFIG_TAG "wifi_config"
#define WIFI_CONFIGURED_BIT BIT1
#define MAX_SSID_LEN 32
#define MAX_PASSWORD_LEN 64

static bool s_wifi_config_mode = false;
static httpd_handle_t s_wifi_config_server = NULL;
static EventGroupHandle_t s_wifi_event_group = NULL;
static char s_configured_ssid[32] = "";
static char s_configured_password[64] = "";

// Embedded gzipped HTML (via EMBED_FILES)
extern const uint8_t wifi_configuration_html_start[] asm("_binary_wifi_configuration_html_start");
extern const uint8_t wifi_configuration_html_end[]   asm("_binary_wifi_configuration_html_end");

// HTTP handlers for WiFi configuration
static esp_err_t wifi_config_index_handler(httpd_req_t *req)
{
    // Try to read from SPIFFS first, fallback to embedded
    FILE *fp = fopen("/spiffs/wifi_configuration.html", "rb");
    if (fp) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        char buf[1024];
        size_t read_len;
        while ((read_len = fread(buf, 1, sizeof(buf), fp)) > 0) {
            httpd_resp_send_chunk(req, buf, read_len);
        }
        fclose(fp);
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
    }

    // Fallback to embedded gzipped HTML
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_send(req, (const char *)wifi_configuration_html_start,
                    wifi_configuration_html_end - wifi_configuration_html_start);
    return ESP_OK;
}

static esp_err_t wifi_config_submit_handler(httpd_req_t *req)
{
    char *buf = NULL;
    size_t buf_len = req->content_len;

    if (buf_len > 1024) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
        return ESP_ERR_INVALID_ARG;
    }

    buf = malloc(buf_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate memory");
        return ESP_ERR_NO_MEM;
    }

    int recv_len = httpd_req_recv(req, buf, buf_len);
    if (recv_len <= 0) {
        free(buf);
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
            return ESP_ERR_TIMEOUT;
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive request");
            return ESP_FAIL;
        }
    }
    buf[recv_len] = '\0';

    // Parse JSON
    cJSON *json = cJSON_Parse(buf);
    free(buf);
    buf = NULL;
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(json, "ssid");
    cJSON *password_item = cJSON_GetObjectItemCaseSensitive(json, "password");

    if (!cJSON_IsString(ssid_item) || !ssid_item->valuestring || strlen(ssid_item->valuestring) >= MAX_SSID_LEN + 1) {
        cJSON_Delete(json);
        httpd_resp_send(req, "{\"success\":false,\"error\":\"Invalid SSID\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    const char *ssid = ssid_item->valuestring;
    const char *password = NULL;
    if (cJSON_IsString(password_item) && password_item->valuestring &&
        strlen(password_item->valuestring) < MAX_PASSWORD_LEN + 1) {
        password = password_item->valuestring;
    }

    // Store configured WiFi credentials in memory (will be saved to NVS by caller)
    strncpy(s_configured_ssid, ssid, sizeof(s_configured_ssid) - 1);
    s_configured_ssid[sizeof(s_configured_ssid) - 1] = '\0';
    if (password && strlen(password) > 0) {
        strncpy(s_configured_password, password, sizeof(s_configured_password) - 1);
        s_configured_password[sizeof(s_configured_password) - 1] = '\0';
    } else {
        s_configured_password[0] = '\0';
    }
    // Signal configuration completion
    if (s_wifi_event_group) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONFIGURED_BIT);
    }

    ESP_LOGI(WIFI_CONFIG_TAG, "WiFi configuration received: SSID=%s", s_configured_ssid);

    cJSON_Delete(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static esp_err_t wifi_config_icon_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// Captive portal handler
static esp_err_t wifi_config_captive_portal_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// Start WiFi configuration Web server
esp_err_t wifi_config_start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.recv_wait_timeout = 15;
    config.send_wait_timeout = 15;

    ESP_RETURN_ON_ERROR(httpd_start(&s_wifi_config_server, &config), WIFI_CONFIG_TAG,
                        "Failed to start WiFi config server");

    // Register URI handlers
    httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = wifi_config_index_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_wifi_config_server, &index_uri);

    httpd_uri_t submit_uri = {
        .uri = "/submit", .method = HTTP_POST, .handler = wifi_config_submit_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_wifi_config_server, &submit_uri);

    // Register icon handlers to avoid 404 warnings
    const char *icon_urls[] = {"/favicon.ico", "/apple-touch-icon.png", "/apple-touch-icon-precomposed.png",
                               "/apple-touch-icon-120x120.png", "/apple-touch-icon-120x120-precomposed.png"};

    for (int i = 0; i < sizeof(icon_urls) / sizeof(icon_urls[0]); i++) {
        httpd_uri_t icon_uri = {
            .uri = icon_urls[i], .method = HTTP_GET, .handler = wifi_config_icon_handler, .user_ctx = NULL};
        httpd_register_uri_handler(s_wifi_config_server, &icon_uri);
    }

    // Register captive portal URLs
    const char *captive_urls[] = {"/hotspot-detect.html",      "/generate_204", "/mobile/status.php",
                                  "/check_network_status.txt", "/ncsi.txt",     "/fwlink/",
                                  "/connectivity-check.html",  "/success.txt",  "/portal.html",
                                  "/library/test/success.html"};

    for (int i = 0; i < sizeof(captive_urls) / sizeof(captive_urls[0]); i++) {
        httpd_uri_t captive_uri = {.uri = captive_urls[i],
                                   .method = HTTP_GET,
                                   .handler = wifi_config_captive_portal_handler,
                                   .user_ctx = NULL};
        httpd_register_uri_handler(s_wifi_config_server, &captive_uri);
    }

    ESP_LOGI(WIFI_CONFIG_TAG, "WiFi configuration Web server started");
    return ESP_OK;
}

void wifi_config_stop_webserver(void)
{
    if (s_wifi_config_server) {
        httpd_stop(s_wifi_config_server);
        s_wifi_config_server = NULL;
    }
}

esp_err_t esp_br_wifi_config_start(void)
{
    ESP_RETURN_ON_FALSE(!s_wifi_config_mode, ESP_OK, WIFI_CONFIG_TAG, "WiFi config mode already started");

    esp_err_t ret = ESP_OK;

    // Create event group (for WiFi connection status if needed)
    if (!s_wifi_event_group) {
        s_wifi_event_group = xEventGroupCreate();
        ESP_GOTO_ON_FALSE(s_wifi_event_group != NULL, ESP_ERR_NO_MEM, cleanup, WIFI_CONFIG_TAG,
                          "Failed to create event group");
    }

    // Start Web server
    ESP_GOTO_ON_ERROR(wifi_config_start_webserver(), cleanup, WIFI_CONFIG_TAG, "Failed to start Web server");

    s_wifi_config_mode = true;
    ESP_LOGI(WIFI_CONFIG_TAG, "WiFi configuration mode started");
    ESP_LOGI(WIFI_CONFIG_TAG, "Access web interface at: http://192.168.4.1");

    return ESP_OK;

cleanup:
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }
    return ret;
}

esp_err_t esp_br_wifi_config_stop(void)
{
    if (!s_wifi_config_mode) {
        return ESP_OK;
    }

    wifi_config_stop_webserver();

    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    s_wifi_config_mode = false;
    // Clear configured WiFi info when stopping
    s_configured_ssid[0] = '\0';
    s_configured_password[0] = '\0';

    ESP_LOGI(WIFI_CONFIG_TAG, "WiFi configuration mode stopped");
    return ESP_OK;
}

esp_err_t esp_br_wifi_config_get_configured_wifi(char *ssid, size_t ssid_len, char *password, size_t password_len,
                                                 uint32_t timeout_ms)
{
    if (!s_wifi_config_mode) {
        return ESP_ERR_INVALID_STATE;
    }

    assert(s_wifi_event_group);

    // If not yet configured, wait for configuration event
    if (s_configured_ssid[0] == '\0') {
        // Clear configured bit before waiting
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONFIGURED_BIT);

        // Wait for configuration event
        TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONFIGURED_BIT, pdTRUE, pdFALSE, timeout_ticks);

        if (!(bits & WIFI_CONFIGURED_BIT)) {
            return ESP_ERR_TIMEOUT;
        }
    }

    // Copy configured WiFi credentials to output buffers
    if (ssid && ssid_len > 0) {
        strncpy(ssid, s_configured_ssid, ssid_len - 1);
        ssid[ssid_len - 1] = '\0';
        ESP_LOGI("HAR", "ssid copied");
    }

    if (password && password_len > 0) {
        strncpy(password, s_configured_password, password_len - 1);
        password[password_len - 1] = '\0';
        ESP_LOGI("HAR", "pswd copied");
    }

    ESP_LOGI("CHECK CHECK", "Saving password... ssid len: %s, %d , psw len: %s, %d", ssid, ssid_len, password, password_len);

    return ESP_OK;
}

bool esp_br_wifi_config_is_active(void)
{
    return s_wifi_config_mode;
}
