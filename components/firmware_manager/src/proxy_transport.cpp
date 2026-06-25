#include "proxy_transport.h"

#include "board_config.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_check.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

static const char* TAG = "ProxyTransport";

// ---------------------------------------------------------------------------
// UartTransport
// ---------------------------------------------------------------------------

esp_err_t UartTransport::open()
{
    savedVprintf = esp_log_set_vprintf([](const char*, va_list) -> int { return 0; });

    uart_config_t cfg = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };

    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_ERROR(uart_param_config(Board::HOST_UART, &cfg), cleanup, TAG, "Failed at [Outside]uart_param_config");

    ESP_GOTO_ON_ERROR(uart_set_pin(Board::HOST_UART,
                        Board::HOST_UART_TX, Board::HOST_UART_RX,
                        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                    cleanup,
                    TAG,
                    "Failed at [Outside]uart_set_pin"
                    );

    ESP_GOTO_ON_ERROR(uart_driver_install(Board::HOST_UART, 2048, 2048, 0, nullptr, 0),
                    cleanup,
                    TAG, 
                    "Failed at [Outside]uart_driver_install"
                    );

    return ret;

cleanup:
    esp_log_set_vprintf(savedVprintf);
    savedVprintf = nullptr;
    return ESP_FAIL;
}

esp_err_t UartTransport::close()
{
    // esp_err_t ret = uart_driver_delete(Board::HOST_UART);

    if (savedVprintf) {
        esp_log_set_vprintf(savedVprintf);
        savedVprintf = nullptr;
    }
    return ESP_OK;
}

int UartTransport::write(const uint8_t* buf, size_t len)
{
    return uart_write_bytes(Board::HOST_UART, buf, len);
}

int UartTransport::read(uint8_t* buf, size_t len)
{
    return uart_read_bytes(Board::HOST_UART, buf, len, pdMS_TO_TICKS(100));
}

// ---------------------------------------------------------------------------
// TcpTransport
// ---------------------------------------------------------------------------

TcpTransport::TcpTransport(uint16_t port) : port{port} {}

TcpTransport::~TcpTransport()
{
    close();
}

esp_err_t TcpTransport::open()
{
    serverFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverFd < 0) {
        ESP_LOGE(TAG, "socket() failed: errno %d", errno);
        return ESP_FAIL;
    }

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind() failed: errno %d", errno);
        ::close(serverFd);
        serverFd = -1;
        return ESP_FAIL;
    }

    if (listen(serverFd, 1) < 0) {
        ESP_LOGE(TAG, "listen() failed: errno %d", errno);
        ::close(serverFd);
        serverFd = -1;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "TCP server listening on port %u", port);
    return ESP_OK;
}

esp_err_t TcpTransport::close()
{
    if (clientFd >= 0) 
    { 
        ::close(clientFd); 
        clientFd = -1; 
    }

    if (serverFd >= 0) 
    { 
        ::close(serverFd); 
        serverFd = -1; 
    }

    return ESP_OK;
}

int TcpTransport::write(const uint8_t* buf, size_t len)
{
    if (clientFd < 0) return -1;
    return send(clientFd, buf, len, 0);
}

int TcpTransport::read(uint8_t* buf, size_t len)
{
    if (clientFd < 0) {
        ESP_LOGI(TAG, "Waiting for TCP client...");
        clientFd = accept(serverFd, nullptr, nullptr);
        if (clientFd < 0) {
            ESP_LOGE(TAG, "accept() failed: errno %d", errno);
            return -1;
        }
        ESP_LOGI(TAG, "TCP client connected");
    }

    int n = recv(clientFd, buf, len, 0);
    if (n <= 0) {
        ESP_LOGI(TAG, "TCP client disconnected");
        ::close(clientFd);
        clientFd = -1;
    }
    return n;
}
