#ifndef WIFICREDS_H_
#define WIFICREDS_H_

#include <cstddef>
#include <cstring>

/** @brief WiFi STA credentials with length validation. */
struct WifiCreds {
    static constexpr std::size_t kMaxSsid = 32;
    static constexpr std::size_t kMaxPass = 64;

    char ssid[kMaxSsid + 1]{};
    char pass[kMaxPass + 1]{};

    /**
     * @brief Copy SSID and password if both are valid; otherwise leave unchanged.
     * @param s SSID, 1..32 chars.
     * @param p Password, empty (open network) or 8..64 chars.
     * @return true if assigned.
     */
    bool assign(const char* s, const char* p)
    {
        if (s == nullptr || p == nullptr) return false;
        const std::size_t ls = std::strlen(s);
        const std::size_t lp = std::strlen(p);
        if (ls == 0 || ls > kMaxSsid) return false;
        if (lp > kMaxPass || (lp > 0 && lp < 8)) return false;   // WPA2: 8..63 chars or 64 hex
        std::memcpy(ssid, s, ls + 1);
        std::memcpy(pass, p, lp + 1);
        return true;
    }

    /** @brief true if an SSID is set. */
    bool valid() const { return ssid[0] != '\0'; }
    /** @brief true for an open network (empty password). */
    bool open()  const { return pass[0] == '\0'; }
};

#endif // WIFICREDS_H_
