#include "nvs.h"
#include "lwip/sockets.h"
#include <cstring>

#include "esp_log.h"
#include "socket_whitelist.h"

namespace {
constexpr const char* NVS_NAMESPACE = "socket_wl";
constexpr const char* NVS_KEY_IPS = "ips";

struct StoredWhitelist {
    uint32_t count;
    char addresses[SocketWhitelist::MAX_ENTRIES]
                  [SocketWhitelist::IPV4_ADDR_SIZE];
};
}




esp_err_t SocketWhitelist::add(const char* ipAddress)
{
    if (ipAddress == nullptr) {
        ESP_LOGE("WHITELIST", "add() failed: ipAddress is nullptr!");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI("WHITELIST", "Versuche IP-Adresse hinzuzufuegen: '%s'", ipAddress);

    in_addr parsedAddress{};
    if (inet_pton(AF_INET, ipAddress, &parsedAddress) != 1) {
        ESP_LOGE("WHITELIST", "add() failed: inet_pton konnte '%s' nicht als gueltige IPv4 parsen!", ipAddress);
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    StoredWhitelist data{};
    size_t size = sizeof(data);
    err = nvs_get_blob(handle, NVS_KEY_IPS, &data, &size);

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

    err = nvs_set_blob(handle, NVS_KEY_IPS, &data, sizeof(data));
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
    err = nvs_get_blob(handle, NVS_KEY_IPS, &data, &size);

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

            err = nvs_set_blob(handle, NVS_KEY_IPS, &data, sizeof(data));
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
    err = nvs_get_blob(handle, NVS_KEY_IPS, &data, &size);
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

esp_err_t SocketWhitelist::check(uint32_t ipAddress, bool* allowed)
{
    if (allowed == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    *allowed = false;

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
    err = nvs_get_blob(handle, NVS_KEY_IPS, &data, &size);
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
        if (storedAddress.s_addr == ipAddress) {
            *allowed = true;
            nvs_close(handle);
            return ESP_OK;
        }
    }


    

    return ESP_OK;
}