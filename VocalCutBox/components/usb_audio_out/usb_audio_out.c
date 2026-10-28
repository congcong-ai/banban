// SPDX-License-Identifier: MIT
#include <inttypes.h>
#include <stdatomic.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "usb/usb_host.h"
#include "usb/uac_host.h"
#include "usb_audio_out.h"
#include "usb_descriptor.h"

static const char *TAG = "usb_out";
typedef struct { uint8_t addr, iface; } connection_t;
static QueueHandle_t connections, disconnected;
static atomic_uint transfer_errors, event_drops;
static banban_render_fn render_pcm;
static void *render_context;
static usb_host_client_handle_t observer;
// 0=尚未审查，1=UAC1可尝试，2=明确不支持；不把UAC2交给已知有缺陷的打开失败路径。
static atomic_uchar compatibility[128];

static void device_event(uac_host_device_handle_t h, uac_host_device_event_t e, void *arg)
{
    if (e == UAC_HOST_DRIVER_EVENT_DISCONNECTED) xQueueOverwrite(disconnected, &h);
    else if (e == UAC_HOST_DEVICE_EVENT_TRANSFER_ERROR) atomic_fetch_add(&transfer_errors, 1);
}
static void driver_event(uint8_t addr, uint8_t iface, uac_host_driver_event_t e, void *arg)
{
    if (e != UAC_HOST_DRIVER_EVENT_TX_CONNECTED) return;
    connection_t c = {addr, iface};
    if (xQueueSend(connections, &c, 0) != pdTRUE) atomic_fetch_add(&event_drops, 1);
}
static void observe(const usb_host_client_event_msg_t *e, void *arg)
{
    if (e->event != USB_HOST_CLIENT_EVENT_NEW_DEV) return;
    usb_device_handle_t d;
    if (usb_host_device_open(observer, e->new_dev.address, &d) != ESP_OK) return;
    atomic_store(&compatibility[e->new_dev.address],0);
    usb_device_info_t physical;
    if(usb_host_device_info(d,&physical)==ESP_OK) ESP_LOGI(TAG,"协商速度=%d（0低速、1全速、2高速）",physical.speed);
    const usb_device_desc_t *desc;
    const usb_config_desc_t *cfg;
    if (usb_host_get_device_descriptor(d, &desc) == ESP_OK) {
        ESP_LOGI(TAG, "USB 地址=%u VID=%04x PID=%04x bcdUSB=%04x 类=%u", e->new_dev.address,
                 desc->idVendor, desc->idProduct, desc->bcdUSB, desc->bDeviceClass);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, desc, sizeof(*desc), ESP_LOG_INFO);
    }
    if (usb_host_get_active_config_descriptor(d, &cfg) == ESP_OK) {
        ESP_LOGI(TAG, "配置描述符 bytes=%u 接口=%u（含 UAC 版本、端点和能力原始信息）", cfg->wTotalLength, cfg->bNumInterfaces);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, cfg, cfg->wTotalLength, ESP_LOG_INFO);
        unsigned version=banban_usb_audio_version((const uint8_t *)cfg,cfg->wTotalLength);
        ESP_LOGI(TAG,"音频类版本 bcdADC=0x%04x",version);
        atomic_store(&compatibility[e->new_dev.address],version==0x0100?1:2);
        if(version!=0x0100) ESP_LOGE(TAG,"此设备不兼容当前UAC1驱动，保持静音等待；不会调用有缺陷的失败清理路径");
    }
    usb_host_device_close(observer, d);
}
static void observer_task(void *arg)
{
    for (;;) usb_host_client_handle_events(observer, portMAX_DELAY);
}
static void host_task(void *arg)
{
    for (;;) { uint32_t events; usb_host_lib_handle_events(portMAX_DELAY, &events); }
}
static bool has_rate(const uac_host_dev_alt_param_t *a, uint32_t rate)
{
    if (!a->sample_freq_type) return rate >= a->sample_freq_lower && rate <= a->sample_freq_upper;
    for (unsigned i = 0; i < a->sample_freq_type && i < UAC_FREQ_NUM_MAX; ++i)
        if (a->sample_freq[i] == rate) return true;
    return false;
}
static esp_err_t choose(uac_host_device_handle_t h, uac_host_stream_config_t *out, uint8_t *bytes)
{
    uac_host_dev_info_t info;
    esp_err_t err = uac_host_get_device_info(h, &info);
    if (err != ESP_OK) return err;
    ESP_LOGI(TAG, "UAC VID=%04x PID=%04x 接口=%u 备选数=%u", info.VID, info.PID, info.iface_num, info.iface_alt_num);
    uac_host_printf_device_param(h);
    const unsigned rates[] = {48000, 44100, 32000, 96000, 16000, 8000};
    // 优先采样率，再优先位宽；只接受已实现打包的立体声 PCM。
    const unsigned bits[] = {16, 24, 32};
    for (unsigned r=0; r<sizeof(rates)/sizeof(*rates); ++r)
        for (unsigned b=0; b<sizeof(bits)/sizeof(*bits); ++b)
            for (unsigned i=1; i<=info.iface_alt_num; ++i) {
                uac_host_dev_alt_param_t a;
                if (uac_host_get_device_alt_param(h, i, &a) != ESP_OK) continue;
                if (a.format != UAC_TYPE_I_PCM || a.channels != 2 || a.bit_resolution != bits[b] ||
                    a.subframe_size != bits[b]/8 || !has_rate(&a,rates[r])) continue;
                *out = (uac_host_stream_config_t){.channels=2,.bit_resolution=bits[b],.sample_freq=rates[r],
                    .flags=FLAG_STREAM_SUSPEND_AFTER_START};
                *bytes = a.subframe_size;
                return ESP_OK;
            }
    ESP_LOGE(TAG, "没有支持的立体声 PCM 格式（支持 8/16/32/44.1/48/96kHz、16/24/32bit 紧密打包）");
    return ESP_ERR_NOT_SUPPORTED;
}
static void writer_task(void *arg)
{
    // 10ms 块上限：96kHz，驱动自己分配 USB DMA 缓冲。
    int16_t *pcm = heap_caps_malloc(960*2*sizeof(int16_t), MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    uint8_t *packed = heap_caps_malloc(960*2*4, MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if (!pcm || !packed) { ESP_LOGE(TAG, "输出缓冲分配失败"); free(pcm); free(packed); vTaskDelete(NULL); return; }
    uac_host_device_handle_t h = NULL, gone;
    uac_host_stream_config_t format = {0};
    uint8_t bytes = 0;
    uint32_t writes=0, failures=0, reconnects=0, render_peak_us=0;
    int64_t report=esp_timer_get_time();
    for (;;) {
        if (xQueueReceive(disconnected, &gone, 0) == pdTRUE && gone == h) {
            // 唯一所有者先停止使用句柄，再释放；回调不关闭句柄。
            esp_err_t err = uac_host_device_close(h);
            ESP_LOGI(TAG, "USB 拔出，关闭=%s", esp_err_to_name(err)); h=NULL;
        }
        if (!h) {
            connection_t c;
            if (xQueueReceive(connections, &c, pdMS_TO_TICKS(100)) != pdTRUE) continue;
            unsigned checked=0;
            while(!atomic_load(&compatibility[c.addr])&&checked++<50) vTaskDelay(pdMS_TO_TICKS(10));
            if(atomic_load(&compatibility[c.addr])!=1) {
                ESP_LOGW(TAG,"跳过不支持的USB音频接口 addr=%u iface=%u",c.addr,c.iface);continue;
            }
            uac_host_device_config_t dc={.addr=c.addr,.iface_num=c.iface,.buffer_size=8192,
                .buffer_threshold=2048,.callback=device_event};
            esp_err_t err=uac_host_device_open(&dc,&h);
            if (err != ESP_OK) { h=NULL; ESP_LOGW(TAG,"打开失败=%s",esp_err_to_name(err)); continue; }
            err=choose(h,&format,&bytes);
            if (err==ESP_OK) err=uac_host_device_start(h,&format);
            if (err != ESP_OK) { ESP_LOGE(TAG,"流准备失败=%s",esp_err_to_name(err)); uac_host_device_close(h); h=NULL; continue; }
            esp_err_t vol=uac_host_device_set_volume(h,25), mute=uac_host_device_set_mute(h,false);
            ESP_LOGI(TAG,"设备音量25=%s，取消静音=%s（始终另有软件低增益）",esp_err_to_name(vol),esp_err_to_name(mute));
            err=uac_host_device_resume(h);
            if (err!=ESP_OK) { ESP_LOGE(TAG,"流启动失败=%s",esp_err_to_name(err)); uac_host_device_stop(h); uac_host_device_close(h); h=NULL; continue; }
            ++reconnects;
            ESP_LOGI(TAG,"输出=%"PRIu32"Hz/%ubit/2ch，每帧%ubytes",format.sample_freq,format.bit_resolution,2*bytes);
        }
        size_t frames=format.sample_freq/100; // 有界 10ms，44.1kHz 为441帧。
        int64_t begin=esp_timer_get_time();
        render_pcm(pcm,frames,format.sample_freq,render_context);
        uint32_t elapsed=esp_timer_get_time()-begin;
        if (elapsed>render_peak_us) render_peak_us=elapsed;
        for (size_t i=0;i<frames*2;++i) {
            // PCM16 左对齐到目标位宽，按小端序输出；避免负整数左移未定义行为。
            uint32_t v=(uint32_t)((int32_t)pcm[i]*(bytes==2?1:bytes==3?256:65536));
            for (unsigned j=0;j<bytes;++j) packed[i*bytes+j]=(uint8_t)(v>>(8*j));
        }
        esp_err_t err=uac_host_device_write(h,packed,frames*2*bytes,pdMS_TO_TICKS(25));
        if (err==ESP_OK) ++writes;
        else { ++failures; vTaskDelay(pdMS_TO_TICKS(10)); }
        if (esp_timer_get_time()-report>=5000000) {
            ESP_LOGI(TAG,"写入=%"PRIu32" 失败=%"PRIu32" 传输错误=%u 事件丢失=%u 连接=%"PRIu32" render峰值=%"PRIu32"us heap=%u 栈余=%u",
                     writes,failures,atomic_load(&transfer_errors),atomic_load(&event_drops),reconnects,render_peak_us,
                     (unsigned)esp_get_free_heap_size(),(unsigned)uxTaskGetStackHighWaterMark(NULL));
            report=esp_timer_get_time();
        }
    }
}
esp_err_t usb_audio_out_start(banban_render_fn render, void *ctx)
{
    if (!render) return ESP_ERR_INVALID_ARG;
    render_pcm=render; render_context=ctx;
    connections=xQueueCreate(16,sizeof(connection_t)); disconnected=xQueueCreate(1,sizeof(uac_host_device_handle_t));
    if (!connections || !disconnected) return ESP_ERR_NO_MEM;
    usb_host_config_t hc={.skip_phy_setup=false,.intr_flags=ESP_INTR_FLAG_LEVEL1,.fsls_only=false};
    esp_err_t e=usb_host_install(&hc); if (e!=ESP_OK) return e;
    if (xTaskCreate(host_task,"usb_events",4096,NULL,7,NULL)!=pdPASS) return ESP_ERR_NO_MEM;
    usb_host_client_config_t oc={.is_synchronous=false,.max_num_event_msg=10,
        .async={.client_event_callback=observe}};
    e=usb_host_client_register(&oc,&observer); if (e!=ESP_OK) return e;
    if (xTaskCreate(observer_task,"usb_desc",4096,NULL,5,NULL)!=pdPASS) return ESP_ERR_NO_MEM;
    uac_host_driver_config_t uc={.create_background_task=true,.task_priority=7,.stack_size=4096,
        .core_id=0,.callback=driver_event};
    e=uac_host_install(&uc); if(e!=ESP_OK) return e;
    if (xTaskCreate(writer_task,"usb_pcm",6144,NULL,6,NULL)!=pdPASS) return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG,"USB Host/UAC 已安装，等待 USB-A 上的数据设备");
    return ESP_OK;
}

esp_err_t usb_audio_out_set_format(uint32_t rate,unsigned bits)
{ (void)rate;(void)bits;return ESP_ERR_NOT_SUPPORTED; }
