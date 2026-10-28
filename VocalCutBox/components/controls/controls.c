/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * ADC初始化、电压梯形与取样模式节选自固定S31 BSP；出处见docs/10_BUTTON_CONTROLS.md。
 */
#include <stdint.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "controls.h"
#include "bsp_adc_calibration.h"
#include "button_interface.h"
#include "iot_button.h"
#include "esp_timer.h"
#include "esp_log.h"
static const char *TAG="按键";
// 当前S31预览SDK的ADC读取持有rtc_spinlock再取regi2c锁；PHY温度跟踪
// 在esp_timer任务中反向取锁。与该任务同核，避免两个核同时进入这条路径。
_Static_assert(CONFIG_ESP_TIMER_TASK_AFFINITY==0,"S31按键ADC要求esp_timer固定在CPU0，请复核SDK锁顺序");
static adc_oneshot_unit_handle_t adc;
static button_handle_t buttons[4];
static banban_key_fn notify;
typedef struct { button_driver_t base; unsigned index; } key_driver_t;
static key_driver_t drivers[4];
static atomic_int sampled_key=-1;
static atomic_uint sampled_tick;
// 官方/已验证BSP：GPIO42 = ADC1_CH0_N，中心380/820/1340/1870mV，空闲2000mV。
static const unsigned limits[5]={160,600,1080,1605,1935};
static void sample_keys(void *arg)
{
    // ADC读取放低优先级任务，避免在高优先级esp_timer里阻塞USB事件。
    // 四键同一梯形每20ms只读一批，事件去抖仍由iot_button负责。
    for(;;) {
        int raw=0,mv=0,detected=-1;unsigned sum=0;
        bool valid=true;
        for(unsigned i=0;i<4;++i) {
            if(adc_oneshot_read(adc,ADC_CHANNEL_0,&raw)!=ESP_OK) {valid=false;break;}
            sum+=raw;
        }
        if(valid&&bsp_s31_adc_calibration_raw_to_mv(ADC_UNIT_1,sum/4,&mv)==ESP_OK)
            for(unsigned i=0;i<4;++i)
                if(mv>(int)limits[i]&&mv<=(int)limits[i+1]) {detected=i;break;}
        atomic_store(&sampled_key,detected);
        atomic_store(&sampled_tick,xTaskGetTickCount());
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
static uint8_t level(button_driver_t *driver)
{
    key_driver_t *key=(key_driver_t *)driver;
    if((TickType_t)(xTaskGetTickCount()-atomic_load(&sampled_tick))>pdMS_TO_TICKS(100))return BUTTON_INACTIVE;
    return atomic_load(&sampled_key)==(int)key->index?BUTTON_ACTIVE:BUTTON_INACTIVE;
}
static void event(void *handle,void *arg)
{
    unsigned key=(uintptr_t)arg;
    // 使用成熟驱动的去抖/长按事件；限制长按重复到每200ms一次。
    static int64_t last_repeat[2];
    if(iot_button_get_event(handle)==BUTTON_LONG_PRESS_HOLD) {
        if(key>1)return;
        int64_t now=esp_timer_get_time();
        if(now-last_repeat[key]<200000)return;
        last_repeat[key]=now;
    }
    if(notify)notify((banban_key_t)key);
}
esp_err_t banban_controls_start(banban_key_fn callback)
{
    if(!callback||adc)return ESP_ERR_INVALID_ARG;
    adc_oneshot_unit_init_cfg_t unit={.unit_id=ADC_UNIT_1};
    esp_err_t err=adc_oneshot_new_unit(&unit,&adc);
    if(err!=ESP_OK)return err;
    adc_oneshot_chan_cfg_t channel={.atten=ADC_ATTEN_DB_0,.bitwidth=ADC_BITWIDTH_DEFAULT};
    err=adc_oneshot_config_channel(adc,ADC_CHANNEL_0,&channel);
    if(err==ESP_OK)err=bsp_s31_adc_calibration_init(adc,ADC_UNIT_1,ADC_CHANNEL_0);
    if(err!=ESP_OK)goto fail;
    notify=callback;
    button_config_t config={0};
    for(unsigned i=0;i<4;++i) {
        drivers[i].base.get_key_level=level;drivers[i].index=i;
        err=iot_button_create(&config,&drivers[i].base,&buttons[i]);
        if(err==ESP_OK)err=iot_button_register_cb(buttons[i],BUTTON_SINGLE_CLICK,NULL,event,(void *)(uintptr_t)i);
        if(err==ESP_OK&&i<2)err=iot_button_register_cb(buttons[i],BUTTON_LONG_PRESS_HOLD,NULL,event,(void *)(uintptr_t)i);
        if(err!=ESP_OK)goto fail;
    }
    if(xTaskCreatePinnedToCore(sample_keys,"key_adc",3072,NULL,2,NULL,0)!=pdPASS) {err=ESP_ERR_NO_MEM;goto fail;}
    ESP_LOGI(TAG,"四键已启动：PLAY原曲/消除，SET消除模式，VOL±强度（长按连续调节）");
    return ESP_OK;
fail:
    notify=NULL;
    for(unsigned i=0;i<4;++i)if(buttons[i]) {iot_button_delete(buttons[i]);buttons[i]=NULL;}
    adc_oneshot_del_unit(adc);adc=NULL;
    return err;
}
