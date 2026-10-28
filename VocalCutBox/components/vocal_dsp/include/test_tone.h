// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef struct {
    uint32_t rate,phase_l,phase_r,step_l,step_r,frame;
    int gain_q16,target_q16,ramp_step;
    unsigned segment;
} banban_test_tone_t;
int banban_test_tone_init(banban_test_tone_t *tone,uint32_t rate);
// 低幅度立体声诊断信号：左440、右880、双声道、静音，每段1秒；volume=0..50。
void banban_test_tone_process(banban_test_tone_t *tone,int16_t *pcm,size_t frames,bool enabled,unsigned volume);
