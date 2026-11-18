
#include "NvsConfigStore.h"

#include "esp_log.h"

NvsConfigStore::~NvsConfigStore()
{
    std::lock_guard lock(mtx_);
    if (open_) nvs_close(h_);
}

esp_err_t NvsConfigStore::init()
{
    std::lock_guard lock(mtx_);
    if (open_) return ESP_OK;
    esp_err_t err = nvs_open(kNs, NVS_READWRITE, &h_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }
    open_ = true;
    return ESP_OK;
}

bool NvsConfigStore::loadCreds(WifiCreds& out) const
{
    std::lock_guard lock(mtx_);
    if (!open_) return false;

    char ssid[WifiCreds::kMaxSsid + 1]{};
    char pass[WifiCreds::kMaxPass + 1]{};
    size_t lenS = sizeof(ssid);
    size_t lenP = sizeof(pass);

    if (nvs_get_str(h_, kSsid, ssid, &lenS) != ESP_OK) return false;   // never stored
    if (nvs_get_str(h_, kPass, pass, &lenP) != ESP_OK) pass[0] = '\0';  // open network

    WifiCreds c;
    if (!c.assign(ssid, pass)) {
        ESP_LOGW(TAG, "stored creds invalid, ignoring");
        return false;
    }
    out = c;
    return true;
}

esp_err_t NvsConfigStore::saveCreds(const WifiCreds& creds)
{
    if (!creds.valid()) return ESP_ERR_INVALID_ARG;

    std::lock_guard lock(mtx_);
    if (!open_) return ESP_ERR_INVALID_STATE;

    // Reset verified first: on power loss mid-write the creds count as
    // unverified, so the way back to the Soft AP stays open.
    esp_err_t err = nvs_set_u8(h_, kVerified, 0);
    if (err == ESP_OK) err = nvs_set_str(h_, kSsid, creds.ssid);
    if (err == ESP_OK) err = nvs_set_str(h_, kPass, creds.pass);
    if (err == ESP_OK) err = nvs_commit(h_);

    if (err != ESP_OK) ESP_LOGE(TAG, "saveCreds failed: %s", esp_err_to_name(err));
    return err;
}

bool NvsConfigStore::isVerified() const
{
    std::lock_guard lock(mtx_);
    if (!open_) return false;
    uint8_t v = 0;
    return nvs_get_u8(h_, kVerified, &v) == ESP_OK && v != 0;
}

esp_err_t NvsConfigStore::setVerified(bool verified)
{
    std::lock_guard lock(mtx_);
    if (!open_) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_set_u8(h_, kVerified, verified ? 1 : 0);
    if (err == ESP_OK) err = nvs_commit(h_);
    if (err != ESP_OK) ESP_LOGE(TAG, "setVerified failed: %s", esp_err_to_name(err));
    return err;
}

esp_err_t NvsConfigStore::clear()
{
    std::lock_guard lock(mtx_);
    if (!open_) return ESP_ERR_INVALID_STATE;
    esp_err_t err = nvs_erase_all(h_);
    if (err == ESP_OK) err = nvs_commit(h_);
    if (err != ESP_OK) ESP_LOGE(TAG, "clear failed: %s", esp_err_to_name(err));
    return err;
}
