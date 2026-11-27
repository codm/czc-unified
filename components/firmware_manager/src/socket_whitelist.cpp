#include "nvs.h"
#include "lwip/sockets.h"
#include <cstring>

#include "esp_check.h"
#include "esp_log.h"
#include "socket_whitelist.h"

static const char* TAG = "SocketWhitelist";

namespace {
constexpr const char* NVS_NAMESPACE = "socket_wl";
constexpr const char* KEY_IPS = "ips";
constexpr const char* KEY_ACTIVE = "ip_wl_active";

struct StoredWhitelist {
    uint32_t count;
    char addresses[SocketWhitelist::MAX_ENTRIES]
                  [SocketWhitelist::IPV4_ADDR_SIZE];
};
}

bool SocketWhitelist::readIpWhitelistActive()
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    uint8_t val{0};
    nvs_get_u8(handle, KEY_ACTIVE, &val);
    nvs_close(handle);
    return static_cast<bool>(val);
}

esp_err_t SocketWhitelist::writeIpWhitelistActive(bool value)
{
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle),
                        TAG, "NVS open failed");
    esp_err_t ret{nvs_set_u8(handle, KEY_ACTIVE, static_cast<uint8_t>(value))};
    if (ret == ESP_OK) ret = nvs_commit(handle);
    nvs_close(handle);
    return ret;
}

esp_err_t SocketWhitelist::add(const char* ipAddress)
{
    if (ipAddress == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    in_addr parsedAddress{};
    if (inet_pton(AF_INET, ipAddress, &parsedAddress) != 1) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    StoredWhitelist data{};
    size_t size = sizeof(data);
    err = nvs_get_blob(handle, KEY_IPS, &data, &size);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        data = {};  // First execution -> nvs not yet initialized
    } else if (err != ESP_OK || size != sizeof(data)) {
        nvs_close(handle);
        return err == ESP_OK ? ESP_ERR_INVALID_SIZE : err;
    }

    for (size_t i = 0; i < data.count; ++i) {
        if (strcmp(data.addresses[i], ipAddress) == 0) {
            nvs_close(handle);
            return ESP_OK;  // Already there
        }
    }

    if (data.count >= MAX_ENTRIES) {
        nvs_close(handle);
        return ESP_ERR_NO_MEM;
    }

    strlcpy(data.addresses[data.count], ipAddress, IPV4_ADDR_SIZE);
    ++data.count;

    err = nvs_set_blob(handle, KEY_IPS, &data, sizeof(data));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
    
}

esp_err_t SocketWhitelist::remove(const char* ipAddress)
{
    if (ipAddress == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    in_addr parsedAddress{};
    if (inet_pton(AF_INET, ipAddress, &parsedAddress) != 1) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    StoredWhitelist data{};
    size_t size = sizeof(data);
    err = nvs_get_blob(handle, KEY_IPS, &data, &size);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return err;
    } else if (err != ESP_OK || size != sizeof(data)) {
        nvs_close(handle);
        return err == ESP_OK ? ESP_ERR_INVALID_SIZE : err;
    }

    if (data.count > MAX_ENTRIES) {
        nvs_close(handle);
        return ESP_ERR_INVALID_SIZE;
    }
    for (size_t i = 0; i < data.count; ++i) {
        if (strcmp(data.addresses[i], ipAddress) == 0) {
            // Element found -> shift all right elements one to the left
            for (size_t j = i; j < data.count - 1; ++j) {
                strlcpy(data.addresses[j], data.addresses[j + 1], IPV4_ADDR_SIZE);
            }
            --data.count;
            data.addresses[data.count][0] = '\0';

            err = nvs_set_blob(handle, KEY_IPS, &data, sizeof(data));
            if (err == ESP_OK) {
                err = nvs_commit(handle);
            }
            nvs_close(handle);
            return err;
        }
    }

    
    nvs_close(handle);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t SocketWhitelist::getAll(char (*ipAddresses)[IPV4_ADDR_SIZE], size_t capacity, size_t* count)
{
    if (count == nullptr || (capacity > 0 && ipAddresses == nullptr)) {
        return ESP_ERR_INVALID_ARG;
    }

    *count = 0;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    StoredWhitelist data{};
    size_t size = sizeof(data);
    err = nvs_get_blob(handle, KEY_IPS, &data, &size);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    if (size != sizeof(data) || data.count > MAX_ENTRIES) {
        return ESP_ERR_INVALID_SIZE;
    }

    *count = data.count;
    if (capacity < data.count) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (data.count > 0) {
        memcpy(ipAddresses, data.addresses,
               data.count * sizeof(data.addresses[0]));
    }

    return ESP_OK;
}

esp_err_t SocketWhitelist::check(const char* ipAddress, bool* allowed)
{
    if (ipAddress == nullptr || allowed == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!readIpWhitelistActive()) {
        *allowed = true;
        return ESP_OK;
    }

    *allowed = false;

    in_addr clientAddress{};
    if (inet_pton(AF_INET, ipAddress, &clientAddress) != 1) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        *allowed = true;
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    StoredWhitelist data{};
    size_t size = sizeof(data);
    err = nvs_get_blob(handle, KEY_IPS, &data, &size);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        *allowed = true; // Empty whitelist -> everyone is allowed
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    if (size != sizeof(data) || data.count > MAX_ENTRIES) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (data.count == 0) {
        *allowed = true; // Empty whitelist -> everyone is allowed
        return ESP_OK;
    }

    for (size_t i = 0; i < data.count; ++i) {
        in_addr storedAddress{};
        if (inet_pton(AF_INET, data.addresses[i], &storedAddress) != 1) {
            nvs_close(handle);
            return ESP_FAIL; // Somehow saved address in nvs is invalid
        }
        if (storedAddress.s_addr == clientAddress.s_addr) {
            *allowed = true;
            nvs_close(handle);
            return ESP_OK;
        }
    }


    

    return ESP_OK;
}