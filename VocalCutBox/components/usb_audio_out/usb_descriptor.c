// SPDX-License-Identifier: MIT
#include "usb_descriptor.h"
unsigned banban_usb_audio_version(const uint8_t *raw,size_t bytes)
{
    if(!raw||bytes<9||raw[0]!=9||raw[1]!=2||((size_t)raw[2]|((size_t)raw[3]<<8))!=bytes) return 0;
    unsigned cls=0,subclass=0,version=0;
    for(size_t pos=0;pos<bytes;) {
        if(pos+2>bytes) return 0;
        unsigned n=raw[pos];if(n<2||pos+n>bytes)return 0;
        if(raw[pos+1]==4) {
            if(n<9)return 0;
            cls=raw[pos+5];subclass=raw[pos+6];
        }
        if(cls==1&&subclass==1&&raw[pos+1]==0x24&&n>=5&&raw[pos+2]==1) {
            unsigned v=raw[pos+3]|((unsigned)raw[pos+4]<<8);
            if(version&&version!=v)return 0;
            version=v;
        }
        pos+=n;
    }
    return version;
}
