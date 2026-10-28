// SPDX-License-Identifier: Apache-2.0
#include "usb_descriptor.h"
#include <assert.h>
#include <stdio.h>
int main(int argc,char **argv)
{
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    unsigned char b[4096];size_t n=fread(b,1,sizeof(b),f);fclose(f);
    assert(n==422&&banban_usb_audio_version(b,n)==0x0200);
    for(size_t i=0;i<n;++i)assert(!banban_usb_audio_version(b,i));
    unsigned char uac1[]={9,2,23,0,1,1,0,0x80,50, 9,4,0,0,0,1,1,0,0, 5,0x24,1,0,1};
    assert(banban_usb_audio_version(uac1,sizeof(uac1))==0x0100);
    uac1[18]=0;assert(!banban_usb_audio_version(uac1,sizeof(uac1)));
    assert(!banban_usb_audio_version(NULL,0));
    puts("通过：实物422字节UAC2描述符、UAC1基线、所有截断长度和非法零长描述符。");
}
