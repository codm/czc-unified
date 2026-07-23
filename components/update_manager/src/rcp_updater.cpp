#include "rcp_updater.h"

#include "board_config.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sse_events.hpp"

static const char* TAG = "RcpUpdater";

constexpr size_t HTTP_BUF_SIZE {1024};
constexpr size_t BSL_BLOCK_SIZE {252};
constexpr uint32_t FLASH_START_ADDR {0x00000000};
constexpr int MAX_REDIRECTS {5};

constexpr float PERCENT_PRINT_DELTA {0.5};

RcpUpdater::RcpUpdater()
    : hal{}, downloadedSize{0}
{}

esp_err_t RcpUpdater::flash(const char* url)
{
    Sse_events::flash::post_device_state(SseDeviceMode::FLASHING, nullptr);

    ESP_RETURN_ON_ERROR(hal.init(Board::RCP_UART), TAG, "HAL init failed");

    esp_err_t ret = downloadToStaging(url);
    if (ret == ESP_OK) {
        ret = flashFromStaging();
    }
    hal.close();

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Flash failed during RCP write!");
        Sse_events::flash::post_flash_complete(SseFlashTarget::RCP, false, "Flash failed during write!");
        return ESP_FAIL;
    }
    Sse_events::flash::post_flash_complete(SseFlashTarget::RCP, true);
    Sse_events::flash::post_device_state(SseDeviceMode::NORMAL, nullptr);

    return ESP_OK;
}

esp_err_t RcpUpdater::downloadToStaging(const char* url)
{
    esp_http_client_config_t httpCfg{};
    httpCfg.url                   = url;
    httpCfg.method                = HTTP_METHOD_GET;
    httpCfg.max_redirection_count = MAX_REDIRECTS;
    httpCfg.transport_type        = HTTP_TRANSPORT_OVER_SSL;
    httpCfg.buffer_size           = HTTP_BUF_SIZE;
    httpCfg.buffer_size_tx        = 2 * 1024;
    httpCfg.crt_bundle_attach     = esp_crt_bundle_attach;
    httpCfg.keep_alive_enable     = false;

    esp_http_client_handle_t client{esp_http_client_init(&httpCfg)};
    if (client == nullptr) {
        return ESP_ERR_HTTP_CONNECT;
    }

    esp_http_client_set_header(client, "Connection", "close");

    esp_err_t err{esp_http_client_open(client, 0)};
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return ESP_ERR_HTTP_CONNECT;
    }

    int64_t contentLength{esp_http_client_fetch_headers(client)};
    int     httpStatus{esp_http_client_get_status_code(client)};

    // Manual open/read does not follow redirects — handle them explicitly
    for (int redirects{0};
         (httpStatus == 301 || httpStatus == 302 || httpStatus == 303 ||
          httpStatus == 307 || httpStatus == 308) && redirects < MAX_REDIRECTS;
         redirects++) {

        ESP_LOGI(TAG, "Redirect %d (HTTP %d)", redirects + 1, httpStatus);
        err = esp_http_client_set_redirection(client);
        if (err != ESP_OK || esp_http_client_open(client, 0) != ESP_OK) {
            esp_http_client_cleanup(client);
            return ESP_ERR_HTTP_CONNECT;
        }
        contentLength = static_cast<int64_t>(esp_http_client_fetch_headers(client));
        httpStatus    = esp_http_client_get_status_code(client);
    }

    if (httpStatus != 200) {
        ESP_LOGE(TAG, "HTTP %d", httpStatus);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_HTTP_READ_TIMEOUT;
    }

    const esp_partition_t* stagingPart{esp_ota_get_next_update_partition(nullptr)};
    if (stagingPart == nullptr) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Staging partition '%s' at 0x%08lx (%lu B)",
             stagingPart->label, stagingPart->address, stagingPart->size);

    if (contentLength > 0 && static_cast<size_t>(contentLength) > stagingPart->size) {
        ESP_LOGE(TAG, "Firmware too large: %lld > %lu", contentLength, stagingPart->size);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_SIZE;
    }

    err = esp_partition_erase_range(stagingPart, 0, stagingPart->size);
    if (err != ESP_OK) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return err;
    }

    uint8_t readBuf[HTTP_BUF_SIZE]{};
    int64_t remaining{contentLength};
    size_t offset{0};
    bool downloadOk{true};
    float lastLoggedPercent {0};

    while (remaining != 0) {
        int toRead{(remaining > 0 && remaining < static_cast<int64_t>(HTTP_BUF_SIZE))
                   ? static_cast<int>(remaining)
                   : static_cast<int>(HTTP_BUF_SIZE)};

        int bytesRead{esp_http_client_read(client, reinterpret_cast<char*>(readBuf), toRead)};
        if (bytesRead < 0) { downloadOk = false; break; }
        if (bytesRead == 0) { break; }

        err = esp_partition_write(stagingPart, offset, readBuf, static_cast<size_t>(bytesRead));
        if (err != ESP_OK) { downloadOk = false; break; }

        offset += static_cast<size_t>(bytesRead);
        if (remaining > 0) 
            remaining -= bytesRead;

        float progressPercent {(static_cast<float>(offset) / contentLength) * 100};
        if ((progressPercent - lastLoggedPercent) > PERCENT_PRINT_DELTA) 
        {
            Sse_events::flash::post_flash_progress(SseFlashTarget::RCP, SseFlashPhase::DOWNLOADING, progressPercent);
            ESP_LOGD(TAG, "RCP Flash in progress: %.2f%% Done!", progressPercent);
            lastLoggedPercent = progressPercent;
        }
        vTaskDelay(1);
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (!downloadOk) {
        return ESP_ERR_HTTP_WRITE_DATA;
    }

    downloadedSize = offset;
    ESP_LOGI(TAG, "Download complete: %u bytes", downloadedSize);
    return ESP_OK;
}

esp_err_t RcpUpdater::flashFromStaging()
{
    if (downloadedSize == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_partition_t* stagingPart{esp_ota_get_next_update_partition(nullptr)};
    if (stagingPart == nullptr) {
        return ESP_FAIL;
    }

    uint32_t totalSize{static_cast<uint32_t>(downloadedSize)};
    // BSL DOWNLOAD requires size to be 4-byte aligned
    uint32_t alignedSize{(totalSize + 3) & ~3u};

    ESP_LOGI(TAG, "Flashing %lu B (aligned: %lu B) to CC2652...", totalSize, alignedSize);

    ESP_RETURN_ON_ERROR(hal.eraseFlash(),                       TAG, "Erase failed");
    ESP_LOGI(TAG, "BEGIN_FLASH...");
    ESP_RETURN_ON_ERROR(hal.beginFlash(FLASH_START_ADDR, alignedSize), TAG, "beginFlash failed");

    uint8_t blockBuf[BSL_BLOCK_SIZE]{};
    size_t offset{0};
    float lastLoggedPercent {0};

    while (offset < static_cast<size_t>(totalSize)) {
        size_t toRead{BSL_BLOCK_SIZE};
        size_t remaining{static_cast<size_t>(totalSize) - offset};
        if (remaining < toRead) 
            toRead = remaining;

        // Pad last block to 4-byte boundary with 0xFF
        size_t paddedRead{(toRead + 3) & ~size_t{3}};
        memset(blockBuf, 0xFF, paddedRead);

        esp_err_t err{esp_partition_read(stagingPart, offset, blockBuf, toRead)};
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Partition read error at offset %u: %s", offset, esp_err_to_name(err));
            return err;
        }

        if (hal.sendData(blockBuf, static_cast<int>(paddedRead)) != ESP_OK) {
            ESP_LOGE(TAG, "sendData failed at offset %u", offset);
            return ESP_FAIL;
        }

        offset += toRead;
        vTaskDelay(1);
        
        float progressPercent {(static_cast<float>(offset) / totalSize) * 100};
        if ((progressPercent - lastLoggedPercent) > PERCENT_PRINT_DELTA) 
        {
            Sse_events::flash::post_flash_progress(SseFlashTarget::RCP, SseFlashPhase::WRITING, progressPercent);
            ESP_LOGD(TAG, "RCP Flash in progress: %.2f%% Done!", progressPercent);
            lastLoggedPercent = progressPercent;
        }
    }

    ESP_LOGI(TAG, "Flash complete — resetting CC2652");
    return hal.reset();
}
