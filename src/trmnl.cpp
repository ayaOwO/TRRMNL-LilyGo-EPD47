#include <esp_http_client.h>
#include <esp_heap_caps.h> // Required for PSRAM allocation
#include <lodepng.h>

#define IMAGE_URL "http://192.168.1.220:4567/storage/images/generated/2f8a6d8b-e6ea-44ea-8e68-f7bcf62c5499.png"
#define MAX_HTTP_BUFFER (860 * 540) // 512KB - adjust based on your expected PNG size

typedef struct {
    uint8_t* data;
    size_t   size;
    size_t   capacity;
} http_buf_t;

static esp_err_t http_event_handler(esp_http_client_event_t* evt)
{
    http_buf_t* buf = (http_buf_t*)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // Check if we have enough pre-allocated space
            if (buf->size + evt->data_len <= buf->capacity) {
                memcpy(buf->data + buf->size, evt->data, evt->data_len);
                buf->size += evt->data_len;
            } else {
                return ESP_FAIL; // Buffer overflow
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

bool fetch_and_convert_image(uint8_t* out_buf, size_t out_buf_size)
{
    // Pre-allocate the HTTP buffer in PSRAM to prevent fragmentation
    http_buf_t http_buf;
    http_buf.size = 0;
    http_buf.capacity = MAX_HTTP_BUFFER;
    http_buf.data = (uint8_t*)heap_caps_malloc(MAX_HTTP_BUFFER, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!http_buf.data) {
        return false;
    }

    esp_http_client_config_t cfg = {
        .url = IMAGE_URL,
        .event_handler = http_event_handler,
        .user_data = &http_buf,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        free(http_buf.data);
        return false;
    }

    if (esp_http_client_perform(client) != ESP_OK) {
        esp_http_client_cleanup(client);
        free(http_buf.data);
        return false;
    }

    // ---- PNG DECODE (grayscale) ----
    uint8_t* decoded_image = NULL;
    unsigned width = 0, height = 0;

    // Use LodePNG to decode from the PSRAM buffer
    unsigned error = lodepng_decode_memory(
        &decoded_image, 
        &width, 
        &height, 
        http_buf.data, 
        http_buf.size, 
        LCT_GREY, 
        8
    );

    if (error) {
        esp_http_client_cleanup(client);
        free(http_buf.data);
        return false;
    }

    // ---- PACK INTO 4-BIT GRAYSCALE (EPD format) ----
    size_t pixel_count = width * height;
    size_t needed_bytes = (pixel_count + 1) / 2;

    if (out_buf_size >= needed_bytes) {
        size_t out_i = 0;
        for (size_t i = 0; i < pixel_count; i += 2) {
            uint8_t p0 = decoded_image[i] >> 4;
            uint8_t p1 = (i + 1 < pixel_count) ? (decoded_image[i + 1] >> 4) : 0;
            out_buf[out_i++] = (p0 << 4) | p1;
        }
    }

    // ---- CLEANUP ----
    free(decoded_image); // LodePNG uses standard free()
    esp_http_client_cleanup(client);
    free(http_buf.data);

    return (out_buf_size >= needed_bytes);
}