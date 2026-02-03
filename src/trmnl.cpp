#include <Arduino.h>
#include <ArduinoJson.h> // Required: install via PlatformIO lib_deps
#include <esp_http_client.h>
#include <esp_heap_caps.h>
#include <WiFi.h>
#include <lodepng.h>
#include "trmnl.hpp"
#include "wifi.hpp"
#include "battery.hpp"
#include <epd_driver.h>

#define API_URL "http://192.168.1.220:4567/api/display"
#define ACCESS_TOKEN "LcFaWmFWLCBLjla0jrq4"
#define MAX_HTTP_BUFFER (EPD_HEIGHT * EPD_WIDTH)
#define COLORS "#000000,#0A0A0A,#151515,#222222,#303030,#404040,#515151,#646464,#7A7A7A,#929292,#ACACAC,#C3C3C3,#D6D6D6,#E5E5E5,#F2F2F2,#FFFFFF"

typedef struct
{
    uint8_t *data;
    size_t size;
    size_t capacity;
} http_buf_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_buf_t *buf = (http_buf_t *)evt->user_data;
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        if (buf->size + evt->data_len <= buf->capacity)
        {
            memcpy(buf->data + buf->size, evt->data, evt->data_len);
            buf->size += evt->data_len;
        }
        else
        {
            Serial.printf("HTTP Buffer Overflow! Increase MAX_HTTP_BUFFER. Current size: %zu, New data: %d", buf->size, evt->data_len);
            return ESP_FAIL;
        }
        break;
    case HTTP_EVENT_ERROR:
        Serial.printf("HTTP_EVENT_ERROR");
        break;
    default:
        break;
    }
    return ESP_OK;
}

DisplayConfig get_display_config()
{
    DisplayConfig config = {"", 900, false};

    // Using a smaller buffer for JSON than the image buffer
    http_buf_t json_buf;
    json_buf.capacity = 2048;
    json_buf.size = 0;
    json_buf.data = (uint8_t *)malloc(json_buf.capacity);

    if (!json_buf.data)
        return config;

    esp_http_client_config_t cfg = {
        .url = API_URL,
        .event_handler = http_event_handler,
        .user_data = &json_buf,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "MAC", WiFi.macAddress().c_str());
    esp_http_client_set_header(client, "Access-Token", ACCESS_TOKEN);
    esp_http_client_set_header(client, "Battery-Voltage", String(getBatteryVoltage()).c_str());
    esp_http_client_set_header(client, "RSSI", String(dashboard::GetRssi()).c_str());
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Colors", COLORS);
    esp_http_client_set_header(client, "Height", String(EPD_HEIGHT).c_str());
    esp_http_client_set_header(client, "Width", String(EPD_WIDTH).c_str());
    esp_http_client_set_header(client, "Content-Type", "application/json");

    if (esp_http_client_perform(client) == ESP_OK)
    {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, (char *)json_buf.data);

        if (!error)
        {
            config.image_url = doc["image_url"].as<String>();
            config.refresh_rate = doc["refresh_rate"].as<int>();
            config.success = true;
            Serial.printf("API Success! URL: %s, Refresh: %d\n", config.image_url.c_str(), config.refresh_rate);
        }
        else
        {
            Serial.printf("JSON Parse Failed: %s\n", error.c_str());
        }
    }

    esp_http_client_cleanup(client);
    free(json_buf.data);
    return config;
}

bool fetch_and_convert_image(const char *url, uint8_t *out_buf, size_t out_buf_size)
{
    Serial.printf("Starting fetch from: %s", url);

    // 1. Allocate Buffer
    http_buf_t http_buf;
    http_buf.size = 0;
    http_buf.capacity = MAX_HTTP_BUFFER;
    http_buf.data = (uint8_t *)heap_caps_malloc(MAX_HTTP_BUFFER, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!http_buf.data)
    {
        Serial.printf("Failed to allocate %d bytes in PSRAM for HTTP buffer", MAX_HTTP_BUFFER);
        return false;
    }

    // 2. HTTP Fetch
    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = &http_buf};

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);

    if (err != ESP_OK)
    {
        Serial.printf("HTTP GET failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(http_buf.data);
        return false;
    }

    int status_code = esp_http_client_get_status_code(client);
    size_t content_length = http_buf.size;
    Serial.printf("HTTP Status = %d, content_length = %zu", status_code, content_length);

    // 3. PNG Decode
    uint8_t *decoded_image = NULL;
    unsigned width = 0, height = 0;

    Serial.printf("Decoding PNG...");
    unsigned error = lodepng_decode_memory(&decoded_image, &width, &height, http_buf.data, http_buf.size, LCT_GREY, 8);

    if (error)
    {
        Serial.printf("LodePNG decode error %u: %s", error, lodepng_error_text(error));
        esp_http_client_cleanup(client);
        free(http_buf.data);
        return false;
    }

    Serial.printf("PNG Decoded Successfully: %u x %u", width, height);

    // 4. Pack into 4-bit (Official EPD logic)
    // Account for the "additional nibble per row" for odd widths
    uint32_t stride_width = (width % 2 == 0) ? width : width + 1;
    size_t needed_bytes = (stride_width * height + 1) / 2;

    Serial.printf("Packing 4-bit data. Needed: %zu bytes, Available: %zu bytes", needed_bytes, out_buf_size);

    if (out_buf_size >= needed_bytes)
    {
        memset(out_buf, 0, out_buf_size);
        for (uint32_t y = 0; y < height; y++)
        {
            for (uint32_t x = 0; x < width; x++)
            {
                uint8_t p8 = decoded_image[y * width + x];
                uint8_t p4 = (p8 * 15) / 255; // Scale to 4-bit

                uint32_t val_idx = y * stride_width + x;
                uint32_t byte_idx = val_idx / 2;

                if (val_idx % 2)
                {
                    out_buf[byte_idx] = (out_buf[byte_idx] & 0x0F) | (p4 << 4);
                }
                else
                {
                    out_buf[byte_idx] = (out_buf[byte_idx] & 0xF0) | (p4 & 0x0F);
                }
            }
        }
        Serial.printf("Packing complete.");
    }
    else
    {
        Serial.printf("Output buffer too small! Needs %zu", needed_bytes);
    }

    // 5. Cleanup
    free(decoded_image);
    esp_http_client_cleanup(client);
    free(http_buf.data);

    return (out_buf_size >= needed_bytes);
}