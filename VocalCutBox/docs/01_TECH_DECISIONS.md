# 技术决策与版本

## 当前0.2.0：UAC2复用路线

2026-10-27新增MIT许可 `Averyy/esp-uac2-host` 0.1.2固定提交，源码在vendor/uac2_host。S31使用官方USB Host的全速模式，已真实查询采样率、开启48kHz/16bit/双声道流。当前用户反馈耳机无声，因此状态为“USB控制与非零总线传输已验证，模拟输出未通过”，不能称作完整P1。

芯片仍是esp32s31，SDK提交仍为ab95e707；不修改全局SDK或两个参考工程。上游MIT许可、来源、原始SHA256、本地补丁和边界保留在vendor。未自行重写USB控制器驱动。

详见 [UAC2实现与诊断](07_UAC2_HOST_SUPPORT.md)。以下保留0.1阶段的选择及失败原因，属于历史记录。


日期：2026-10-27。

## 固定基线

- ESP-IDF锁定 `ab95e70748f2eddac6f87217b0cf62533684a6ba`（v6.2开发版），目标严格为 `esp32s31`。沿用本机已在两工程使用的SDK，构建脚本检查提交，不修改SDK。
- `espressif/usb_host_uac ==1.5.0`，组件管理器生成并提交 `dependencies.lock`，当前解析USB Host为 `espressif/usb 1.6.0`、cmake_utilities 0.5.3。不能仅凭源码中的宽松范围推断版本。
- P1无BSP依赖：USB-A原生PHY由官方USB Host初始化，未初始化ES8389、LCD、相机、Wi-Fi、SD。
- 项目自有代码MIT（第三方节选保留原许可）；依赖许可见 `THIRD_PARTY_LICENSES.md`。

## USB

采用官方Host与UAC驱动，不写USB类驱动。UAC 1.4.0开始支持S31；1.5.0修复44.1kHz包长分配与subframe尺寸问题。该驱动只支持UAC 1.0；本只CX31993已实测为UAC2.0（06cb:1595），故该驱动无法播放；不能仅将bcdADC检查删掉强行使用。

枚举日志记录VID/PID、完整设备/配置描述符及可用PCM格式。优先48kHz、16bit、双声道；随后尝试44.1/32/96/16/8kHz及24/32bit紧密打包双声道。24bit放在4byte槽、单声道及未列出的采样率目前明确拒绝，避免播放速度或样本打包错误。选择必须匹配设备实际声明能力，不按CX31993型号硬编码。

单一writer任务拥有UAC句柄，连接队列与独立拔出邮箱隔离生命周期。回调不写PCM、不关闭句柄。写入等待上限25ms；无设备时阻塞100ms等待。输出10ms PCM块送入官方8KB环形缓冲；驱动负责USB传输和DMA内存。应用缓冲显式使用内部RAM。5秒汇总写入、传输错误、事件丢失、连接数、渲染峰值、heap及writer栈余量。

P1默认测试音峰值在软件音量10时约-40dBFS，并设置设备音量25；设备若不支持音量/静音请求会明确记录，软件低增益仍生效。左右声道分别440/880Hz，每秒切换左、右、双声道、静音，片段边缘淡变。

## 蓝牙与DSP路线

优先审计IDF官方A2DP Sink内置SBC解码路径，避免第一阶段额外引入GMF调度与媒体播放器。控制器官方gitlink为 `cc027ecd288b31198f82331faabb972a37d6800e`；私有安装脚本从官方固定归档下载并核验SHA256，不修改SDK和参考工程。

USB实机验收先于蓝牙集成验收。蓝牙待接入时需在回调中仅非阻塞入队，工作任务负责格式适配与DSP；不能直接把44.1kHz字节当48kHz发送。

纯C DSP与IDF完全分离；提供Bypass、Basic、Music Preserve，20ms参数平滑。暂不加入STFT、AI模型、网络音源和麦克风。

## 官方来源

- [板卡指南](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html)
- [UAC 1.5.0与许可证](https://components.espressif.com/components/espressif/usb_host_uac/versions/1.5.0/readme)
- [官方USB播放器](https://github.com/espressif/esp-iot-solution/tree/master/examples/usb/host/usb_audio_player)
- [S31 Classic Bluetooth](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/api-reference/bluetooth/classic_bt.html)
- [ESP-GMF蓝牙包备选](https://github.com/espressif/esp-gmf/tree/main/packages/esp_bt_audio)

## 实物结果改变的边界

主机控制传输默认256字节，真板CX31993配置为422字节，故工程上限改4096。实际读到UAC2.0，直接交给UAC1驱动会触发其打开失败清理空指针。应用先解析描述符并拒绝不支持版本，避免进入该错误路径；没有修改managed_components或自行重写UAC2类驱动。

通过官方 `usb_host_config_t.fsls_only=true` 实测强制12Mbps全速，仍返回UAC2.0；没有获得UAC1兼容模式。最终恢复默认高速协商。UAC2的采样率不在这些格式描述符中，需时钟实体查询，当前没有谎报48kHz已获支持。

官方主分支README仍只支持UAC1，UAC2支持列入后续2.0.0里程碑：[官方规划](https://github.com/espressif/esp-usb/milestone/23)。下一步必须获得可复用、许可明确的UAC2 Host实现再推进发声；调大缓冲只解决枚举，不解决协议兼容。

蓝牙实验版已完成编译：SBC由锁定IDF内部解码，回调仅向16个固定PCM块队列复制；每块最多256帧，包含采样率、通道、时间和代次。音频任务非阻塞取样、线性重采样到USB速率、经过DSP，送官方USB环形缓冲。暂停/断开/格式变化增加代次并丢弃旧块，超过150ms的排队PCM丢弃；欠载输出做5ms淡变。此实现没有真实播放、时钟漂移或听感验收，不能标作P2通过。
