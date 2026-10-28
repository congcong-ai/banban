// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
// 无IDF依赖；非法、截断或冲突描述符返回0。
unsigned banban_usb_audio_version(const uint8_t *data,size_t bytes);
