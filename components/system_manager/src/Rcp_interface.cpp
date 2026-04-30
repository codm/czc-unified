#include "Rcp_interface.h"

const char* Rcp_interface::TAG = "RCP-Interface";

Rcp_interface::Rcp_interface(/* args */)
{
}

Rcp_interface::~Rcp_interface()
{
}

esp_err_t Rcp_interface::rcp_update_init(uart_port_t _rcp_uart)
{
    this->rcp_uart = _rcp_uart;
    return ESP_OK;
}

esp_err_t Rcp_interface::rcp_update_run()
{
    return ESP_OK;
}

esp_err_t Rcp_interface::download_image(const char *url)
{
        esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .max_redirection_count = 5,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .buffer_size = HTTP_READ_BUFFER_SIZE,
        .buffer_size_tx = 512,
        .skip_cert_common_name_check = true,
        .crt_bundle_attach = NULL, 
        .keep_alive_enable = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "esp_http_client_init failed");
        return ESP_ERR_HTTP_CONNECT;
    }

    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
    esp_http_client_set_header(client, "Connection",   "close");

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return ESP_ERR_HTTP_CONNECT;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int http_status = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "[HTTP] Status: %d, Content-Length: %d", http_status, content_length);
    
    if (http_status != 200) {
        ESP_LOGE(TAG, "[HTTP] GET failed, Status: %d", http_status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_HTTP_READ_TIMEOUT;
    }

    if (content_length > 0 && !enough_spiffs_space(content_length)) {
        ESP_LOGE(TAG, "[SPIFFS] not enough spiffs space, need: %d Bytes", content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_SIZE;
    }

    mkdir_if_missing(FIRMWARE_DIR);

    FILE *fw_file = fopen(FIRMWARE_PATH, "wb");   /* "wb" = write binary  */
    if (fw_file == NULL) {
        ESP_LOGE(TAG, "[SPIFFS] error opening file: %s", FIRMWARE_PATH);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_NOT_ALLOWED;
    }

    ESP_LOGI(TAG, "[HTTP] Downloading...");
 
    uint8_t read_buffer[HTTP_READ_BUFFER_SIZE];
    int remaining = content_length;
    // int total_length = content_length;
    // float prev_percent = 0.0f;
    bool download_ok = true;

    while (remaining != 0) {
 
        int to_read = (remaining > 0 && remaining < HTTP_READ_BUFFER_SIZE)? remaining : HTTP_READ_BUFFER_SIZE;

        int bytes_read = esp_http_client_read(client, (char *)read_buffer, to_read);
 
        if (bytes_read < 0) {
            ESP_LOGE(TAG, "[HTTP] read error: %d", bytes_read);
            download_ok = false;
            break;
        }
        if (bytes_read == 0) {
            break;
        }
 
        if (fwrite(read_buffer, 1, bytes_read, fw_file) != (size_t)bytes_read) {
            ESP_LOGE(TAG, "[SPIFFS] write error");
            download_ok = false;
            break;
        }
 
        if (remaining > 0) remaining -= bytes_read;
 
        taskYIELD();
    }
 
    fclose(fw_file);
 
    if (!download_ok) {
        remove(FIRMWARE_PATH);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_HTTP_WRITE_DATA;
    }
 
    ESP_LOGI(TAG, "[HTTP] Download finished, %s", FIRMWARE_PATH);
 
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
 
    return ESP_OK;
}

esp_err_t Rcp_interface::flash_image()
{
    FILE *fw_file = fopen(FIRMWARE_PATH, "rb");
    if (fw_file == NULL) {
        ESP_LOGE(TAG, "Couldnt open file: %s", FIRMWARE_PATH);
        return ESP_ERR_FLASH_BASE;
    }

    fseek(fw_file, 0, SEEK_END);
    int total_size = (int)ftell(fw_file);
    fseek(fw_file, 0, SEEK_SET);
 
    if (total_size <= 0) {
        ESP_LOGE(TAG, "Bad file size: %d", total_size);
        fclose(fw_file);
        return false;
    }

        ESP_LOGI(TAG, "Firmware-Größe: %d Bytes", total_size);
 
    ESP_LOGI(TAG, "[FLASH] Erase ...");
    if (bsl_erase_flash() != ESP_OK) {
        ESP_LOGE(TAG, "[FLASH] Erase failed");
        fclose(fw_file);
        return false;
    }
    ESP_LOGI(TAG, "[FLASH] Erase successful!");

    if (bsl_begin_flash(BEGIN_ZB_ADDR, (uint32_t)total_size) != ESP_OK) {
        ESP_LOGE(TAG, "[FLASH] beginFlash failed");
        fclose(fw_file);
        return false;
    }

    uint8_t block_buffer[BSL_TRANSFER_SIZE];
    int loaded_size = 0;
    // float prev_percent = 0.0f;
    bool flash_ok = true;

    while (loaded_size < total_size) {
        size_t to_read = sizeof(block_buffer);
        size_t remaining = (size_t)(total_size - loaded_size);
        if (remaining < to_read) to_read = remaining;

        size_t bytes_read = fread(block_buffer, 1, to_read, fw_file);
        if (bytes_read == 0) {
            if (feof(fw_file)) {
                break;
            }
            ESP_LOGE(TAG, "[FLASH] Read file error");
            flash_ok = false;
            break;
        }
 
        if (bsl_process_flash(block_buffer, (int)bytes_read) != ESP_OK) {
            ESP_LOGE(TAG, "[FLASH] Write error at offset: %d", loaded_size);
            flash_ok = false;
            break;
        }
 
        taskYIELD();
    }
 
    fclose(fw_file);
 
    if (!flash_ok) {
        ESP_LOGE(TAG, "[FLASH] canceled");
        return false;
    }
 
    ESP_LOGI(TAG, "[FLASH] Finished!");

    if (bsl_reset_target() != ESP_OK) {
        ESP_LOGW(TAG, "[FLASH] Reset-Cmd failed");
    }
 
    return ESP_OK;
}

bool Rcp_interface::enough_spiffs_space(uint8_t required_space)
{
    size_t total_bytes = 0;
    size_t used_bytes = 0;
    esp_spiffs_info("spiffs", &total_bytes, &used_bytes);
    ESP_LOGI(TAG, "Checked SPIFFS: Total Bytes: %d, Used Bytes: %d", total_bytes, used_bytes);

    if (used_bytes < required_space)
    {
        return false;
    }
    
    return true;
}

void Rcp_interface::mkdir_if_missing(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        if (mkdir(path, 0775) != 0 && errno != EEXIST) {
            ESP_LOGE(TAG, "mkdir(%s) failed: %s", path, strerror(errno));
        }
    }
}

esp_err_t Rcp_interface::bsl_enter_bootloader(void)
{
    if(bsl_mode == false) {
        ESP_RETURN_ON_ERROR(gpio_set_level(RST_PIN, 0), TAG, "Error on RST set");
        ESP_RETURN_ON_ERROR(gpio_set_level(BSL_PIN, 0), TAG, "Error on BSL set");
        vTaskDelay(pdMS_TO_TICKS(50));
        ESP_RETURN_ON_ERROR(gpio_set_level(RST_PIN, 1), TAG, "Error on BSL set");
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP_RETURN_ON_ERROR(gpio_set_level(BSL_PIN, 1), TAG, "Error on BSL set");
    }

    bsl_mode = true;
    return ESP_OK;
}

esp_err_t Rcp_interface::bsl_erase_flash(void)
{
    if (bsl_mode == false) {
        bsl_enter_bootloader();
    }
    return esp_err_t();
}

esp_err_t Rcp_interface::bsl_begin_flash(uint32_t address, uint32_t size)
{
    return esp_err_t();
}

esp_err_t Rcp_interface::bsl_process_flash(const uint8_t *data, int length)
{
    return esp_err_t();
}

esp_err_t Rcp_interface::bsl_reset_target(void)
{
    ESP_RETURN_ON_ERROR(gpio_set_level(RST_PIN, 0), TAG, "Error on RST set");
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_RETURN_ON_ERROR(gpio_set_level(RST_PIN, 1), TAG, "Error on RST set");
    vTaskDelay(pdMS_TO_TICKS(500));
    bsl_mode = false;

    return ESP_OK;
}

esp_err_t Rcp_interface::bsl_send_packet(const uint8_t *cmd_and_data, size_t length)
{
    return esp_err_t();
}

bool Rcp_interface::bsl_wait_ack(uint32_t timeout_ms)
{
    unsigned long start_millis = xTaskGetTickCount();
    while (xTaskGetTickCount() - start_millis < pdMS_TO_TICKS(timeout_ms))
    {
        size_t buffer_len = 0;
        uart_get_buffered_data_len(rcp_uart, &buffer_len);
        if(buffer_len >= 1) {
            uint8_t received_data = uart_read_bytes(rcp_uart, &received_data, sizeof(received_data), pdMS_TO_TICKS(10));
            if (received_data == BSL_ACK) {
                return true;
            }
            else if (received_data == BSL_NACK) {
                return false;
            }
        }
        taskYIELD();
    }
    
    ESP_LOGW(TAG, "Timeout waiting for ACK/NACK");
    return false;
}

esp_err_t Rcp_interface::bsl_read_response(uint8_t *out_buf, size_t buf_size, size_t *out_len)
{
    return esp_err_t();
}
