// SPDX-License-Identifier: Apache-2.0
#include "vocal_dsp.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv)
{
    if(argc!=5) return 2;
    vocal_dsp_t d;
    if(vocal_dsp_init(&d,(unsigned)strtoul(argv[1],NULL,10))) return 2;
    int16_t pcm[512]; size_t n;
    while((n=fread(pcm,sizeof(*pcm),512,stdin))) {
        if(n%2) return 3;
        vocal_dsp_process(&d,pcm,pcm,n/2,atoi(argv[2]),strtof(argv[3],NULL),strtof(argv[4],NULL));
        if(fwrite(pcm,sizeof(*pcm),n,stdout)!=n) return 4;
    }
    return ferror(stdin)?5:0;
}
