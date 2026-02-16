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
#include "config.hpp"

#define MAX_HTTP_BUFFER (EPD_HEIGHT * EPD_WIDTH)

/*
def generate_epd_lut(gamma=0.7, bits=4):
    max_input = 255
    max_output = (2**bits) - 1

    lut = []
    for i in range(256):
        # Calculate gamma: (input/max)^gamma * max_output
        val = math.pow(i / max_input, gamma) * max_output

        # Round to nearest integer and clamp
        rounded_val = round(val)
        lut.append(rounded_val)

    return lut
*/
const uint8_t k_gamma_lut[256] = {
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15};

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
            Serial.printf("HTTP Buffer Overflow! Increase MAX_HTTP_BUFFER. Current size: %zu, New data: %d\n", buf->size, evt->data_len);
            return ESP_FAIL;
        }
        break;
    case HTTP_EVENT_ERROR:
        Serial.printf("HTTP_EVENT_ERROR\n");
        break;
    default:
        break;
    }
    return ESP_OK;
}

DisplayConfig get_display_config()
{
    DisplayConfig config = {"", DEVICE_REFRESHRATE, false};

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
    esp_http_client_set_header(client, "Host", HOST_HEADER);
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
    Serial.printf("Starting fetch from: %s\n", url);

    // 1. Allocate Buffer
    http_buf_t http_buf;
    http_buf.size = 0;
    http_buf.capacity = MAX_HTTP_BUFFER;
    http_buf.data = (uint8_t *)heap_caps_malloc(MAX_HTTP_BUFFER, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!http_buf.data)
    {
        Serial.printf("Failed to allocate %d bytes in PSRAM for HTTP buffer\n", MAX_HTTP_BUFFER);
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
        Serial.printf("HTTP GET failed: %s\n", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(http_buf.data);
        return false;
    }

    int status_code = esp_http_client_get_status_code(client);
    size_t content_length = http_buf.size;
    Serial.printf("HTTP Status = %d, content_length = %zu\n", status_code, content_length);

    // 3. PNG Decode
    uint8_t *decoded_image = NULL;
    unsigned width = 0, height = 0;

    Serial.printf("Decoding PNG...\n");
    unsigned error = lodepng_decode_memory(&decoded_image, &width, &height, http_buf.data, http_buf.size, LCT_GREY, 8);

    if (error)
    {
        Serial.printf("LodePNG decode error %u: %s\n", error, lodepng_error_text(error));
        esp_http_client_cleanup(client);
        free(http_buf.data);
        return false;
    }

    free(http_buf.data);
    esp_http_client_cleanup(client);
    WiFi.mode(WIFI_OFF); // Turn off WiFi immediately after fetch to save power

    Serial.printf("PNG Decoded Successfully: %u x %u\n", width, height);

    // 4. Pack into 4-bit (Official EPD logic)
    size_t total_pixels = (size_t)width * height;
    size_t needed_bytes = total_pixels / 2; // No +1 needed since it's always even

    Serial.printf("Packing 4-bit data, Need: %zu bytes\n", needed_bytes);

    if (out_buf_size >= needed_bytes)
    {
        for (size_t i = 0; i < needed_bytes; i++)
        {
            // Grab two 8-bit pixels at once
            uint8_t p8_1 = decoded_image[i * 2];
            uint8_t p8_2 = decoded_image[i * 2 + 1];

            // Convert both via LUT and pack into one byte
            // Low nibble = Pixel 1, High nibble = Pixel 2
            out_buf[i] = (k_gamma_lut[p8_2] << 4) | (k_gamma_lut[p8_1] & 0x0F);
        }
        Serial.printf("Packing complete.\n");
    }
    else
    {
        Serial.printf("Output buffer too small!\n");
    }

    // 5. Cleanup
    free(decoded_image);

    return (out_buf_size >= needed_bytes);
}