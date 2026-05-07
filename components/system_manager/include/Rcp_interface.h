#include "esp_log.h"
#include "esp_check.h"
#include "driver/uart.h"
#include "esp_http_client.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "driver/gpio.h"

#define HTTP_READ_BUFFER_SIZE 1024
#define PROGRESS_STEP_PERCENT 2.0f
#define BEGIN_ZB_ADDR 0x00000000
#define BSL_TRANSFER_SIZE 252  

#define RST_PIN       GPIO_NUM_16
#define BSL_PIN       GPIO_NUM_32
#define BSL_UART_TX   GPIO_NUM_4
#define BSL_UART_RX   GPIO_NUM_36
#define BSL_UART_BAUD 115200
// --- BSL CMD
#define BSL_CMD_PING 0x20
#define BSL_CMD_DOWNLOAD 0x21   
#define BSL_CMD_GET_STATUS 0x23
#define BSL_CMD_SEND_DATA 0x24  
#define BSL_CMD_RESET 0x25  
#define BSL_CMD_SECTOR_ERASE 0x26 
#define BSL_CMD_CRC32 0x27   
#define BSL_CMD_GET_CHIP_ID 0x28
#define BSL_CMD_MEMORY_READ 0x2A
#define BSL_CMD_BANK_ERASE 0x2C 
#define BSL_CMD_SET_CCFG 0x2D

#define BSL_ACK 0xCC
#define BSL_NACK 0x33

class Rcp_interface
{
private:
    static const char* TAG;

    uart_port_t rcp_uart;
    bool bsl_mode = false;
    size_t downloaded_size = 0;

    static void rcp_update_task(void *pvParameters);
    esp_err_t rcp_update_run(const char* url);

    esp_err_t download_image(const char* url);
    esp_err_t flash_image();
    esp_err_t bsl_uart_acquire(void);
    void      bsl_uart_release(void);

    esp_err_t bsl_enter_bootloader(void);
    esp_err_t bsl_erase_flash(void);
    esp_err_t bsl_begin_flash(uint32_t address, uint32_t size);
    esp_err_t bsl_process_flash(const uint8_t *data, int length);
    // esp_err_t bsl_verify_crc(uint32_t address, uint32_t size, uint32_t expected_crc);
    esp_err_t bsl_reset_target(void);
    esp_err_t bsl_send_packet(const uint8_t *cmd_and_data, size_t length);
    bool bsl_wait_ack(uint32_t timeout_ms);
    esp_err_t bsl_read_response(uint8_t *out_buf, size_t buf_size, size_t *out_len);
    esp_err_t bsl_check_last_cmd(void);
    uint8_t bsl_calc_checksum(const uint8_t *data, size_t length);

public:
    Rcp_interface();
    ~Rcp_interface();
    esp_err_t rcp_update_init(uart_port_t _rcp_uart);
    esp_err_t rcp_update_start(const char* url);
};

struct RcpUpdateParams {
    Rcp_interface *self;
    char url[256];
};