# 可复用UAC2 Host移植与真实结果

日期：2026-10-27。目标仍是ESP32-S31-Korvo-1 V1.1；没有换成S3、改变全局SDK或修改只读参考项目。

## 候选审查与选择

1. 乐鑫usb_host_uac 1.5.0：只支持UAC1，前次真板已确认本只DAC不兼容。其后续UAC2规划不是可直接使用的实现。
2. TinyUSB现有audio_host支持UAC1/UAC2，但需要同时核对S31 Host控制器端口与当前IDF集成；直接替换整个Host栈会扩大改动范围。
3. EspUsbHost有UAC2支持，但为Arduino C++综合USB库，与本项目纯ESP-IDF组件组织不同，且真实音频设备覆盖有限。
4. 选用Averyy/esp-uac2-host：纯C ESP-IDF组件、MIT，已有S3/miniDSP真机播放与热插拔记录，底层调用官方USB Host API，适合最小移植。上游记录只用于筛选，不作为本板通过证据。

固定上游版本0.1.2，提交 `a072adb6a2410f9f93474637a483724c93d5fc6f`。源码仅复制组件文件，完整保留LICENSE及来源哈希。没有复制上游模拟器或开发提示文件。局部改动详见 `vendor/uac2_host/PATCHES.md`。

## S31适配边界

使用S31官方USB Host，设置 `fsls_only=true`，协商12Mbps全速。本组件当前按1ms包周期调度，不能直接在480Mbps高速的125us微帧下发送；驱动入口明确拒绝非全速设备。48kHz/16bit双声道只需192字节/ms，符合实物端点能力，无需高速传输。

原始解析器只保存4种AS备选；本只DAC有5种，因此上限增加到8。补充空指针、总长度、截断、零长和短格式描述符检查，在主机ASan/UBSan运行真实解析源码，保留S31编译与真实USB请求测试两层证据。

本只DAC有两个时钟源，但播放链是USB输入端子1→特征单元2→耳机输出端子3，时钟9；录音时钟10未使用。当前沿用上游优先播放端子解析并通过实物拓扑回归核实，**不宣称通用多时钟或双工支持**。

## 实际控制与播放

本次真板日志读取：

- VID/PID `06cb:1595`，产品描述 `VENTURE-DAS-32`，UAC2.0，配置422字节。
- 时钟范围离散为8k、16k、32k、44.1k、48k、96kHz（全速配置）。
- 实际选择接口1、alt1、OUT端点0x01，48kHz/16bit/双声道。
- SET采样率成功；GET读回48000Hz，Clock Valid=true。
- 特征单元2主通道可控制mute，左右声道可控制volume；左右设置-12dB、主通道解除静音均返回ESP_OK。
- 测试音每段1秒：左440Hz、右880Hz、双声道、静音。软件默认音量10；不默认满幅。
- 真实连续PCM发送超过5分钟，失败=0、传输错误=0、事件丢失=0，heap约421064字节保持不变。属于USB发送结果，耳机/D100实际声音需用户确认；尚未做60分钟音频验收。

44.1kHz虽然在设备时钟范围中，但上游没有显式feedback时会拒绝分数包率播放；本项目优先48kHz。BT 44.1kHz输入仍须在应用转换到48kHz，不可直接写入USB。

## 可复验命令

```sh
bash scripts/test_uac2_host.sh /tmp/banban-uac2-host-tests
bash scripts/test_usb_host.sh /tmp/banban-usb-tests
bash scripts/test_host.sh /tmp/banban-dsp-tests
bash scripts/build.sh
bash scripts/build.sh --bt
```

烧录脚本仍按CP2102N USB序列号与芯片MAC核实设备，不仅依赖易变化的串口名称。用户已授权此板供伴伴开发，沿用已有完整Flash备份。板上默认保留USB测试固件，蓝牙镜像的编译不等于手机播放通过。

## 有源音响与Type-C问题

CX31993的模拟3.5mm输出可作为后续有源音响输入来源，接已核验三极TRS→红白双RCA线，D100和DAC音量先调低。耳机不是固件的运行依赖，只是初次核验左右输出的工具；D100实际出声另行记录，不能据USB发送成功直接勾选。

开发板UART Type-C接USB转串口桥，Power Type-C仅供电，没有连接USB Host数据通路。DAC必须继续接USB-A Host，现有转接头已通过枚举和持续传输验证。板载4Ω/3W扬声器功放端不能接RCA。

## 官方/上游资料

- [复用组件](https://github.com/Averyy/esp-uac2-host/tree/a072adb6a2410f9f93474637a483724c93d5fc6f/components/uac2_host)
- [TinyUSB Host Audio](https://docs.tinyusb.org/en/latest/examples/host/audio_host.html)
- [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost)
- [S31-Korvo-1接口说明](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s31/esp32-s31-korvo-1/user_guide.html)

## 最新听感与无声诊断

用户反馈耳机“完全听不到”，因此“USB发送超过5分钟”不等于模拟播放通过。诊断版新增只读传输统计：完成包数、实际完成字节数、非零负载字节、补零字节与跳过包；应用输出PCM峰值、mute和左右音量读回。

真板读回：PCM峰值327，mute=0，左右-12dB；USB完成与非零字节持续增长，跳过0，起始补零后无持续欠载。现阶段仍需独立手机/电脑DAC+耳机基线和后续实机排查，不能宣称已输出给D100。

## 解决路径与最终已确认格式

板载ES8389对照已被用户确认实际出声，随即按要求静音。随后尝试32bit，发现默认周期TX FIFO只能接受MPS≤512B，而此设备32bit端点的MPS为768B。采用官方Host自定义FIFO API，在S31上分配1024B周期TX、520B RX、256B控制TX，32bit流得以真正启动。

用户实听确认48kHz/32bit双声道有声、左右正确，默认格式据此设为32bit。16bit在当前组合下无声；24bit没有听感验收，不泛化为全部格式均可播放。DSP核心仍是PCM16，USB32bit采用有符号左对齐扩展，低位补零，不额外创造音频精度。

新增串口 `usb-format 48000 16/24/32`，所有关闭/重开由唯一writer任务执行，重配失败后也可再次请求恢复，不必反复烧录。使用非默认格式前须确认设备范围和实际听感。

测试音改成自制2048点低幅度查表与定点淡变，经主机频率/通道/峰值/静音/分块一致性验证；真板渲染峰值457us，相比原逐样本sinf约10ms大幅下降，持续补零问题不再增长。默认启动静音，防止接有源音响时自动发声。

D100的三极TRS→双RCA线已由用户确认，但用户当前无法连接，故音箱输出仍待实物验收。USB-A→DAC→耳机模拟输出已经实听通过，UART Type-C仍不是音频Host入口。


## 06cb:1595 的启动兼容处理（2026-10-28）

实测不能把“32bit流启动、Clock Valid、mute读回正确、非零USB完成字节”视为一定可闻：冷启动直接32bit曾完全无声；用户确认手动打开16bit再关闭重开32bit后恢复，左右正确。

兼容步骤位于应用封装 `prepare_cx31993()`，没有修改上游通用驱动，也没有绕过描述符协商。仅当VID=06cb、PID=1595且请求48kHz/32bit时，先从描述符选择48kHz/16bit，使用同样的音量/取消静音流程发送250ms零PCM，然后关闭全部该流资源、重开32bit。错误会记录并关闭句柄，不继续使用半初始化流；其他设备和其他请求格式不会套用。不能将此处理声称为所有CX31993的官方要求，DAC内部根因仍未确认。

仅USB版自动流程已通过一次真实拔出约5秒、重新插回后的耳机左右试听；仍需重复拔插、整板断电启动及长时间播放。初始化只发送静音，不自动开启测试音或板载喇叭。蓝牙版复用同一封装，实机歌曲验收独立记录。
