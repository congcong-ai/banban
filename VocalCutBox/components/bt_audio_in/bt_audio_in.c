// SPDX-License-Identifier: MIT
#include <stdatomic.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "bt_audio_in.h"
static const char *TAG="bt_in";
#define FRAMES 256
#define BLOCKS 32
// 固定有界队列；回调仅复制PCM，禁止USB写入、DSP与文件IO。
typedef struct { uint32_t epoch,rate; uint16_t count,channels; int64_t time_us; int16_t pcm[FRAMES*2]; } pcm_block_t;
static QueueHandle_t queue;
static atomic_uint epoch=1,input_rate=44100,input_channels=2,overruns,underruns,stale;
static atomic_bool playing;
static atomic_bool connected;
static pcm_block_t current;
static unsigned position,render_epoch;
static float left_a,right_a,left_b,right_b,phase;
static bool primed;
static float gate,last_l,last_r;
void bt_audio_in_discard(void) { atomic_fetch_add(&epoch,1); }
static void pcm_callback(const uint8_t *data,uint32_t len)
{
    if(!atomic_load(&playing))return;
    unsigned channels=atomic_load(&input_channels),bytes=channels*2;
    if(len%bytes) { atomic_fetch_add(&overruns,1);return; }
    while(len) {
        pcm_block_t b={.epoch=atomic_load(&epoch),.rate=atomic_load(&input_rate),.channels=channels,.time_us=esp_timer_get_time()};
        b.count=len/bytes>FRAMES?FRAMES:len/bytes;
        memcpy(b.pcm,data,b.count*bytes);
        // 丢弃本次剩余突发，保留已排队音频的连续性。溢出不等于换歌或断连，
        // 不能递增epoch将整个队列全部作废，否则会形成溢出→清空→欠载循环。
        if(xQueueSend(queue,&b,0)!=pdTRUE) { atomic_fetch_add(&overruns,1);return; }
        data+=b.count*bytes;len-=b.count*bytes;
    }
}
static void a2dp_event(esp_a2d_cb_event_t event,esp_a2d_cb_param_t *p)
{
    switch(event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        ESP_LOGI(TAG,"连接状态=%d",p->conn_stat.state);
        if(p->conn_stat.state==ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            atomic_store(&connected,false);
            atomic_store(&playing,false);bt_audio_in_discard();
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE,ESP_BT_GENERAL_DISCOVERABLE);
            ESP_LOGI(TAG,"未连接：重新可发现，手机/电脑可直接点选连接");
        } else if(p->conn_stat.state==ESP_A2D_CONNECTION_STATE_CONNECTED) {
            atomic_store(&connected,true);
            esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE,ESP_BT_NON_DISCOVERABLE);
            ESP_LOGI(TAG,"已连接：停止发现，拒绝第二台设备抢占");
        }
        break;
    case ESP_A2D_AUDIO_STATE_EVT:
        atomic_store(&playing,p->audio_stat.state==ESP_A2D_AUDIO_STATE_STARTED);
        bt_audio_in_discard();ESP_LOGI(TAG,"播放状态=%d",p->audio_stat.state);break;
    case ESP_A2D_AUDIO_CFG_EVT: {
        atomic_store(&playing,false);bt_audio_in_discard();
        if(p->audio_cfg.mcc.type!=ESP_A2D_MCT_SBC) { ESP_LOGE(TAG,"不支持非SBC编码");break; }
        esp_a2d_cie_sbc_t *s=&p->audio_cfg.mcc.cie.sbc_info;
        unsigned rate=s->samp_freq&ESP_A2D_SBC_CIE_SF_48K?48000:s->samp_freq&ESP_A2D_SBC_CIE_SF_44K?44100:
            s->samp_freq&ESP_A2D_SBC_CIE_SF_32K?32000:16000;
        atomic_store(&input_rate,rate);atomic_store(&input_channels,s->ch_mode&ESP_A2D_SBC_CIE_CH_MODE_MONO?1:2);
        ESP_LOGI(TAG,"解码格式=%uHz/PCM16/%uch",rate,atomic_load(&input_channels));break;
    }
    case ESP_A2D_PROF_STATE_EVT:
        if(p->a2d_prof_stat.init_state==ESP_A2D_INIT_SUCCESS) {
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE,ESP_BT_GENERAL_DISCOVERABLE);
            ESP_LOGI(TAG,"VocalCutBox 已可发现；SSP Just Works，无屏幕/PIN核对");
        }
        break;
    default:break;
    }
}
static void gap_event(esp_bt_gap_cb_event_t e,esp_bt_gap_cb_param_t *p)
{
    if(e==ESP_BT_GAP_AUTH_CMPL_EVT) ESP_LOGI(TAG,"配对结果=%d",p->auth_cmpl.stat);
    else if(e==ESP_BT_GAP_CFM_REQ_EVT) esp_bt_gap_ssp_confirm_reply(p->cfm_req.bda,true);
    else if(e==ESP_BT_GAP_PIN_REQ_EVT) {
        // 本项目采用SSP Just Works，不提供需要人工输入PIN的旧式配对。
        esp_bt_pin_code_t pin={0};
        esp_bt_gap_pin_reply(p->pin_req.bda,false,0,pin);
        ESP_LOGW(TAG,"拒绝需手工PIN的旧式配对；请使用支持SSP的音频源");
    }
}
static bool sample(float *l,float *r,int64_t deadline)
{
    for(;;) {
        if(position<current.count&&current.epoch==render_epoch) {
            *l=current.pcm[position*current.channels];*r=current.channels==2?current.pcm[position*2+1]:*l;
            ++position;return true;
        }
        int64_t remaining=deadline-esp_timer_get_time();
        TickType_t wait=remaining>0?pdMS_TO_TICKS((unsigned)(remaining/1000)):0;
        if(xQueueReceive(queue,&current,wait)!=pdTRUE) return false;
        position=0;
        if(current.epoch!=render_epoch||esp_timer_get_time()-current.time_us>150000) { current.count=0;atomic_fetch_add(&stale,1); }
    }
}
void bt_audio_in_render(int16_t *pcm,size_t frames,uint32_t output_rate)
{
    unsigned e=atomic_load(&epoch);
    if(e!=render_epoch) { render_epoch=e;primed=false;current.count=position=0;phase=0; }
    // 无设备时回调队列仍有界；接回后丢弃150ms以前的PCM。
    bool starved=false;
    // 整个输出块共享一个等待截止点，而非每采样等待。让SBC解码任务有机会
    // 补齐下一批PCM；上限12ms，USB已有环形缓冲继续发送，蓝牙回调始终不阻塞。
    int64_t deadline=esp_timer_get_time()+12000;
    const float ramp_step=1.f/(output_rate*.005f);
    for(size_t i=0;i<frames;++i) {
        bool valid=false;
        if(atomic_load(&playing)) {
            if(!primed&&(position<current.count||uxQueueMessagesWaiting(queue)>=3)) {
                primed=sample(&left_a,&right_a,deadline)&&sample(&left_b,&right_b,deadline);phase=0;
            }
            if(primed) {
                last_l=left_a+(left_b-left_a)*phase;
                last_r=right_a+(right_b-right_a)*phase;
                valid=true;
                phase+=(float)atomic_load(&input_rate)/output_rate;
                while(phase>=1) {
                    phase-=1;left_a=left_b;right_a=right_b;
                    if(!sample(&left_b,&right_b,deadline)) {primed=false;starved=true;break;}
                }
            } else starved=true;
        } else primed=false;
        gate=valid?fminf(1,gate+ramp_step):fmaxf(0,gate-ramp_step);
        pcm[i*2]=(int16_t)(last_l*gate);pcm[i*2+1]=(int16_t)(last_r*gate);
    }
    if(starved) atomic_fetch_add(&underruns,1);
    static int64_t last;
    if(esp_timer_get_time()-last>5000000) {
        ESP_LOGI(TAG,"队列=%u/%d 溢出=%u 欠载块=%u 旧块丢弃=%u",(unsigned)uxQueueMessagesWaiting(queue),BLOCKS,
                 atomic_load(&overruns),atomic_load(&underruns),atomic_load(&stale));last=esp_timer_get_time();
    }
}
esp_err_t bt_audio_in_start(void)
{
    // 不自动erase NVS，避免清除现有设备数据；需要擦除时返回错误交由人工判断。
    esp_err_t e=nvs_flash_init();if(e!=ESP_OK)return e;
    queue=xQueueCreate(BLOCKS,sizeof(pcm_block_t));if(!queue)return ESP_ERR_NO_MEM;
    esp_bt_controller_config_t cfg=BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    e=esp_bt_controller_init(&cfg);if(e!=ESP_OK)return e;
    ESP_LOGI(TAG,"控制器初始化完成，启用BR/EDR，heap=%u",(unsigned)esp_get_free_heap_size());
    e=esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);if(e!=ESP_OK)return e;
    ESP_LOGI(TAG,"BR/EDR控制器启用完成，初始化Bluedroid，heap=%u",(unsigned)esp_get_free_heap_size());
    e=esp_bluedroid_init();if(e!=ESP_OK)return e;
    ESP_LOGI(TAG,"Bluedroid初始化完成，heap=%u",(unsigned)esp_get_free_heap_size());
    e=esp_bluedroid_enable();if(e!=ESP_OK)return e;
    ESP_LOGI(TAG,"Bluedroid启用完成，注册A2DP Sink，heap=%u",(unsigned)esp_get_free_heap_size());
    esp_bt_io_cap_t cap=ESP_BT_IO_CAP_NONE;
    ESP_ERROR_CHECK(esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE,&cap,sizeof(cap)));
    ESP_ERROR_CHECK(esp_bt_gap_register_callback(gap_event));
    ESP_ERROR_CHECK(esp_bt_gap_set_device_name("VocalCutBox"));
    esp_bt_cod_t cod={.major=ESP_BT_COD_MAJOR_DEV_AV,.minor=0,.service=ESP_BT_COD_SRVC_RENDERING|ESP_BT_COD_SRVC_AUDIO};
    ESP_ERROR_CHECK(esp_bt_gap_set_cod(cod,ESP_BT_INIT_COD));
    esp_bt_eir_data_t eir={.include_name=true,.include_uuid=true};
    ESP_ERROR_CHECK(esp_bt_gap_config_eir_data(&eir));
    ESP_ERROR_CHECK(esp_a2d_register_callback(a2dp_event));
    ESP_ERROR_CHECK(esp_a2d_sink_register_data_callback(pcm_callback));
    return esp_a2d_sink_init();
}
