// SPDX-License-Identifier: Apache-2.0
#include "test_tone.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int16_t pcm[96000],split[96000];
int main(void)
{
    const unsigned rates[]={44100,48000};
    for(unsigned z=0;z<2;++z) {
        unsigned rate=rates[z];banban_test_tone_t a,b;
        assert(!banban_test_tone_init(&a,rate));assert(!banban_test_tone_init(&b,rate));
        banban_test_tone_process(&a,pcm,rate,true,25);
        for(unsigned i=0;i<rate;) {unsigned n=rate-i>173?173:rate-i;banban_test_tone_process(&b,split+i*2,n,true,25);i+=n;}
        assert(!memcmp(pcm,split,rate*2*sizeof(*pcm)));
        unsigned crossings=0;int peak=0;
        for(unsigned i=1;i<rate;++i) {
            assert(pcm[i*2+1]==0);int p=abs(pcm[i*2]);if(p>peak)peak=p;
            if(pcm[(i-1)*2]<=0&&pcm[i*2]>0)++crossings;
        }
        assert(crossings>=439&&crossings<=441&&peak>=810&&peak<=820);
        banban_test_tone_process(&a,pcm,rate,true,25);crossings=0;
        for(unsigned i=1;i<rate;++i) {assert(pcm[i*2]==0);if(pcm[(i-1)*2+1]<=0&&pcm[i*2+1]>0)++crossings;}
        assert(crossings>=879&&crossings<=881);
        banban_test_tone_process(&a,pcm,rate,true,25);
        banban_test_tone_process(&a,pcm,rate,true,25);
        for(unsigned i=0;i<rate*2;++i)assert(pcm[i]==0);
        banban_test_tone_process(&a,pcm,rate,true,25);
        banban_test_tone_process(&a,pcm,rate,false,25);
        for(unsigned i=rate/20*2;i<rate*2;++i)assert(pcm[i]==0);
        printf("%uHz：左右频率、峰值、静音、淡出与任意分块一致性通过\n",rate);
    }
}
