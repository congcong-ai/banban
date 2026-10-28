// SPDX-License-Identifier: Apache-2.0
#include "usb/uac2_desc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv)
{
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    unsigned char b[4096];size_t n=fread(b,1,sizeof(b),f);fclose(f);
    uac2_device_info_t info;
    assert(n==422&&uac2_parse_config_descriptor(b,n,&info));
    assert(info.is_uac2&&info.bcdADC==0x0200&&info.num_clock_sources==2);
    assert(info.num_as_ifaces==5&&info.num_feature_units==2);
    assert(info.as_ifaces[0].interface_num==1&&info.as_ifaces[0].ep_addr==1);
    assert(info.as_ifaces[0].nr_channels==2&&info.as_ifaces[0].bit_resolution==16);
    assert(info.as_ifaces[1].bit_resolution==24&&info.as_ifaces[2].bit_resolution==32);
    assert(info.feature_units[0].unit_id==2&&info.feature_units[0].volume_ch_map==6&&info.feature_units[0].mute_ch_map==1);
    int clock=0;
    for(unsigned i=0;i<info.num_terminals;++i)if(info.terminals[i].terminal_id==3)clock=info.terminals[i].clock_source_id;
    assert(clock==9);
    for(size_t i=0;i<n;++i) {
        unsigned char *short_copy=malloc(i?i:1);assert(short_copy);memcpy(short_copy,b,i);
        assert(!uac2_parse_config_descriptor(short_copy,i,&info));free(short_copy);
    }
    assert(!uac2_parse_config_descriptor(NULL,0,&info));
    assert(!uac2_parse_config_descriptor(b,n,NULL));
    b[9]=0;assert(!uac2_parse_config_descriptor(b,n,&info));
    puts("通过：真实CX31993 UAC2拓扑、播放时钟9、左右音量控制位、三种PCM位宽及422种截断边界；ASan/UBSan无错误。");
}
