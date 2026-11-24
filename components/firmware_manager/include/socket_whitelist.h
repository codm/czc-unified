#ifndef CZC_SOCKET_WHITELIST_H_
#define CZC_SOCKET_WHITELIST_H_

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Manages the persistent IPv4 whitelist for TCP connections.
 *
 * Entries are stored in NVS. If the whitelist is empty, all IPv4 addresses
 * are allowed.
 */
namespace SocketWhitelist {
    /**
     * @brief Maximum number of IPv4 addresses in the whitelist.
     * @note Keep the corresponding limit in the esp_br_web.c handler in sync.
     */
    inline constexpr size_t MAX_ENTRIES = 16;

    /** @brief Maximum buffer size for an IPv4 address, including the null terminator. */
    inline constexpr size_t IPV4_ADDR_SIZE = 16;

    /**
     * @brief Adds an IPv4 address to the whitelist.
     *
     * Addresses that are already present are not added again.
     *
     * @param ipAddress Null-terminated IPv4 address string, e.g. "192.168.1.10".
     * @return ESP_OK on success; otherwise an error code, such as
     *         ESP_ERR_INVALID_ARG for an invalid address or ESP_ERR_NO_MEM
     *         if the whitelist is full.
     */
    esp_err_t add(const char* ipAddress);

    /**
     * @brief Removes an IPv4 address from the whitelist.
     *
     * @param ipAddress Null-terminated IPv4 address string.
     * @return ESP_OK on success; otherwise an error code, including
     *         ESP_ERR_INVALID_ARG for an invalid address or
     *         ESP_ERR_NOT_FOUND if the address is not present.
     */
    esp_err_t remove(const char* ipAddress);

    /**
     * @brief Reads the IPv4 addresses from the whitelist.
     *
     * @param ipAddresses Destination array with entries of size IPV4_ADDR_SIZE.
     *                    May be null when capacity is 0.
     * @param capacity Number of entries available in ipAddresses.
     * @param count Output parameter for the number of stored addresses.
     * @return ESP_OK on success; ESP_ERR_INVALID_ARG for invalid pointers,
     *         ESP_ERR_INVALID_SIZE if the array is too small, or another
     *         error code if an NVS operation fails.
     */
    esp_err_t getAll(char (*ipAddresses)[IPV4_ADDR_SIZE], size_t capacity, size_t* count);

    /**
     * @brief Checks whether an IPv4 address is allowed by the whitelist.
     *
     * If the whitelist is empty or has not yet been created, all addresses
     * are allowed.
     *
     * @param ipAddress Null-terminated IPv4 address string.
     * @param allowed Output parameter set to true if the address is allowed.
     * @return ESP_OK if the check succeeds; ESP_ERR_INVALID_ARG for invalid
     *         arguments or an invalid IPv4 address, or another error code if
     *         an NVS operation fails.
     */
    esp_err_t check(const char* ipAddress, bool* allowed);
}

#endif // CZC_SOCKET_WHITELIST_H_
