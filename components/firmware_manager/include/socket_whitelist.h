#ifndef CZC_SOCKET_WHITELIST_H_
#define CZC_SOCKET_WHITELIST_H_

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

namespace SocketWhitelist {
    inline constexpr size_t MAX_ENTRIES = 16;
    inline constexpr size_t IPV4_ADDR_SIZE = 16;

    esp_err_t add(const char* ipAddress);
    esp_err_t remove(const char* ipAddress);
    esp_err_t getAll(char (*ipAddresses)[IPV4_ADDR_SIZE], size_t capacity, size_t* count);
    esp_err_t check(uint32_t ipAddress, bool* allowed);
}

#endif // CZC_SOCKET_WHITELIST_H_
