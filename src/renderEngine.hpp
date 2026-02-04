#pragma once
#include <Arduino.h>

uint8_t *init_framebuffer(void);
int render_frame(uint8_t *framebuffer);