#ifndef CZC_PROXY_TRANSPORT_H_
#define CZC_PROXY_TRANSPORT_H_

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_log_write.h"

/**
 * @brief Bidirectional byte-stream interface between the Zigbee proxy and its host.
 *
 *        The proxy pumps bytes in both directions:
 *          RCP UART  ←→  IProxyTransport  ←→  host (USB serial or TCP socket)
 *
 *        Two implementations exist:
 *          - UartTransport  : USB CDC / physical serial port
 *          - TcpTransport   : TCP socket with IP whitelist
 */
class IProxyTransport {
public:
    virtual esp_err_t open()  = 0;
    virtual esp_err_t close() = 0;

    /**
     * @brief Write bytes to the host side.
     *
     * @param[in] buf   Data buffer
     * @param[in] len   Number of bytes to write
     *
     * @return Number of bytes written, or negative on error
     */
    virtual int write(const uint8_t* buf, size_t len) = 0;

    /**
     * @brief Read bytes from the host side (blocking until data available or timeout).
     *
     * @param[out] buf  Buffer to read into
     * @param[in]  len  Maximum bytes to read
     *
     * @return Number of bytes read, 0 on timeout, negative on error
     */
    virtual int read(uint8_t* buf, size_t len) = 0;

    virtual ~IProxyTransport() = default;
};

// ---------------------------------------------------------------------------

/**
 * @brief IProxyTransport over USB/UART serial.
 */
class UartTransport : public IProxyTransport {
public:
    esp_err_t open() override;
    esp_err_t close() override;
    int write(const uint8_t* buf, size_t len) override;
    int read(uint8_t* buf, size_t len) override;
private:
    vprintf_like_t savedVprintf{nullptr};
};

// ---------------------------------------------------------------------------

/**
 * @brief IProxyTransport over TCP.
 *
 *        open() binds and listens on PROXY_TCP_PORT.
 *        The first read() call blocks until a client connects (accept).
 *        When the client disconnects, the next read() accepts a new one.
 */
class TcpTransport : public IProxyTransport {
public:
    TcpTransport(uint16_t port);
    ~TcpTransport();

    esp_err_t open() override;
    esp_err_t close() override;
    int write(const uint8_t* buf, size_t len) override;
    int read(uint8_t* buf, size_t len) override;

private:
    uint16_t port;
    int      serverFd{-1};
    int      clientFd{-1};
};

#endif // CZC_PROXY_TRANSPORT_H_
