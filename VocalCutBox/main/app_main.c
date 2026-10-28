// SPDX-License-Identifier: MIT
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "usb_audio_out.h"
#include "vocal_dsp.h"
#include "test_tone.h"
#include "board_audio_out.h"
#include "controls.h"
#if CONFIG_BANBAN_BT_AUDIO
#include "bt_audio_in.h"
#endif
#if CONFIG_BANBAN_BOARD_SPEAKER_TEST
static atomic_int volume=20;
#else
static atomic_int volume=10;
#endif
// 设备启动先静音，避免插接耳机或有源音响时意外发声。
static atomic_bool tone=false;
static atomic_int mode=BANBAN_BYPASS,strength=100,last_cut=BANBAN_BASIC_CUT;
#if CONFIG_BANBAN_BT_AUDIO
static atomic_bool use_bt=true;
#endif
static vocal_dsp_t dsp;
static uint32_t dsp_rate;
static banban_test_tone_t test_tone;
static void key(banban_key_t pressed)
{
    int active=atomic_load(&mode);
    if(pressed==BANBAN_KEY_PLAY) {
        if(active==BANBAN_BYPASS)atomic_store(&mode,atomic_load(&last_cut));
        else {atomic_store(&last_cut,active);atomic_store(&mode,BANBAN_BYPASS);}
    } else if(pressed==BANBAN_KEY_SET) {
        int next=atomic_load(&last_cut)==BANBAN_BASIC_CUT?BANBAN_MUSIC_PRESERVE:BANBAN_BASIC_CUT;
        atomic_store(&last_cut,next);
        if(active!=BANBAN_BYPASS)atomic_store(&mode,next);
    } else {
        int previous=atomic_load(&strength),next;
        do {
            next=previous+(pressed==BANBAN_KEY_PLUS?5:-5);
            if(next<0)next=0;
            if(next>100)next=100;
        } while(!atomic_compare_exchange_weak(&strength,&previous,next));
    }
    ESP_LOGI("按键","键=%d 模式=%d（0原曲/1基础/2保留） 下次消除=%d 强度=%d 播放音量=%d",
             pressed,atomic_load(&mode),atomic_load(&last_cut),atomic_load(&strength),atomic_load(&volume));
}
static void render(int16_t *pcm,size_t frames,uint32_t rate,void *ctx)
{
    if(dsp_rate!=rate) { vocal_dsp_init(&dsp,rate);dsp.gain=0;dsp_rate=rate;banban_test_tone_init(&test_tone,rate); }
#if CONFIG_BANBAN_BT_AUDIO
    if(atomic_load(&use_bt)) {
        bt_audio_in_render(pcm,frames,rate);
        vocal_dsp_process(&dsp,pcm,pcm,frames,atomic_load(&mode),atomic_load(&strength)*.01f,atomic_load(&volume)*.01f);
        return;
    }
#endif
    banban_test_tone_process(&test_tone,pcm,frames,atomic_load(&tone),atomic_load(&volume));
}
static void console(void *arg)
{
    char line[96]; unsigned n=0;
    ESP_LOGI("控制","命令：tone on | tone off | volume 0..50 | usb-format 48000 16/24/32");
    for (;;) {
        int c=getchar();
        if(c==EOF) { clearerr(stdin);vTaskDelay(pdMS_TO_TICKS(20)); continue; }
        if(c=='\n'||c=='\r') {
            line[n]=0; int v;unsigned format_rate,format_bits;
            if(!strcmp(line,"tone on")) {
                atomic_store(&tone,true);
#if CONFIG_BANBAN_BT_AUDIO
                atomic_store(&use_bt,false);bt_audio_in_discard();
#endif
            }
#if CONFIG_BANBAN_BT_AUDIO
            else if(!strcmp(line,"source bt")) { atomic_store(&use_bt,true);bt_audio_in_discard(); }
#endif
            else if(sscanf(line,"usb-format %u %u",&format_rate,&format_bits)==2) {
                esp_err_t err=usb_audio_out_set_format(format_rate,format_bits);
                ESP_LOGI("控制","USB格式请求=%s",esp_err_to_name(err));
            }
            else if(!strcmp(line,"mode bypass")) atomic_store(&mode,BANBAN_BYPASS);
            else if(!strcmp(line,"mode basic")) {atomic_store(&mode,BANBAN_BASIC_CUT);atomic_store(&last_cut,BANBAN_BASIC_CUT);}
            else if(!strcmp(line,"mode preserve")) {atomic_store(&mode,BANBAN_MUSIC_PRESERVE);atomic_store(&last_cut,BANBAN_MUSIC_PRESERVE);}
            else if(sscanf(line,"strength %d",&v)==1&&v>=0&&v<=100) atomic_store(&strength,v);
            else if(!strcmp(line,"tone off")) atomic_store(&tone,false);
            else if(sscanf(line,"volume %d",&v)==1&&v>=0&&v<=50) atomic_store(&volume,v);
            else if(n) ESP_LOGW("控制","无效命令");
            if(n) ESP_LOGI("控制","测试音=%d 播放音量=%d/50 模式=%d 强度=%d",atomic_load(&tone),atomic_load(&volume),atomic_load(&mode),atomic_load(&strength));
            n=0;
        } else if(n<sizeof(line)-1) line[n++]=(char)c;
        else n=0;
    }
}
void app_main(void)
{
    esp_chip_info_t chip; esp_chip_info(&chip);
    ESP_LOGI("banban","伴伴 0.2.0，目标="CONFIG_IDF_TARGET" 芯片型号=%d rev=%d 核=%d IDF=%s heap=%u",
             chip.model,chip.revision,chip.cores,esp_get_idf_version(),(unsigned)esp_get_free_heap_size());
    esp_err_t controls=banban_controls_start(key);
    if(controls!=ESP_OK)ESP_LOGW("按键","初始化失败=%s，继续音频并使用串口控制",esp_err_to_name(controls));
#if CONFIG_BANBAN_BT_AUDIO
    ESP_ERROR_CHECK(bt_audio_in_start());
#endif
#if CONFIG_BANBAN_BOARD_SPEAKER_TEST
    ESP_ERROR_CHECK(board_audio_out_start(render,NULL));
#else
    ESP_ERROR_CHECK(usb_audio_out_start(render,NULL));
#endif
    configASSERT(xTaskCreate(console,"console",4096,NULL,3,NULL)==pdPASS);
}
