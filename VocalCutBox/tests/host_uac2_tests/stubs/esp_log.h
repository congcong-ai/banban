// 主机测试仅替换日志输出，不替换描述符解析实现。
#pragma once
#include <stdio.h>
#include <inttypes.h>
#define ESP_LOGI(tag,...) do { (void)(tag); if(0)fprintf(stderr,__VA_ARGS__); } while(0)
#define ESP_LOGW ESP_LOGI
#define ESP_LOGE ESP_LOGI
#define ESP_LOGD ESP_LOGI
