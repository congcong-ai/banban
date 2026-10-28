// SPDX-License-Identifier: Apache-2.0
#include "vocal_dsp.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#define N 48000
static int16_t in[N*2],out[N*2];
static double rms(const int16_t *p,size_t begin,size_t end)
{ double sum=0; for(size_t i=begin*2;i<end*2;++i) sum+=(double)p[i]*p[i]; return sqrt(sum/((end-begin)*2)); }
static double measure_signal(unsigned rate,unsigned hz,int mode,int centered)
{
    vocal_dsp_t d; assert(!vocal_dsp_init(&d,rate));
    for(unsigned i=0;i<rate;++i) { int16_t s=lrint(12000*sin(6.28318530718*hz*i/rate)); in[i*2]=s; in[i*2+1]=centered?s:-s; }
    vocal_dsp_process(&d,in,out,rate,mode,1,1);
    double ratio=rms(out,rate/2,rate)/rms(in,rate/2,rate);
    printf("%u,%u,%d,%d,%.6f,%.3f\n",rate,hz,mode,centered,ratio,20*log10(fmax(1e-12,ratio)));
    return ratio;
}
int main(void)
{
    puts("采样率,频率,模式,中置,幅度比,变化dB");
    unsigned rates[]={44100,48000};
    for(unsigned r=0;r<2;++r) {
        assert(fabs(measure_signal(rates[r],1000,BANBAN_BYPASS,1)-1)<1e-5);
        assert(measure_signal(rates[r],1000,BANBAN_BASIC_CUT,1)<.001);
        assert(measure_signal(rates[r],1000,BANBAN_MUSIC_PRESERVE,1)<.35);
        assert(measure_signal(rates[r],80,BANBAN_MUSIC_PRESERVE,1)>.7);
        assert(fabs(measure_signal(rates[r],1000,BANBAN_MUSIC_PRESERVE,0)-1)<1e-5);
    }
    vocal_dsp_t d; assert(vocal_dsp_init(&d,0)==-1); assert(!vocal_dsp_init(&d,48000));
    for(unsigned i=0;i<N*2;++i) in[i]=i%3?INT16_MAX:INT16_MIN;
    for(unsigned block=0;block<100;++block) {
        vocal_dsp_process(&d,in,out,1,block%3,(block%101)/100.f,1);
        vocal_dsp_process(&d,in,out,31,block%3,.75f,.4f);
    }
    vocal_dsp_process(&d,in,out,0,BANBAN_BASIC_CUT,1,1);
    for(unsigned i=0;i<N*2;++i) in[i]=0;
    assert(!vocal_dsp_init(&d,48000));
    vocal_dsp_process(&d,in,out,N,BANBAN_MUSIC_PRESERVE,1,1);
    for(unsigned i=0;i<N*2;++i) assert(out[i]==0);
    // 原地直通逐采样一致；零强度与模式切换不引入输出突变。
    for(unsigned i=0;i<N*2;++i) in[i]=(int16_t)(i*77);
    assert(!vocal_dsp_init(&d,48000));
    vocal_dsp_process(&d,in,out,N,BANBAN_BYPASS,0,1);
    for(unsigned i=0;i<N*2;++i) assert(out[i]==in[i]);
    vocal_dsp_process(&d,in,in,N,BANBAN_BYPASS,0,1);
    for(unsigned i=0;i<N*2;++i) assert(out[i]==in[i]);
    fprintf(stderr,"通过：双采样率、频带/侧声道、零输入、满幅、短块、原地处理、模式切换。\n");
}
