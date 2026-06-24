#ifndef CZC_RCP_INTERFACE_H_
#define CZC_RCP_INTERFACE_H_

#include "esp_err.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

inline constexpr EventBits_t RCP_UPDATE_SUCCESS_BIT = BIT0;
inline constexpr EventBits_t RCP_UPDATE_FAIL_BIT    = BIT1;

class Rcp_interface
{
private:
    static const char* TAG;

    uart_port_t rcp_uart;
    bool bsl_mode = false;
    size_t downloaded_size = 0;
    EventGroupHandle_t update_event_group = nullptr;

    static void rcp_update_task(void *pvParameters);
    esp_err_t rcp_update_run(const char* url);

    esp_err_t download_image(const char* url);
    esp_err_t flash_image();
    esp_err_t bsl_uart_acquire(void);
    void bsl_uart_release(void);

    esp_err_t bsl_enter_bootloader(void);
    esp_err_t bsl_erase_flash(void);
    esp_err_t bsl_begin_flash(uint32_t address, uint32_t size);
    esp_err_t bsl_process_flash(const uint8_t *data, int length);
    esp_err_t bsl_reset_target(void);
    esp_err_t bsl_send_packet(const uint8_t *cmd_and_data, size_t length);
    esp_err_t bsl_uart_sync();
    bool bsl_wait_ack(uint32_t timeout_ms);
    esp_err_t bsl_read_response(uint8_t *out_buf, size_t buf_size, size_t *out_len);
    esp_err_t bsl_check_last_cmd(void);
    uint8_t bsl_calc_checksum(const uint8_t *data, size_t length);

public:
    Rcp_interface();
    ~Rcp_interface();
    esp_err_t rcp_update_init(uart_port_t _rcp_uart);
    esp_err_t rcp_update_start(const char* url);
    EventGroupHandle_t get_update_event_group() const { return update_event_group; }
};

struct RcpUpdateParams {
    Rcp_interface *self;
    char url[256];
};

#endif // CZC_RCP_INTERFACE_H_
