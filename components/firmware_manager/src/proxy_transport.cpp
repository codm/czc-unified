#include "proxy_transport.h"

#include "board_config.h"
#include "status_light_event.h"
#include "proxy_transport_event.h"
#include "socket_whitelist.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_check.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

static const char* TAG = "ProxyTransport";

ESP_EVENT_DEFINE_BASE(CLIENT_CONNECTED_EVENT);

// ---------------------------------------------------------------------------
// UartTransport
// ---------------------------------------------------------------------------

esp_err_t UartTransport::open()
{
    if (uart_is_driver_installed(Board::HOST_UART)) {
        ESP_LOGW(TAG, "UART%d driver already installed at init: deleting stale instance!", Board::HOST_UART);
        uart_driver_delete(Board::HOST_UART);
    }

    savedVprintf = esp_log_set_vprintf([](const char*, va_list) -> int { return 0; });

    uart_config_t cfg = {
        .baud_rate  = Board::COORD_BAUD,
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

TcpTransport::TcpTransport(uint16_t port) : port{port}
{
    esp_event_handler_register(IP_WHITELIST_EVENT, ESP_EVENT_ANY_ID, &TcpTransport::ip_whitelist_event_handler, this);
}

TcpTransport::~TcpTransport()
{
    close();
}

esp_err_t TcpTransport::open()
{
    // socket connection didn't work most of the time because of errno 23 (serverFd was still used)
    if (serverFd >= 0) {
        ::close(serverFd);
        serverFd = -1;
    }

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
    StatusLedMode::set(LedState::ZIGBEE_HOST_WAIT);
    return ESP_OK;
}

esp_err_t TcpTransport::close()
{
    if (clientFd >= 0)
    {
        ::close(clientFd);
        clientFd = -1;
        StatusLedMode::set(LedState::ZIGBEE_HOST_WAIT);
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

        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);

        clientFd = accept(serverFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if (clientFd < 0) {
            ESP_LOGE(TAG, "accept() failed: errno %d", errno);
            return -1;
        }

        inet_ntop(AF_INET, &(clientAddr.sin_addr), currentClientIp, INET_ADDRSTRLEN);

        bool ip_allowed = false;
        esp_err_t whitelistErr = SocketWhitelist::check(currentClientIp, &ip_allowed);

        if (whitelistErr != ESP_OK || !ip_allowed) {
            if (whitelistErr != ESP_OK) {
                ESP_LOGE(TAG, "Failed to check TCP client IP %s against whitelist: %s",
                         currentClientIp, esp_err_to_name(whitelistErr));
            }
            ESP_LOGI(TAG, "TCP client tried to connect from IP: %s not on whitelist", currentClientIp);
            ::close(clientFd);
            clientFd = -1;
            return -1;
        }

        ESP_LOGI(TAG, "TCP client connected from IP: %s", currentClientIp);
        StatusLedMode::set(LedState::ZIGBEE_NET);
        esp_event_post(CLIENT_CONNECTED_EVENT, CLIENT_CONNECTED, nullptr, 0, portMAX_DELAY);
    }

    int n = recv(clientFd, buf, len, 0);
    if (n <= 0) {
        ESP_LOGI(TAG, "TCP client disconnected");
        ::close(clientFd);
        clientFd = -1;
        StatusLedMode::set(LedState::ZIGBEE_HOST_WAIT);
        esp_event_post(CLIENT_CONNECTED_EVENT, CLIENT_DISCONNECTED, nullptr, 0, portMAX_DELAY);
    }
    return n;
}

void TcpTransport::ip_whitelist_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    auto* transport = static_cast<TcpTransport*>(handler_args);
    if (transport->clientFd >= 0) {
        bool ip_allowed = false;
        esp_err_t whitelistErr = SocketWhitelist::check(transport->currentClientIp, &ip_allowed);
        if (whitelistErr != ESP_OK || ip_allowed) {
            if (whitelistErr != ESP_OK) {
                ESP_LOGE(TAG, "Failed to check TCP client IP %s against whitelist: %s",
                         transport->currentClientIp, esp_err_to_name(whitelistErr));
            }
            return;
        }
        ::shutdown(transport->clientFd, SHUT_RDWR);
    }
}