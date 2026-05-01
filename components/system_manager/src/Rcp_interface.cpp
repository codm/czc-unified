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

esp_err_t Rcp_interface::rcp_update_start(const char *url)
{
    ESP_LOGD(TAG, "Starting RCP Update Task...");

    RcpUpdateParams *params = new RcpUpdateParams();
    params->self = this;
    strncpy(params->url, url, sizeof(params->url) - 1);
    params->url[sizeof(params->url) - 1] = '\0';

    BaseType_t ret = xTaskCreate(
        rcp_update_task,
        "rcp_update_task",
        1024 * 8,
        params,
        5,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RCP update task");
        delete params;
        return ESP_FAIL;
    }

    return ESP_OK;
}

void Rcp_interface::rcp_update_task(void *pvParameters)
{
    RcpUpdateParams *params = static_cast<RcpUpdateParams*>(pvParameters);
    Rcp_interface *self = params->self;

    char url[256];
    strncpy(url, params->url, sizeof(url));
    delete params;

    esp_err_t err = self->rcp_update_run(url);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "RCP update successful!");
    } else {
        ESP_LOGE(TAG, "RCP update failed: %s", esp_err_to_name(err));
    }

    vTaskDelete(NULL);
}

esp_err_t Rcp_interface::rcp_update_run(const char* url)
{
    ESP_RETURN_ON_ERROR(download_image(url), TAG, "RCP image download failed!");
    ESP_RETURN_ON_ERROR(flash_image(), TAG, "RCP image flash failed!");
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
        ESP_RETURN_ON_ERROR(bsl_enter_bootloader(), TAG, "Enter bootloader failed");
    }

    ESP_LOGI(TAG, "[BSL] Sending BANK_ERASE (0x2C)...");

    uint8_t cmd = BSL_CMD_BANK_ERASE;  // 0x2C
    ESP_RETURN_ON_ERROR(bsl_send_packet(&cmd, 1), TAG, "BANK_ERASE send failed");

    // Bank-Erase needs time - big Timeout
    if (!bsl_wait_ack(10000)) {
        ESP_LOGE(TAG, "BANK_ERASE: no ACK");
        return ESP_FAIL;
    }

    return bsl_check_last_cmd();
}

// bsl_begin_flash  (DOWNLOAD 0x21)
//
// tells Bootloader: "writing beginning at address 'address'
// in total 'size' Bytes."
// size has to be a multiple of 4!
esp_err_t Rcp_interface::bsl_begin_flash(uint32_t address, uint32_t size)
{
    if ((size % 4) != 0) {
        ESP_LOGE(TAG, "bsl_begin_flash: size %lu is not a multiple of 4", size);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "[BSL] DOWNLOAD cmd: addr=0x%08lX, size=%lu", address, size);

    // Checksum = (sum(addr_bytes) + sum(size_bytes) + cmd) & 0xFF
    // address and size are transmitted Big-Endian
    uint8_t payload[9] = {
        BSL_CMD_DOWNLOAD,                      // [0] CMD = 0x21
        (uint8_t)((address >> 24) & 0xFF),     // [1] addr MSB
        (uint8_t)((address >> 16) & 0xFF),     // [2]
        (uint8_t)((address >>  8) & 0xFF),     // [3]
        (uint8_t)((address >>  0) & 0xFF),     // [4] addr LSB
        (uint8_t)((size    >> 24) & 0xFF),     // [5] size MSB
        (uint8_t)((size    >> 16) & 0xFF),     // [6]
        (uint8_t)((size    >>  8) & 0xFF),     // [7]
        (uint8_t)((size    >>  0) & 0xFF),     // [8] size LSB
    };

    ESP_RETURN_ON_ERROR(bsl_send_packet(payload, sizeof(payload)),
                        TAG, "DOWNLOAD send failed");

    if (!bsl_wait_ack(2000)) {
        ESP_LOGE(TAG, "DOWNLOAD: no ACK");
        return ESP_FAIL;
    }

    return bsl_check_last_cmd();
}

// bsl_process_flash  (SEND_DATA 0x24)
//
// Transmits one data block. Maximum of 252 Bytes per call
// (BSL_TRANSFER_SIZE = 252 recommended)
esp_err_t Rcp_interface::bsl_process_flash(const uint8_t *data, int length)
{
    if (data == NULL || length <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // max. 252 Bytes use-data per SEND_DATA
    // (255 Bytes max SIZE - 2 Header - 1 CMD = 252)
    if (length > 252) {
        ESP_LOGE(TAG, "bsl_process_flash: block too large (%d > 252)", length);
        return ESP_ERR_INVALID_SIZE;
    }

    // build packet: [CMD, data...]
    uint8_t packet[1 + 252];
    packet[0] = BSL_CMD_SEND_DATA;  // 0x24
    memcpy(&packet[1], data, length);

    ESP_RETURN_ON_ERROR(bsl_send_packet(packet, 1 + length),
                        TAG, "SEND_DATA send failed");

    if (!bsl_wait_ack(10000)) {
        ESP_LOGE(TAG, "SEND_DATA: no ACK");
        return ESP_FAIL;
    }

    return bsl_check_last_cmd();
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

// send full BSL-packet:
//   [SIZE] [CHECKSUM] [CMD] [DATA...]
//
// Parameter cmd_and_data: Byte 0 = CMD, Byte 1..n = Payload
// SIZE = length + 2  (SIZE and CHECKSUM count as well)
// CHECKSUM = sum(cmd_and_data) & 0xFF
esp_err_t Rcp_interface::bsl_send_packet(const uint8_t *cmd_and_data, size_t length)
{
    if (length == 0 || cmd_and_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t size = (uint8_t)(length + 2); // +2 for SIZE and CHECKSUM 

    uint8_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum += cmd_and_data[i];
    }
    // checksum & 0xFF implicit through uint8_t

    // write Header: SIZE, CHECKSUM
    uint8_t header[2] = { size, checksum };
    if (uart_write_bytes(rcp_uart, (const char *)header, sizeof(header)) < 0) {
        ESP_LOGE(TAG, "UART write header failed");
        return ESP_FAIL;
    }

    // write CMD + DATA 
    if (uart_write_bytes(rcp_uart, (const char *)cmd_and_data, length) < 0) {
        ESP_LOGE(TAG, "UART write data failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

// The protocoll always sends 2-Byte-answers:
//   ACK:  0x00 0xCC
//   NACK: 0x00 0x33
bool Rcp_interface::bsl_wait_ack(uint32_t timeout_ms)
{
    TickType_t start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms))
    {
        size_t buffer_len = 0;
        uart_get_buffered_data_len(rcp_uart, &buffer_len);

        if (buffer_len >= 2) {
            uint8_t buf[2] = {0};
            int n = uart_read_bytes(rcp_uart, buf, 2, pdMS_TO_TICKS(10));
            if (n == 2) {
                if (buf[0] == 0x00 && buf[1] == BSL_ACK) {   // 0x00 0xCC
                    return true;
                } else if (buf[0] == 0x00 && buf[1] == BSL_NACK) { // 0x00 0x33
                    ESP_LOGW(TAG, "BSL NACK received");
                    return false;
                } else {
                    ESP_LOGW(TAG, "Unexpected bytes: 0x%02X 0x%02X", buf[0], buf[1]);
                }
            }
        }
        taskYIELD();
    }

    ESP_LOGW(TAG, "Timeout waiting for ACK/NACK");
    return false;
}

// reads Response-packet from CC2652:
//   [SIZE] [CHECKSUM] [DATA...]
// test Checksum, sends ACK or NACK response.
//
// out_buf: buffer for use-data (without SIZE/CHECKSUM)
// out_len: actually read bytes
esp_err_t Rcp_interface::bsl_read_response(uint8_t *out_buf, size_t buf_size, size_t *out_len)
{
    if (out_buf == NULL || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_len = 0;

    // read Header: SIZE and CHECKSUM
    uint8_t header[2] = {0};
    int n = uart_read_bytes(rcp_uart, header, 2, pdMS_TO_TICKS(500));
    if (n != 2) {
        ESP_LOGE(TAG, "bsl_read_response: header read timeout");
        return ESP_ERR_TIMEOUT;
    }

    uint8_t size = header[0]; // full size incl. SIZE+CHECKSUM
    uint8_t checksum = header[1];

    if (size < 2) {
        ESP_LOGE(TAG, "bsl_read_response: invalid header read size byte %d", size);
        return ESP_ERR_INVALID_RESPONSE;
    }

    size_t data_len = (size_t)(size - 2); // data length without SIZE and CHECKSUM

    if (data_len > buf_size) {
        ESP_LOGE(TAG, "bsl_read_response: buffer too small (%d > %d)", data_len, buf_size);
        return ESP_ERR_INVALID_SIZE;
    }

    // read use-data
    if (data_len > 0) {
        n = uart_read_bytes(rcp_uart, out_buf, data_len, pdMS_TO_TICKS(500));
        if (n != (int)data_len) {
            ESP_LOGE(TAG, "bsl_read_response: data read incomplete (%d/%d)", n, data_len);
            // NACK senden
            uint8_t nack[2] = {0x00, BSL_NACK};
            uart_write_bytes(rcp_uart, (const char *)nack, 2);
            return ESP_ERR_TIMEOUT;
        }
    }

    // Checksum 
    uint8_t calc_sum = 0;
    for (size_t i = 0; i < data_len; i++) {
        calc_sum += out_buf[i];
    }

    if (calc_sum != checksum) {
        ESP_LOGE(TAG, "bsl_read_response: checksum mismatch (got 0x%02X, expected 0x%02X)",
                 calc_sum, checksum);
        uint8_t nack[2] = {0x00, BSL_NACK};
        uart_write_bytes(rcp_uart, (const char *)nack, 2);
        return ESP_ERR_INVALID_CRC;
    }

    // send ACK
    uint8_t ack[2] = {0x00, BSL_ACK};
    uart_write_bytes(rcp_uart, (const char *)ack, 2);

    *out_len = data_len;
    return ESP_OK;
}

// send GET_STATUS (0x23) and react on response.
// checkLastCmd() in Python-script
// Returns ESP_OK when 0x40 (Success), else ESP_FAIL.
esp_err_t Rcp_interface::bsl_check_last_cmd(void)
{
    uint8_t cmd = BSL_CMD_GET_STATUS;  // 0x23
    ESP_RETURN_ON_ERROR(bsl_send_packet(&cmd, 1), TAG, "GET_STATUS send failed");

    if (!bsl_wait_ack(500)) {
        ESP_LOGE(TAG, "GET_STATUS: no ACK");
        return ESP_FAIL;
    }

    uint8_t resp[4] = {0};
    size_t  resp_len = 0;
    ESP_RETURN_ON_ERROR(bsl_read_response(resp, sizeof(resp), &resp_len),
                        TAG, "GET_STATUS read response failed");

    if (resp_len < 1) {
        ESP_LOGE(TAG, "GET_STATUS: empty response");
        return ESP_FAIL;
    }

    if (resp[0] == 0x40) {
        ESP_LOGD(TAG, "BSL: last command successful");
        return ESP_OK;
    }

    // error respose
    const char *err_str = "unknown";
    switch (resp[0]) {
        case 0x41: err_str = "Unknown command";  break;
        case 0x42: err_str = "Invalid command";  break;
        case 0x43: err_str = "Invalid address";  break;
        case 0x44: err_str = "Flash fail";       break;
    }
    ESP_LOGE(TAG, "BSL error: 0x%02X (%s)", resp[0], err_str);
    return ESP_FAIL;
}

// checksum = (sum(data_bytes) + cmd) & 0xFF
uint8_t Rcp_interface::bsl_calc_checksum(const uint8_t *data, size_t length)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
    // the CMD-Byte has to be added by the caller because its part of data[] 
    // (data[0] = cmd, data[1..] = payload)
}