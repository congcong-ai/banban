#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
// 回调只在音频任务执行；返回交错 PCM16 双声道，帧数不等于采样数。
typedef void (*banban_render_fn)(int16_t *pcm, size_t frames, uint32_t rate, void *context);
esp_err_t usb_audio_out_start(banban_render_fn render, void *context);

// 仅发出有界重配请求；流关闭/重开由音频任务完成。
esp_err_t usb_audio_out_set_format(uint32_t rate,unsigned bits);
