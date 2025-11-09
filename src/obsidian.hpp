#ifndef OBSIDIAN_HPP
#define OBSIDIAN_HPP

#include <vector>
#include <string>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "cred.hpp"

namespace dashboard {

class Obsidian {
private:
    static constexpr const char* BASE_URL = "http://192.168.1.202/couchdb/";
    static constexpr const char* VAULT = "obsidian";
    HTTPClient http;

    String buildUrl(const String& filename) const {
        return String(BASE_URL) + VAULT + "/" + filename;
    }

    void setupAuth() {
        http.setAuthorization(COUCHDB_CREDS.Username.c_str(), COUCHDB_CREDS.Password.c_str());
    }

public:
    Obsidian() = default;
    
    std::vector<std::string> queryDocument(const String& filename) {
        std::vector<std::string> responses;

        // Query the initial document to get the list of children
        http.begin(buildUrl(filename));
        setupAuth();
        int httpCode = http.GET();

        if (httpCode != HTTP_CODE_OK) {
            // ensure resources freed and return empty list
            http.end();
            return responses;
        }

        String payload = http.getString();
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            http.end();
            return responses;
        }

        // Get children array
        JsonArray children = doc["children"].as<JsonArray>();

        // done with the initial request
        http.end();

        // Query each child document and return only the "data" section
        for (JsonVariant childVar : children) {
            const char* childName = childVar.as<const char*>();
            if (!childName) {
                // keep alignment: push empty string on malformed entry
                responses.emplace_back("");
                continue;
            }

            http.begin(buildUrl(String(childName)));
            setupAuth();
            int childHttpCode = http.GET();

            if (childHttpCode == HTTP_CODE_OK) {
                String childPayload = http.getString();

                // parse child payload and extract "data"
                DynamicJsonDocument childDoc(2048);
                DeserializationError childErr = deserializeJson(childDoc, childPayload);
                if (!childErr) {
                    JsonVariant dataVar = childDoc["data"];
                    if (!dataVar.isNull()) {
                        String out;
                        serializeJson(dataVar, out);
                        responses.emplace_back(std::string(out.c_str()));
                    } else {
                        // no data field -> push empty string
                        responses.emplace_back("");
                    }
                } else {
                    // parse failed -> push empty string
                    responses.emplace_back("");
                }
            } else {
                // request failed -> push empty string
                responses.emplace_back("");
            }

            // free resources for this child request
            http.end();
        }

        return responses;
    }
};

} // namespace dashboard

#endif // OBSIDIAN_HPP