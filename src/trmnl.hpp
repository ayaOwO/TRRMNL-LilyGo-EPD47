#pragma once
#include <cstdint>
#include <cstddef>
#include <Arduino.h>

struct DisplayConfig {
    String image_url;
    int refresh_rate;
    bool success;
};

DisplayConfig get_display_config();
bool fetch_and_convert_image(const char* url, uint8_t* out_buf, size_t out_buf_size);