// SPDX-License-Identifier: MIT
#include "vocal_dsp.h"
#include <math.h>
#include <string.h>
static float bound(float v,float lo,float hi) { return isfinite(v)?fmaxf(lo,fminf(hi,v)):lo; }
int vocal_dsp_init(vocal_dsp_t *d,uint32_t rate)
{
    if(!d||rate<8000||rate>192000) return -1;
    memset(d,0,sizeof(*d)); d->gain=1;
    d->low_alpha=1-expf(-6.28318530718f*180/rate);
    d->high_alpha=1-expf(-6.28318530718f*4000/rate);
    d->smooth_alpha=1-expf(-1.f/(rate*.02f)); return 0;
}
static int16_t quantize(vocal_dsp_t *d,float v)
{
    if(v>32767) { ++d->limited_samples; return 32767; }
    if(v< -32768) { ++d->limited_samples; return -32768; }
    return (int16_t)lrintf(v);
}
void vocal_dsp_process(vocal_dsp_t *d,const int16_t *in,int16_t *out,size_t frames,
                      banban_mode_t mode,float strength,float gain)
{
    if(!d||!in||!out) return;
    strength=bound(strength,0,1); gain=bound(gain,0,1);
    for(size_t i=0;i<frames;++i) {
        float l=in[i*2],r=in[i*2+1],mid=(l+r)*.5f,side=(l-r)*.5f;
        d->low+=d->low_alpha*(mid-d->low);
        d->high+=d->high_alpha*(mid-d->high);
        d->basic_mix+=d->smooth_alpha*((mode==BANBAN_BASIC_CUT?1.f:0)-d->basic_mix);
        d->preserve_mix+=d->smooth_alpha*((mode==BANBAN_MUSIC_PRESERVE?1.f:0)-d->preserve_mix);
        d->strength+=d->smooth_alpha*(strength-d->strength);
        d->gain+=d->smooth_alpha*(gain-d->gain);
        // BASIC：差分受控复制到双声道，满强度失去立体声但折叠不互消。
        float b=d->basic_mix*d->strength;
        // 保留低频90%、中频15%、高频65%的Mid；Side不变。参数是混合量而非去人声百分比。
        float removed=.10f*d->low+.85f*(d->high-d->low)+.35f*(mid-d->high);
        float p=d->preserve_mix*d->strength;
        out[i*2]=quantize(d,(l+b*(side-l)-p*removed)*d->gain);
        out[i*2+1]=quantize(d,(r+b*(side-r)-p*removed)*d->gain);
    }
}
