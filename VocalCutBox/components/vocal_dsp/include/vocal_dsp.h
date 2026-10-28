// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef enum { BANBAN_BYPASS, BANBAN_BASIC_CUT, BANBAN_MUSIC_PRESERVE } banban_mode_t;
typedef struct {
    float low, high, basic_mix, preserve_mix, strength, gain;
    float low_alpha, high_alpha, smooth_alpha;
    uint64_t limited_samples;
} vocal_dsp_t;
// 纯运算层：输入输出均为PCM16交错立体声，可原地处理；参数由调用线程持有。
int vocal_dsp_init(vocal_dsp_t *dsp, uint32_t sample_rate);
void vocal_dsp_process(vocal_dsp_t *dsp, const int16_t *in, int16_t *out, size_t frames,
                      banban_mode_t mode, float strength, float gain);
