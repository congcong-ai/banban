# 第三方来源与许可核查

自有代码采用MIT；LICENSE保留许可证的英文法律原文。第三方代码继续适用各自许可，Apache-2.0全文保存在LICENSES/Apache-2.0.txt。没有复制SmartBadge或HomeStash业务代码，没有引入模型、音乐素材或字体。

| 依赖 | 版本/来源 | 已检查的许可 | 再分发策略 |
| --- | --- | --- | --- |
| ESP-IDF | ab95e70748f2eddac6f87217b0cf62533684a6ba | 根LICENSE Apache-2.0；单独组件以文件头与各自许可为准 | SDK不进入本仓库；复现时固定提交安装，发布固件须携带依赖NOTICE |
| usb_host_uac | 1.5.0，esp-usb提交6d24137e14a4f6c8138662a7a074be16a899c2c6 | 下载包LICENSE与源码头Apache-2.0 | 组件管理器下载，提交版本锁；不修改驱动 |
| usb | 1.6.0，组件管理器锁文件 | 下载包LICENSE Apache-2.0 | 同上 |
| cmake_utilities | 0.5.3 | 下载包LICENSE Apache-2.0 | 仅构建工具 |
| S31控制器库 | 官方esp32s31-bt-lib，cc027ecd288b31198f82331faabb972a37d6800e | 包内README/NOTICE说明包含Apache Mynewt NimBLE来源；归档未附独立LICENSE正文 | 只从官方固定归档拉取到忽略目录，不将.a库上传仓库；公开发布含蓝牙二进制前，继续确认该预览库的完整再分发条款并携带NOTICE |
| 官方A2DP示例 | 本机相同IDF提交 | main/辅助模块文件头Unlicense或CC0-1.0；API头Apache-2.0 | 阅读调用模式，自行实现适合本项目的模块，未整份搬运例程 |
| Korvo官方BSP参考 | esp-dev-kits 6f4b0f6ef304d76246325e9a9f94c1ddad8ed39c | 已读取vendor根Apache-2.0许可证 | P1不引入；按键阶段再择最小官方封装 |

本仓库发布源码，不包含本机固件归档、SDK或蓝牙控制器二进制。后续若引入新依赖，需补充其许可和NOTICE，不能据本仓库MIT将所有第三方内容一概重新授权。

## 本次新增：可复用UAC2 Host

采用 [Averyy/esp-uac2-host](https://github.com/Averyy/esp-uac2-host) 0.1.2，固定提交 `a072adb6a2410f9f93474637a483724c93d5fc6f`。已读取组件LICENSE（MIT）、公开API、驱动和解析器源码；允许在保留版权与许可声明的条件下复制、修改和再分发。源码按固定版本保存在vendor/uac2_host，原始哈希、许可证、NOTICE和本地PATCHES完整保留，不需要运行时访问GitHub。

这是第三方开源组件，不能称为乐鑫官方UAC2实现，也不能把上游S3/miniDSP测试当作本项目S31/CX31993测试。S31移植使用官方USB Host API；以全速运行，拒绝高速设备进入按1ms调度的路径。描述符定义上游说明参考USBX（MIT）和CherryUSB（Apache-2.0），对应署名在vendor NOTICE保留。未引入限制再分发的模型或音乐。

板载对照新增 `espressif/esp_codec_dev ==1.6.2`（Apache-2.0，已读取本机实际组件manifest与许可证），并从只读参考中的官方S31 BSP音频段提取最小初始化封装。源文件保留Espressif版权、SPDX与固定出处；未复制SmartBadge业务代码、凭证、录音或完整BSP。


实体按键新增 `espressif/button ==4.2.1`，与只读参考的已锁定版本一致；已读取实际包`license.txt`（Apache-2.0）和接口源码。S31 ADC校准C文件与私有头从同一Espressif/Apache-2.0声明的参考BSP原样节选，保留版权，来源、范围与文件SHA256见`docs/10_BUTTON_CONTROLS.md`。没有引入完整BSP、LCD或摄像头依赖。

## 源码许可边界与分发清单

- 自有实现中原有单行SPDX声明改为MIT；未将带Espressif版权的文件或vendor组件改为MIT。
- `components/board_audio_out/board_audio_out.c`、`components/controls/controls.c`、`components/controls/esp32_s31_adc_calibration.c`、`components/controls/bsp_adc_calibration.h` 含官方BSP节选，继续采用Apache-2.0，保留文件头与来源。伴伴对这些文件的本地修改包括板载音频最小封装、四键事件映射与ADC采样任务核绑定；详见按键文档和Git历史。
- `vendor/uac2_host/` 保留上游MIT LICENSE、NOTICE、来源与PATCHES；其描述符参考的Apache-2.0部分继续保留原声明，不能整体视为MIT。
- 分发源码时一并保留本文件、根LICENSE、LICENSES目录、vendor的LICENSE/NOTICE/PATCHES及源码版权头。
- 分发编译固件时，应另附实际链接依赖的许可证和NOTICE（包括ESP-IDF各子组件及蓝牙控制器）；当前清单不代表所有SDK子组件统一采用Apache-2.0。蓝牙预览库完整再分发条款尚待核实，当前不提供公共固件下载。

许可依据：[MIT原文](https://opensource.org/license/mit)、[ESP-IDF许可说明](https://github.com/espressif/esp-idf/blob/ab95e70748f2eddac6f87217b0cf62533684a6ba/LICENSE)、[UAC2固定提交许可](https://github.com/Averyy/esp-uac2-host/blob/a072adb6a2410f9f93474637a483724c93d5fc6f/LICENSE)。
