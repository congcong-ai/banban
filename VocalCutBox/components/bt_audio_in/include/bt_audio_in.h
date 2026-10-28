#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
esp_err_t bt_audio_in_start(void);
void bt_audio_in_render(int16_t *stereo,size_t frames,uint32_t output_rate);
void bt_audio_in_discard(void);
