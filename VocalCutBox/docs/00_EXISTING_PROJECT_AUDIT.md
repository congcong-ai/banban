# 既有工程只读审计

审计日期：2026-10-27。以下 `$HOME` 指本机用户目录，避免在公开仓库写入用户名。未修改两个参考仓库、全局 SDK 或历史固件源码。

## 工程边界与搜索

初始工作目录 `$HOME/code/congcongai/banban` 为空，非 Git 仓库。搜索父工作区及 `$HOME/code` 四层以内，找到：

- SmartBadge：`$HOME/code/congcongai/fde-moregy/SmartBadge`。
- Homestash：`$HOME/code/EmbodiedAI/homestash`（实际全小写）。

两者位于不同父目录，故独立工程落在当前工作区 `banban/VocalCutBox`，独立初始化 Git；没有在参考仓库内开发。SmartBadge 初始有 `scripts/server-deploy.sh` 修改；Homestash 的 README、固件 README、package.json、backup.py、部署脚本等已有修改及 restore.py/restore.sh 新文件，均保留。禁止把这些已有改动带入伴伴提交。

## 实际查阅

SmartBadge 文档：`docs/development-board-firmware-guide.md`、`firmware-flashing-and-recovery.md`、`firmware-playback-and-debugging.md`、`hardware-device-controls.md`、`hardware-getting-started.md`、`hardware-integration.md`、`hardware-acceptance.md`、`ESP32-S31-Korvo-1/README.md`，以及对应板卡资料索引、依赖锁和硬件验收记录。没有发现 `docs/00-BOARD-QUICKSTART-ZH.md`。

SmartBadge 源码/配置：`firmware/esp32-s31-korvo/{CMakeLists.txt,sdkconfig.defaults,dependencies.lock,main/idf_component.yml,main/main.c}`；`scripts/hardware/build-firmware.sh`、`flash-firmware.py`；`vendor/espressif/esp32-s31-korvo/components/esp32_s31_korvo/` 的板级实现、音频/按键接口与许可证。

Homestash：`firmware/freshmate-korvo/{CMakeLists.txt,sdkconfig.defaults,main/idf_component.yml,README.md}`；`firmware/diagnostics/korvo-provision-usb/` 的 USB Host 任务实现；`firmware/common/components/` 模块布局；`docs/hardware-evidence/ble-provisioning-usb-host-20261020.json`；`scripts/hardware/{backup.py,restore.py,restore.sh}`；固定蓝牙组件的准备记录及控制器库许可证说明。

官方源码：ESP-IDF 本机 `examples/bluetooth/bluedroid/classic_bt/a2dp_sink_stream`、通用内部 SBC 解码辅助模块、`esp_a2dp_api.h`；官方 USB audio_player 主程序；实际下载的 UAC 1.5.0 头文件、实现、CHANGELOG、LICENSE；USB 1.6.0 主机 API。

## 已验证基线与可复用方式

| 来源 | 查得的真实证据 | 本项目采用方式 |
| --- | --- | --- |
| SmartBadge | S31、IDF ab95e707、官方 BSP；文档有烧录校验、串口与录放音证据 | 锁定 SDK、独立构建、芯片/USB身份核对、日志与主机 C 测试 |
| SmartBadge | 按键 GPIO42 ADC，板级校准；官方 BSP 为单体，含 LCD/摄像头依赖 | 不猜阈值；P1不拉入整个BSP，不初始化按键与Codec |
| SmartBadge | UART 打开可能产生复位脉冲；持续会话优于反复打开 | 提供持续串口命令与记录脚本 |
| Homestash | 2026-10-20 独立诊断曾实际枚举USB键盘并收到4次按键 | 复用 Host独立事件任务模式；不把 HID 成功当作 UAC 成功 |
| Homestash | 本机SDK的S31控制器库缺失，项目私有组件补齐同一gitlink版本 | 蓝牙依赖只在本项目私有目录准备；不写全局SDK |
| Homestash | 16MB备份、逐块/整片摘要核验与恢复脚本 | 覆盖前独立复核已有完整备份；不调用参考项目写入/恢复脚本 |

不能复用：云端录音业务、Wi-Fi凭证、SD录音、ES8389 I2S/TDM初始化、相机、LCD和OTA业务。USB DAC是独立音频输出链，不经过板载扬声器功放。

## 本机实查环境

- ESP-IDF：`$HOME/esp/esp-idf-smartbadge`，提交 `ab95e70748f2eddac6f87217b0cf62533684a6ba`，`idf.py --version` 为 v6.2.0，开发版，S31为预览目标。
- Python：`$HOME/.espressif/python_env/idf6.2_py3.12_env`，3.12.11；RISC-V GCC 16.1.0。
- SDK有部分固定子模块归档，并非所有可选子模块完整。构建保留该事实，不更新全局目录。
- 唯一目标USB-UART为CP2102N，`/dev/cu.usbserial-1410`；esptool真实识别ESP32-S31 rev0.0。具体USB序列号和芯片MAC留在私有日志。
- 最小固件与P1 USB固件已编译；已获用户明确授权将现有HomeStash切换为伴伴开发板，P1已烧录并通过写入哈希。
- 没有启动网络服务，无端口占用或Docker需求。

## 接线风险

USB-A才是Host，Power USB-C仅供电，UART USB-C用于开发。USB-A下游最大500mA。CX31993的3.5mm是耳机输出；已有A转C转接头尚需数据枚举证明。TRRS四极不能仅凭外观推断L/R/GND；先耳机，再核验TRS→双RCA。不能连接板载4Ω/3W功放端口到D100。
