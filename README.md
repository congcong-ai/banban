# 伴伴 BanBan · 本地蓝牙消原唱硬件

伴伴（BanBan）是基于 **ESP32-S31-Korvo-1 V1.1 + CX31993 USB DAC** 的开源音频工程：接收手机蓝牙音乐，在开发板本地削弱立体声中置人声，再经 USB DAC 输出到耳机或有源音响。无需账号、云服务或模型推理。

自有代码采用 [MIT](LICENSE)，第三方代码保留原许可。项目处于硬件原型阶段，核心链路已有真板记录，长期稳定性仍待验证。

## 一行刷回已保存固件

**本机已有归档、连接原开发板时，在仓库根目录执行：**

```bash
./VocalCutBox/scripts/restore_firmware.sh
```

在任意目录快速恢复（维护者本机路径）：

```bash
/Users/peter/code/congcongai/banban/VocalCutBox/scripts/restore_firmware.sh
```

只检查归档、不连接开发板：

```bash
./VocalCutBox/scripts/restore_firmware.sh --check
```

无需重新编译。脚本核对归档SHA256、USB串口序列号、芯片和MAC后写入，再逐段校验Flash。连接 **USB-C UART** 数据线并关闭串口监视器。

**新克隆仓库不包含归档，不能直接运行恢复。** `VocalCutBox/artifacts/firmware/` 含原板身份与二进制，仅本机保存。换板请按下方源码构建与烧录流程操作。[归档位置、环境要求与真实恢复记录](VocalCutBox/docs/11_FIRMWARE_BACKUP_RESTORE.md)。

## 伴伴能做什么

- 手机、电脑或支持A2DP音频输出的电视，通过系统蓝牙连接 `VocalCutBox`，无需开发板屏幕。
- 本地DSP提供原曲直通、基础消除（BASIC_CUT）和伴奏保留（MUSIC_PRESERVE）模式。
- 四个实体按键切换模式、调节消除强度，手机独立调节播放响度。
- USB Host输出到已验证的CX31993，当前播放格式为48kHz、32bit、双声道；内部PCM/DSP为16bit，输出扩展不增加有效精度。

这里的“消原唱”是立体声中置声像削弱，不是AI人声分离。居中的鼓、贝斯和乐器也可能被削弱；单声道、混响和偏声像人声的效果有限。目前没有可承诺的通用人声消除比例或实测端到端延迟。

## 硬件与接线

| 硬件 | 用途 |
| --- | --- |
| ESP32-S31-Korvo-1 V1.1 | 蓝牙输入、本地DSP、USB Host与实体按键 |
| CX31993 USB DAC（已测06cb:1595） | USB音频转3.5mm模拟输出 |
| USB-A转USB-C数据转接头 | 连接开发板Host与DAC |
| 耳机或有源音响 | 接DAC模拟输出；D100整链路尚待验收 |

```text
手机/电脑 → A2DP蓝牙 → ESP32-S31 → 本地DSP
         → USB-A Host → 数据转接头 → CX31993 → 3.5mm → 耳机/有源音响
```

USB-C UART用于烧录调试，USB-C Power用于供电，DAC接 **USB-A Host**。板载扬声器功放输出不能接音响RCA输入。[详细接线](VocalCutBox/docs/02_HARDWARE_CONNECTIONS.md)。

## 快速开始：从源码构建

需要Git、ESP-IDF及其Python环境；主机回归另需C编译器与Python 3。当前固定预览SDK提交为 `ab95e70748f2eddac6f87217b0cf62533684a6ba`，目标 `esp32s31`，不能直接换用普通ESP32-S3。

```bash
git clone https://github.com/congcong-ai/banban.git
cd banban/VocalCutBox

# 先按构建文档安装固定SDK并设置 BANBAN_IDF_PATH / BANBAN_PYTHON_ENV
bash scripts/build.sh              # USB测试音诊断固件，启动静音
bash scripts/build.sh --bt         # 蓝牙输入与四键消除固件
```

输出分别为 `build/banban.bin` 和 `build-bt/banban.bin`。SDK、自动下载组件和控制器二进制不提交到仓库。依赖版本见 `dependencies.lock`。

[SDK安装、构建与烧录](VocalCutBox/docs/03_BUILD_FLASH_TEST.md)包含端口、USB序列号与MAC核验步骤；烧录脚本默认目标及 `--build-dir` 参数请先查看 `bash scripts/flash_monitor.sh --help`，蓝牙镜像需明确选择 `build-bt`。不要把默认诊断固件误当蓝牙版本。

## 蓝牙连接与四键操作

刷入蓝牙版本后，在系统蓝牙设置选择 **VocalCutBox**。使用SSP Just Works，系统可能要求一次配对确认；连接后停止发现，断开后重新可发现。电视必须支持A2DP音频输出。

| 操作 | 功能 |
| --- | --- |
| PLAY短按 | 原曲 ↔ 上次消除模式；首次切换为基础消除 |
| SET短按 | BASIC_CUT ↔ MUSIC_PRESERVE；原曲时选择下次模式 |
| VOL+ / VOL-短按 | 消除强度±5，范围0～100 |
| VOL+ / VOL-长按 | 连续调节消除强度 |

VOL±控制消除强度，播放响度由音源和串口 `volume 0..50` 独立控制。上电回到原曲，默认强度100，不保存设置。板载喇叭保持关闭。[连接说明](VocalCutBox/docs/09_BLUETOOTH_CONNECTION_FLOW.md) · [按键说明](VocalCutBox/docs/10_BUTTON_CONTROLS.md)。

串口调试可用 `source bt`、`mode bypass/basic/preserve`、`strength 0..100`、`volume 0..50`。USB诊断版用 `tone on` / `tone off` 播放或停止左右测试音；蓝牙版的 `tone off` 不会停止蓝牙音乐。

## 当前验收状态

| 验收项 | 状态 | 证据 | 风险/边界 |
| --- | --- | --- | --- |
| ESP32-S31目标编译 | 通过 | 默认P1与BT实验版构建 | 固定IDF开发版 |
| CX31993枚举/UAC2控制 | 通过 | 06cb:1595、422字节描述符、真实时钟范围及48k读回 | 仅全速播放，未开放通用多时钟/双工 |
| USB持续PCM发送 | 通过 | 超过5分钟，写入/传输错误0、heap稳定 | 不等于已验证模拟输出 |
| 耳机左右实际播放 | 通过 | 用户确认48kHz/32bit模式有声、左右正确 | 直接32bit冷启动曾无声；已增加16→32兼容初始化并通过重插试听 |
| D100有源音响播放 | 待实机 | 设计为DAC 3.5mm→已核验TRS/RCA | 用户已确认TRS/RCA线，实际音箱输出未验收 |
| 手机A2DP配对播放 | 部分通过 | 用户确认修复版音乐基本正常 | 曾有轻微电流声，恢复音量后未再听到，需复测 |
| 蓝牙→USB直通 | 部分通过 | 用户确认音乐可闻，44100→48000Hz实际运行 | 噪声需复测，SRC品质与时钟漂移待完善 |
| DSP Bypass/Basic/Preserve | 离线通过 | ASan/UBSan与30组合成指标 | 未试听真实歌曲 |
| 实体四键 | 功能实测通过 | 四键事件、PLAY实际A/B、强度增减与VOL-长按 | ADC/PHY同核规避后超过10分钟未重启，60分钟未验收 |
| LCD | 未完成 | 核心音频不依赖UI | 暂不启用 |
| USB热插拔恢复 | 待实机 | 单所有者关闭/重开实现 | 一次拔出5秒重插后用户确认有声；需继续重复测试 |
| 60分钟稳定性 | 未完成 | 尚未运行 | 不作稳定性承诺 |

## 测试与贡献

从 `VocalCutBox/` 执行主机测试，产物放临时目录，不连接或烧录开发板：

```bash
bash scripts/test_uac2_host.sh /tmp/banban-uac2-tests
bash scripts/test_usb_host.sh /tmp/banban-usb-tests
bash scripts/test_host.sh /tmp/banban-dsp-tests
```

测试覆盖实际描述符解析与本地DSP，不能替代真板试听。欢迎改进采样率转换、时钟漂移、热插拔恢复与稳定性；提交时附复现条件、测试命令和实际输出。[贡献指南](CONTRIBUTING.md) · [问题与后续工作](VocalCutBox/docs/06_KNOWN_ISSUES_NEXT_STEPS.md)。

## 文档与常见问题

- [常见问题：设备兼容、消原唱原理与离线使用](docs/FAQ.md)
- [技术决策](VocalCutBox/docs/01_TECH_DECISIONS.md) · [DSP算法与限制](VocalCutBox/docs/04_DSP_ALGORITHMS_AND_LIMITS.md)
- [真板测试记录](VocalCutBox/docs/05_REAL_DEVICE_TEST_LOG.md) · [UAC2移植](VocalCutBox/docs/07_UAC2_HOST_SUPPORT.md)
- [第三方来源与许可](VocalCutBox/THIRD_PARTY_LICENSES.md) · [机器可读项目索引](llms.txt)

## 开源协议与致谢

伴伴自有代码使用 [MIT许可证](LICENSE)。含Espressif源码节选的文件继续采用Apache-2.0，`vendor/uac2_host`保留MIT及其引用部分的原许可与署名。感谢 [Espressif ESP-IDF](https://github.com/espressif/esp-idf) 和 [Averyy/esp-uac2-host](https://github.com/Averyy/esp-uac2-host)。分发时请保留[第三方许可与NOTICE](VocalCutBox/THIRD_PARTY_LICENSES.md)。蓝牙预览库完整再分发条款尚待核实，本仓库不提供公共固件二进制。
