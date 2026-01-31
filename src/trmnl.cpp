#include <Arduino.h>
#include <esp_http_client.h>
#include <esp_heap_caps.h>
#include <lodepng.h>

#define IMAGE_URL "http://192.168.1.220:4567/storage/images/generated/8466ad6c-e505-40f9-9c89-4b4750d310f6.png"
#define MAX_HTTP_BUFFER (860 * 540)

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

bool fetch_and_convert_image(uint8_t *out_buf, size_t out_buf_size)
{
    Serial.printf("Starting fetch from: %s", IMAGE_URL);

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
        .url = IMAGE_URL,
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
                uint8_t p4 = p8 >> 4; // Scale to 4-bit

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