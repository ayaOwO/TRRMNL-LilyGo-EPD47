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

    typedef struct {
        const std::string ClientId;
        const std::string ClientSecret;
        const std::string RefreshToken;
    } SpotifyCreds;

    typedef struct {
        const std::string Username;
        const std::string Password;
    } CouchDBCreds;

    extern const WifiCreds WIFI_CREDS;
    extern const SpotifyCreds SPOTIFY_CREDS;
    extern const CouchDBCreds COUCHDB_CREDS;
}
