#ifndef OBSIDIAN_HPP
#define OBSIDIAN_HPP

#include <vector>
#include <string>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "cred.hpp"

namespace dashboard
{

    class Obsidian
    {
    private:
        static constexpr const char *BASE_URL = "http://192.168.1.202/couchdb/";
        static constexpr const char *VAULT = "obsidian";
        HTTPClient http;

        String buildUrl(const String &filename) const
        {
            return String(BASE_URL) + VAULT + "/" + filename;
        }

        void setupAuth()
        {
            http.setAuthorization(COUCHDB_CREDS.Username.c_str(), COUCHDB_CREDS.Password.c_str());
        }

    public:
        Obsidian() = default;
        std::vector<const char *> queryDocument(const String &filename)
        {
            std::vector<const char *> responses;

            // Query the initial document to get the list of children
            http.begin(buildUrl(filename));
            setupAuth();
            int httpCode = http.GET();

            if (httpCode != HTTP_CODE_OK)
            {
                http.end();
                return responses;
            }

            String payload = http.getString();
            JsonDocument doc; // JsonDocument replaces deprecated StaticJsonDocument
            DeserializationError error = deserializeJson(doc, payload);
            http.end();

            // Check that "children" exists and is an array
            if (error || !doc["children"].is<JsonArray>())
            {
                return responses;
            }

            JsonArray children = doc["children"].as<JsonArray>();

            // Query each child document and extract "data"
            for (JsonVariant childVar : children)
            {
                const char *childName = childVar.as<const char *>();
                if (!childName)
                {
                    continue; // skip malformed entries
                }

                http.begin(buildUrl(childName));
                setupAuth();
                int childHttpCode = http.GET();

                if (childHttpCode == HTTP_CODE_OK)
                {
                    String childPayload = http.getString();
                    JsonDocument childDoc; // use JsonDocument instead of deprecated DynamicJsonDocument
                    DeserializationError childErr = deserializeJson(childDoc, childPayload);

                    // Use modern API to check if "data" exists and is a string
                    if (!childErr && childDoc["data"].is<const char *>())
                    {
                        const char *dataValue = childDoc["data"];
                        // Copy the string to avoid dangling pointer after childDoc goes out of scope
                        char *copy = new char[strlen(dataValue) + 1];
                        strcpy(copy, dataValue);
                        responses.push_back(copy);
                    }
                }

                http.end();
            }

            return responses;
        }
    };

} // namespace dashboard

#endif // OBSIDIAN_HPP