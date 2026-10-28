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
#include "usb/uac2_host.h"
#include "usb_audio_out.h"
#include "usb_descriptor.h"

static const char *TAG = "usb_uac2";
typedef struct { uint8_t addr, iface; } connection_t;
static QueueHandle_t connections, disconnected;
static atomic_uint transfer_errors, event_drops;
static atomic_bool stream_failed,reconfigure;
static atomic_uint requested_rate=48000,requested_bits=32;
esp_err_t usb_audio_out_set_format(uint32_t rate,unsigned bits)
{
    if((bits!=16&&bits!=24&&bits!=32)||(rate!=48000&&rate!=96000&&rate!=44100&&rate!=32000&&rate!=16000&&rate!=8000))return ESP_ERR_INVALID_ARG;
    atomic_store(&requested_rate,rate);atomic_store(&requested_bits,bits);atomic_store(&reconfigure,true);return ESP_OK;
}
static banban_render_fn render_pcm;
static void *render_context;
static usb_host_client_handle_t observer;
// 0=尚未审查，1=UAC2可尝试，2=当前后端不支持。
static atomic_uchar compatibility[128];

static void device_event(uac2_host_device_handle_t h, uac2_host_device_event_t e, void *arg)
{
    if (e == UAC2_HOST_DEVICE_EVENT_DISCONNECTED) xQueueOverwrite(disconnected, &h);
    else if (e == UAC2_HOST_DEVICE_EVENT_STREAM_ERROR) atomic_store(&stream_failed,true);
    else if (e == UAC2_HOST_DEVICE_EVENT_TRANSFER_ERROR) atomic_fetch_add(&transfer_errors, 1);
}
static void driver_event(uint8_t addr, uint8_t iface, uac2_host_driver_event_t e, void *arg)
{
    if (e != UAC2_HOST_DRIVER_EVENT_TX_CONNECTED) return;
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
        atomic_store(&compatibility[e->new_dev.address],version==0x0200?1:2);
        if(version!=0x0200) ESP_LOGE(TAG,"此设备不兼容当前UAC2后端，保持静音等待");
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
static bool has_rate(const uac2_sample_rate_range_t *ranges,unsigned count,uint32_t rate)
{
    for(unsigned i=0;i<count;++i)
        if(rate>=ranges[i].min&&rate<=ranges[i].max&&(!ranges[i].res||(rate-ranges[i].min)%ranges[i].res==0)) return true;
    return false;
}
static esp_err_t choose(uac2_host_device_handle_t h,uint8_t iface,unsigned rate,unsigned bit_depth,uac2_host_stream_config_t *out,uint8_t *bytes)
{
    uac2_device_info_t info;
    esp_err_t err=uac2_host_device_get_info(h,&info);if(err!=ESP_OK)return err;
    ESP_LOGI(TAG,"UAC2 VID=%04x PID=%04x 时钟源数=%u 备选数=%u",info.vid,info.pid,info.num_clock_sources,info.num_as_ifaces);
    uac2_sample_rate_range_t ranges[UAC2_MAX_SAMPLE_RATE_RANGES];uint8_t count=0;
    err=uac2_host_device_get_sample_rate_range(h,ranges,&count);
    if(err!=ESP_OK) { ESP_LOGE(TAG,"读取时钟采样率范围失败=%s",esp_err_to_name(err));return err; }
    for(unsigned i=0;i<count;++i) ESP_LOGI(TAG,"真实采样率范围=%"PRIu32"..%"PRIu32" step=%"PRIu32,ranges[i].min,ranges[i].max,ranges[i].res);
    // 当前可复用驱动以1ms为包周期；48k优先，无显式feedback的44.1k格式明确交由驱动拒绝。
    const unsigned rates[]={rate};
    const unsigned bits[]={bit_depth};
    for(unsigned r=0;r<sizeof(rates)/sizeof(*rates);++r) {
        if(!has_rate(ranges,count,rates[r]))continue;
        for(unsigned b=0;b<sizeof(bits)/sizeof(*bits);++b)
            for(unsigned i=0;i<info.num_as_ifaces;++i) {
                const uac2_as_iface_t *a=&info.as_ifaces[i];
                if(a->interface_num!=iface||(a->ep_addr&0x80)||a->nr_channels!=2||a->bit_resolution!=bits[b]||a->sub_slot_size!=bits[b]/8)continue;
                if(((rates[r]+999)/1000)*2*a->sub_slot_size>a->ep_max_packet_size)continue;
                *out=(uac2_host_stream_config_t){.channels=2,.bit_resolution=bits[b],.sample_freq=rates[r],.flags=UAC2_FLAG_STREAM_SUSPEND_AFTER_START};
                *bytes=a->sub_slot_size;
                ESP_LOGI(TAG,"选择播放接口=%u alt=%u PCM=%uHz/%ubit/2ch endpoint=0x%02x",iface,a->alt_setting,rates[r],bits[b],a->ep_addr);
                return ESP_OK;
            }
    }
    return ESP_ERR_NOT_SUPPORTED;
}
// 实物06cb:1595在冷启动直接打开32bit时有总线数据却无模拟声；用户确认
// 16→32切换后可闻。只针对这组VID/PID和已验证格式，其他设备不套用。
// 先发送250ms的16bit静音，再完整关闭/重开，复现串口手动切换的资源顺序。
static esp_err_t prepare_cx31993(uac2_host_device_handle_t *h,
                                const uac2_host_device_config_t *dc,uint8_t iface,
                                unsigned rate,unsigned bits,uint8_t *silence)
{
    uac2_device_info_t info;
    esp_err_t err=uac2_host_device_get_info(*h,&info);
    if(err!=ESP_OK)return err;
    if(info.vid!=0x06cb||info.pid!=0x1595||rate!=48000||bits!=32)return ESP_OK;
    ESP_LOGI(TAG,"06cb:1595兼容初始化：48k/16bit静音→关闭→48k/32bit");
    uac2_host_stream_config_t warm={0};uint8_t slot=0;
    err=choose(*h,iface,48000,16,&warm,&slot);
    if(err==ESP_OK)err=uac2_host_device_start(*h,&warm);
    if(err==ESP_OK)err=uac2_host_device_set_volume_all_channels(*h,-12*256);
    if(err==ESP_OK)err=uac2_host_device_set_mute(*h,0,false);
    if(err==ESP_OK)err=uac2_host_device_resume(*h);
    memset(silence,0,480*2*2);
    for(unsigned i=0;i<25&&err==ESP_OK;++i) {
        err=uac2_host_device_write(*h,silence,480*2*2,25);
        // 只发送静音，无需抢占式填满驱动缓冲；留出USB事件与按键任务调度。
        if(err==ESP_OK)vTaskDelay(pdMS_TO_TICKS(10));
    }
    // 排空末尾静音后关闭。失败同样交还到关闭路径，不保留半初始化句柄。
    if(err==ESP_OK)vTaskDelay(pdMS_TO_TICKS(30));
    esp_err_t closed=uac2_host_device_close(*h);
    *h=NULL;
    if(err!=ESP_OK)return err;
    if(closed!=ESP_OK)return closed;
    return uac2_host_device_open(dc,h);
}
static void writer_task(void *arg)
{
    // 10ms 块上限：96kHz，驱动自己分配 USB DMA 缓冲。
    int16_t *pcm = heap_caps_malloc(960*2*sizeof(int16_t), MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    uint8_t *packed = heap_caps_malloc(960*2*4, MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if (!pcm || !packed) { ESP_LOGE(TAG, "输出缓冲分配失败"); free(pcm); free(packed); vTaskDelete(NULL); return; }
    uac2_host_device_handle_t h = NULL, gone;
    uac2_host_stream_config_t format = {0};
    connection_t last_connection={0};
    uint8_t bytes = 0;
    uint32_t writes=0, failures=0, reconnects=0, render_peak_us=0;
    int64_t report=esp_timer_get_time();
    uint32_t pcm_peak=0;
    for (;;) {
        if (xQueueReceive(disconnected, &gone, 0) == pdTRUE && gone == h) {
            // 唯一所有者先停止使用句柄，再释放；回调不关闭句柄。
            esp_err_t err = uac2_host_device_close(h);
            ESP_LOGI(TAG, "USB 拔出，关闭=%s", esp_err_to_name(err)); h=NULL;
        }
        if(atomic_exchange(&reconfigure,false)&&last_connection.addr) {
            ESP_LOGI(TAG,"串口请求重配=%uHz/%ubit",atomic_load(&requested_rate),atomic_load(&requested_bits));
            if(h) {uac2_host_device_close(h);h=NULL;}
            xQueueSendToFront(connections,&last_connection,0);
        }
        if(h&&atomic_exchange(&stream_failed,false)) {
            ESP_LOGE(TAG,"驱动报告持续传输错误，关闭流并等待重新插入");
            uac2_host_device_close(h);h=NULL;
        }
        if (!h) {
            connection_t c;
            if (xQueueReceive(connections, &c, pdMS_TO_TICKS(100)) != pdTRUE) continue;
            unsigned checked=0;
            while(!atomic_load(&compatibility[c.addr])&&checked++<50) vTaskDelay(pdMS_TO_TICKS(10));
            if(atomic_load(&compatibility[c.addr])!=1) {
                ESP_LOGW(TAG,"跳过不支持的USB音频接口 addr=%u iface=%u",c.addr,c.iface);continue;
            }
            uac2_host_device_config_t dc={.addr=c.addr,.iface_num=c.iface,.buffer_size=8192,
                .buffer_threshold=2048,.callback=device_event};
            last_connection=c;atomic_store(&reconfigure,false);
            atomic_store(&stream_failed,false);
            esp_err_t err=uac2_host_device_open(&dc,&h);
            if (err != ESP_OK) { h=NULL; ESP_LOGW(TAG,"打开失败=%s",esp_err_to_name(err)); continue; }
            unsigned rate=atomic_load(&requested_rate),bits=atomic_load(&requested_bits);
            err=prepare_cx31993(&h,&dc,c.iface,rate,bits,packed);
            if(err==ESP_OK)err=choose(h,c.iface,rate,bits,&format,&bytes);
            if (err==ESP_OK) err=uac2_host_device_start(h,&format);
            if (err != ESP_OK) { ESP_LOGE(TAG,"流准备失败=%s",esp_err_to_name(err)); if(h)uac2_host_device_close(h); h=NULL; continue; }
            esp_err_t vol=uac2_host_device_set_volume_all_channels(h,-12*256), mute=uac2_host_device_set_mute(h,0,false);
            ESP_LOGI(TAG,"设备音量-12dB=%s，取消静音=%s（始终另有软件低增益）",esp_err_to_name(vol),esp_err_to_name(mute));
            err=uac2_host_device_resume(h);
            if (err!=ESP_OK) { ESP_LOGE(TAG,"流启动失败=%s",esp_err_to_name(err)); uac2_host_device_stop(h); uac2_host_device_close(h); h=NULL; continue; }
            uint32_t actual=0;bool valid=false;
            esp_err_t clock_err=uac2_host_device_get_sample_rate(h,&actual);
            esp_err_t valid_err=uac2_host_device_get_clock_valid(h,&valid);
            ESP_LOGI(TAG,"时钟读回=%"PRIu32"Hz (%s) valid=%d (%s)",actual,esp_err_to_name(clock_err),valid,esp_err_to_name(valid_err));
            bool muted=true;int16_t lvol=0,rvol=0;
            esp_err_t me=uac2_host_device_get_mute(h,0,&muted);
            esp_err_t le=uac2_host_device_get_volume(h,1,&lvol),re=uac2_host_device_get_volume(h,2,&rvol);
            ESP_LOGI(TAG,"控制读回：mute=%d (%s) 左=%d/256dB (%s) 右=%d/256dB (%s)",muted,esp_err_to_name(me),lvol,esp_err_to_name(le),rvol,esp_err_to_name(re));
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
            uint32_t peak=pcm[i]<0?-(int32_t)pcm[i]:pcm[i];if(peak>pcm_peak)pcm_peak=peak;
            uint32_t v=(uint32_t)((int32_t)pcm[i]*(bytes==2?1:bytes==3?256:65536));
            for (unsigned j=0;j<bytes;++j) packed[i*bytes+j]=(uint8_t)(v>>(8*j));
        }
        esp_err_t err=uac2_host_device_write(h,packed,frames*2*bytes,25);
        if (err==ESP_OK) ++writes;
        else { ++failures; vTaskDelay(pdMS_TO_TICKS(10)); }
        if (esp_timer_get_time()-report>=5000000) {
            ESP_LOGI(TAG,"写入=%"PRIu32" 失败=%"PRIu32" 传输错误=%u 事件丢失=%u 连接=%"PRIu32" render峰值=%"PRIu32"us heap=%u 栈余=%u",
                     writes,failures,atomic_load(&transfer_errors),atomic_load(&event_drops),reconnects,render_peak_us,
                     (unsigned)esp_get_free_heap_size(),(unsigned)uxTaskGetStackHighWaterMark(NULL));
            uac2_host_tx_stats_t tx={0};
            if(uac2_host_device_get_tx_stats(h,&tx)==ESP_OK)
                ESP_LOGI(TAG,"PCM峰值=%"PRIu32" USB完成包=%"PRIu32" 实际bytes=%"PRIu32" 非零bytes=%"PRIu32" 补零bytes=%"PRIu32" 跳过包=%"PRIu32,
                    pcm_peak,tx.completed_packets,tx.completed_bytes,tx.nonzero_payload_bytes,tx.zero_padding_bytes,tx.skipped_packets);
            pcm_peak=0;
            report=esp_timer_get_time();
        }
    }
}
esp_err_t usb_audio_out_start(banban_render_fn render, void *ctx)
{
    if (!render) return ESP_ERR_INVALID_ARG;
    render_pcm=render; render_context=ctx;
    connections=xQueueCreate(16,sizeof(connection_t)); disconnected=xQueueCreate(1,sizeof(uac2_host_device_handle_t));
    if (!connections || !disconnected) return ESP_ERR_NO_MEM;
    // S31原生HS控制器以全速运行；为DAC最大768B端点保留1024B周期TX FIFO。
    // RX与控制OUT分别保留520B/256B，均在S31控制器FIFO总量内，Host会做容量校验。
    usb_host_config_t hc={.skip_phy_setup=false,.intr_flags=ESP_INTR_FLAG_LEVEL1,.fsls_only=true,
        .fifo_settings_custom={.rx_fifo_lines=130,.nptx_fifo_lines=64,.ptx_fifo_lines=256}};
    esp_err_t e=usb_host_install(&hc); if (e!=ESP_OK) return e;
    if (xTaskCreate(host_task,"usb_events",4096,NULL,7,NULL)!=pdPASS) return ESP_ERR_NO_MEM;
    usb_host_client_config_t oc={.is_synchronous=false,.max_num_event_msg=10,
        .async={.client_event_callback=observe}};
    e=usb_host_client_register(&oc,&observer); if (e!=ESP_OK) return e;
    if (xTaskCreate(observer_task,"usb_desc",4096,NULL,5,NULL)!=pdPASS) return ESP_ERR_NO_MEM;
    uac2_host_driver_config_t uc={.create_background_task=true,.task_priority=7,.stack_size=4096,
        .core_id=0,.callback=driver_event};
    e=uac2_host_install(&uc); if(e!=ESP_OK) return e;
    if (xTaskCreate(writer_task,"usb_pcm",6144,NULL,6,NULL)!=pdPASS) return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG,"USB Host/UAC2 已安装（全速），等待 USB-A 上的数据设备");
    return ESP_OK;
}
