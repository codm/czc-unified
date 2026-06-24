#ifndef CZC_PROXY_TRANSPORT_H_
#define CZC_PROXY_TRANSPORT_H_

#include <stdint.h>
#include <stddef.h>

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
 * @brief IProxyTransport over USB/UART serial — stub, not yet implemented.
 */
class UartTransport : public IProxyTransport {
public:
    int write(const uint8_t* buf, size_t len) override;
    int read(uint8_t* buf, size_t len) override;
};

/**
 * @brief IProxyTransport over TCP with IP whitelist — stub, not yet implemented.
 */
class TcpTransport : public IProxyTransport {
public:
    int write(const uint8_t* buf, size_t len) override;
    int read(uint8_t* buf, size_t len) override;
};

#endif // CZC_PROXY_TRANSPORT_H_
