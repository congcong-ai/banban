// SPDX-License-Identifier: MIT
#include "test_tone.h"
#include <string.h>
#include "test_tone_table.h"
int banban_test_tone_init(banban_test_tone_t *t,uint32_t rate)
{
    if(!t||rate<8000||rate>192000)return -1;
    memset(t,0,sizeof(*t));t->rate=rate;
    t->step_l=(uint32_t)(((uint64_t)440<<32)/rate);
    t->step_r=(uint32_t)(((uint64_t)880<<32)/rate);
    return 0;
}
void banban_test_tone_process(banban_test_tone_t *t,int16_t *pcm,size_t frames,bool enabled,unsigned volume)
{
    if(!t||!pcm||!t->rate)return;
    if(volume>50)volume=50;
    int target=enabled?(int)(volume*65536/100):0;
    if(target!=t->target_q16) {
        t->target_q16=target;int delta=target-t->gain_q16;if(delta<0)delta=-delta;
        t->ramp_step=(delta+(int)t->rate/100-1)/((int)t->rate/100);
    }
    unsigned fade=t->rate/50;
    for(size_t i=0;i<frames;++i) {
        if(t->gain_q16<target) {t->gain_q16+=t->ramp_step;if(t->gain_q16>target)t->gain_q16=target;}
        else if(t->gain_q16>target) {t->gain_q16-=t->ramp_step;if(t->gain_q16<target)t->gain_q16=target;}
        unsigned edge=t->frame<t->rate-1-t->frame?t->frame:t->rate-1-t->frame;
        unsigned envelope=edge>=fade?32767:edge*32767/fade;
        int l=(t->segment==0||t->segment==2)?sine_table[t->phase_l>>21]:0;
        int r=(t->segment==1||t->segment==2)?sine_table[t->phase_r>>21]:0;
        pcm[i*2]=(int16_t)(((l*t->gain_q16/65536)*(int)envelope)/32767);
        pcm[i*2+1]=(int16_t)(((r*t->gain_q16/65536)*(int)envelope)/32767);
        t->phase_l+=t->step_l;t->phase_r+=t->step_r;
        if(++t->frame==t->rate) {t->frame=0;t->segment=(t->segment+1)%4;t->phase_l=t->phase_r=0;}
    }
}
