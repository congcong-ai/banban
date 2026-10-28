// SPDX-License-Identifier: MIT
#pragma once
#include "esp_err.h"
typedef enum { BANBAN_KEY_PLUS, BANBAN_KEY_MINUS, BANBAN_KEY_PLAY, BANBAN_KEY_SET } banban_key_t;
typedef void (*banban_key_fn)(banban_key_t key);
// 回调仅更新参数，不执行音频、USB或蓝牙操作。
esp_err_t banban_controls_start(banban_key_fn callback);
