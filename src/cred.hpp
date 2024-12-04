/**
 * @brief Credentials for wifi.
 * @author Aya Kraise
 */

#pragma once

#include <string>

namespace dashboard {
    typedef struct {
        const std::string WifiName;
        const std::string Password;
    } WifiCreds;

    extern const WifiCreds WIFI_CREDS;
}
