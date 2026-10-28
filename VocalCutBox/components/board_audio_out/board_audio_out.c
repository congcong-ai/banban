/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * 板级初始化节选并整理自官方esp-dev-kits S31-Korvo BSP，固定来源及范围见docs/08_BOARD_SPEAKER_TEST.md。
 * 只打开DAC/功放；不创建录音业务、屏幕或相机。此输出禁止连接D100 RCA。
 */
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "board_audio_out.h"
static const char *TAG="board_audio";
static esp_codec_dev_handle_t speaker;
static banban_render_fn render_pcm;
static void *render_context;
static void playback_task(void *arg)
{
    int16_t pcm[480*2];uint32_t blocks=0,peak=0;int64_t report=esp_timer_get_time();
    for(;;) {
        render_pcm(pcm,480,48000,render_context);
        for(unsigned i=0;i<960;++i) { unsigned p=pcm[i]<0?-(int32_t)pcm[i]:pcm[i];if(p>peak)peak=p; }
        int err=esp_codec_dev_write(speaker,pcm,sizeof(pcm));
        if(err!=ESP_CODEC_DEV_OK) { ESP_LOGE(TAG,"I2S写入失败=%d",err);vTaskDelay(pdMS_TO_TICKS(100)); }
        else ++blocks;
        if(esp_timer_get_time()-report>=5000000) {
            ESP_LOGI(TAG,"48kHz/16bit/2ch 写入块=%u PCM峰值=%u heap=%u 栈余=%u",blocks,peak,(unsigned)esp_get_free_heap_size(),(unsigned)uxTaskGetStackHighWaterMark(NULL));
            report=esp_timer_get_time();peak=0;
        }
    }
}
esp_err_t board_audio_out_start(banban_render_fn render,void *context)
{
    if(!render)return ESP_ERR_INVALID_ARG;
    i2c_master_bus_handle_t bus;
    const i2c_master_bus_config_t bc={.clk_source=I2C_CLK_SRC_DEFAULT,.sda_io_num=GPIO_NUM_0,.scl_io_num=GPIO_NUM_1,.i2c_port=I2C_NUM_0,.flags.enable_internal_pullup=true};
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bc,&bus),TAG,"创建板载I2C失败");
    i2s_chan_handle_t tx,rx;
    i2s_chan_config_t cc=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);cc.auto_clear=true;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&cc,&tx,&rx),TAG,"创建BSP基线I2S通道失败");
    const i2s_std_config_t sc={.clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(48000),
        .slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO),
        .gpio_cfg={.mclk=GPIO_NUM_2,.bclk=GPIO_NUM_3,.ws=GPIO_NUM_4,.dout=GPIO_NUM_5,.din=GPIO_NUM_6}};
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(tx,&sc),TAG,"I2S TX初始化失败");
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(rx,&sc),TAG,"I2S RX初始化失败");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(tx),TAG,"I2S TX启用失败");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(rx),TAG,"I2S RX启用失败");
    audio_codec_i2s_cfg_t dc={.port=I2S_NUM_0,.tx_handle=tx,.rx_handle=rx};
    const audio_codec_data_if_t *data=audio_codec_new_i2s_data(&dc);
    const audio_codec_gpio_if_t *gpio=audio_codec_new_gpio();
    audio_codec_i2c_cfg_t ic={.port=I2C_NUM_0,.addr=ES8389_CODEC_DEFAULT_ADDR,.bus_handle=bus};
    const audio_codec_ctrl_if_t *ctrl=audio_codec_new_i2c_ctrl(&ic);
    if(!data||!gpio||!ctrl)return ESP_ERR_NO_MEM;
    es8389_codec_cfg_t ec={.ctrl_if=ctrl,.gpio_if=gpio,.codec_mode=ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin=GPIO_NUM_7,.pa_reverted=false,.master_mode=false,.use_mclk=false,.digital_mic=false,
        .invert_mclk=false,.invert_sclk=false,.hw_gain={.pa_voltage=5.0f,.codec_dac_voltage=3.3f},.no_dac_ref=false};
    const audio_codec_if_t *codec=es8389_codec_new(&ec);if(!codec)return ESP_FAIL;
    esp_codec_dev_cfg_t device={.dev_type=ESP_CODEC_DEV_TYPE_OUT,.codec_if=codec,.data_if=data};
    speaker=esp_codec_dev_new(&device);if(!speaker)return ESP_FAIL;
    esp_codec_dev_sample_info_t sample={.sample_rate=48000,.bits_per_sample=16,.channel=2,.channel_mask=0};
    int e=esp_codec_dev_open(speaker,&sample);if(e!=ESP_CODEC_DEV_OK) {ESP_LOGE(TAG,"ES8389打开失败=%d",e);return ESP_FAIL;}
    e=esp_codec_dev_set_out_vol(speaker,80);if(e!=ESP_CODEC_DEV_OK)return ESP_FAIL;
    e=esp_codec_dev_set_out_mute(speaker,false);if(e!=ESP_CODEC_DEV_OK)return ESP_FAIL;
    render_pcm=render;render_context=context;
    if(xTaskCreate(playback_task,"board_pcm",8192,NULL,6,NULL)!=pdPASS)return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG,"板载ES8389 DAC对照已启动，功放GPIO7；Codec音量80，测试音由同一render生成；USB Host未启动");
    return ESP_OK;
}
