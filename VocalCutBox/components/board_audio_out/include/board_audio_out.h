#pragma once
#include "usb_audio_out.h"
esp_err_t board_audio_out_start(banban_render_fn render,void *context);
