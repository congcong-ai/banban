# 构建、烧录与阶段验收

## 当前默认输出后端

0.2.0默认 `CONFIG_BANBAN_USB_UAC2=y`，复用vendor/uac2_host，S31 Host协商全速。原官方UAC1后端保留，但不能播放本只CX31993。默认UAC2后端已在真板开启48kHz/16bit流并记录非零USB包；此处为早期调试记录；后续48kHz/32bit耳机播放已确认，当前状态以仓库首页和真板日志为准。

新增回归：`bash scripts/test_uac2_host.sh /tmp/banban-uac2-tests`，使用实际复用解析器验证本只DAC的2个时钟、5种AS备选、左右控制位与全部截断长度。

## Mac构建

使用本机已安装的ESP-IDF，不使用Docker：

```sh
cd VocalCutBox
bash scripts/build.sh
bash scripts/test_host.sh /tmp/banban-dsp-tests
```

脚本默认从 `$HOME/esp/esp-idf-smartbadge` 加载固定SDK，从 `$HOME/.espressif/python_env/idf6.2_py3.12_env` 加载Python。可用 `BANBAN_IDF_PATH` / `BANBAN_PYTHON_ENV` 覆盖。SDK提交必须匹配技术决策文件。

新电脑安装官方SDK：

```sh
git clone https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout ab95e70748f2eddac6f87217b0cf62533684a6ba
git submodule update --init --recursive
./install.sh esp32s31
```

安装路径不同需设置上述变量。SDK在项目外。本机SDK部分子模块用固定归档补齐，所以脚本使用 `IDF_SKIP_CHECK_SUBMODULES=1`；这不表示依赖完整，也不替代缺失文件检查。S31需 `idf.py --preview`。

初次解析通过组件Registry下载固定UAC版本，`dependencies.lock` 必须保留。普通增量构建不fullclean。默认构建产物 `build/banban.bin`，日志/测试媒体放临时目录，不提交二进制。

## 烧录

先确认设备是ESP32-S31、串口USB身份与MAC，备份旧固件。确认目标板身份并备份原固件，换设备需重新核对身份。

```sh
bash scripts/flash_monitor.sh --port /dev/cu.usbserial-XXXX \
  --expect-mac XX:XX:XX:XX:XX:XX --usb-serial 实际USB序列号 --dry-run
# 去掉 --dry-run 才会打开串口并写入。
# 蓝牙版本在上述命令追加 --build-dir build-bt；默认build是USB诊断镜像。
```

脚本检查构建目标、镜像偏移与文件、CP2102N序列号、芯片类型与MAC，再使用构建生成的flash_args。不会erase-flash，不写NVS镜像。会更新引导程序、分区表和factory应用，旧系统不再启动；本项目分区布局不能作为HomeStash OTA布局的兼容更新。完整旧Flash已存本地忽略目录 `.local/backups/`，包含隐私配置，不上传。

## 持续串口

```sh
"$HOME/.espressif/python_env/idf6.2_py3.12_env/bin/python" scripts/serial_session.py \
  --port /dev/cu.usbserial-XXXX --log /tmp/banban-device.log
```

115200波特率，支持输入命令，Ctrl+C退出。macOS USB-UART驱动可能在打开时复位，所以尽量保留一个会话。日志不写入录音、Wi-Fi或手机设备名称。

P1命令：`tone on`、`tone off`、`volume 0`至`volume 50`。默认10，参数表示软件播放音量，不是消除强度或实际声压。测试音未接DSP处理，便于单独定位USB。

## 验收顺序

1. P1：读取VID/PID、完整配置描述符、UAC版本和格式；确认USB数据转接头。耳机验证左440Hz、右880Hz、双声道和静音，连续5分钟；拔插并恢复。听感必须由实际试听确认，写入计数不能替代声音。
2. P2：只有P1实际出声与恢复验证后，才刷蓝牙实验构建；手机蓝牙设置配对 `VocalCutBox`，用合法来源音乐测试10分钟、暂停/下一曲/断连/恢复。实际速率应与日志一致。
3. P3：比较Bypass/Basic/Preserve、强度和播放音量，记录歌曲听感；不能将合成指标当真实效果。
4. P4：实体四键已接入并实测；LCD未完成。按键映射见10_BUTTON_CONTROLS.md。
5. P5：实际持续60分钟，记录heap/栈/欠载、反复拔插与配对，未运行就不标通过。

端到端延迟可用同一测试脉冲同时记录手机参照输出与DAC输出，再用波形差估算；目前没有实测延迟数值。

## 蓝牙实验版构建

```sh
bash scripts/build.sh --bt
```

脚本将同一SDK的BT组件复制到本工程 `.local/sdk-components/bt`，从官方固定提交下载S31控制器归档并核验SHA256，SDK本身保持只读。独立配置为 `.local/sdkconfig.bt`，输出 `build-bt/`，不覆盖默认P1配置。后续版本已完成真板配对和音乐试听，曾出现的噪声根因仍待复测；详见真板日志。

输入PCM队列约85ms容量（48kHz时），150ms以前的旧块丢弃，3块预填充；线性采样转换不作为高保真带限SRC或时钟同步验收。蓝牙实验命令为 `source bt`、`mode bypass/basic/preserve`、`strength 0..100`，音量仍是独立 `volume 0..50`。控制入口已编译，后续音乐试听反馈与边界见仓库首页。

实物USB描述符的主机回归：`bash scripts/test_usb_host.sh /tmp/banban-usb-tests`。它测试真实解析函数对422字节UAC2捕获、UAC1基线和每一种截断长度的处理，不模拟USB发声。

## 当前输出格式与静音

当前已实听通过的是USB 48kHz/32bit/双声道，默认不再使用实测无声的16bit路径。S31自定义周期TX FIFO为1024B，满足此端点MPS768B。DSP/SBC PCM仍是16bit，再左对齐扩展到USB32bit槽位；不意味着增加有效音频精度。

默认P1和板载对照镜像均启动静音。P1测试：先 `volume 10` 再 `tone on`，结束 `tone off`。诊断格式可用 `usb-format 48000 32`，重配由音频任务串行执行。蓝牙镜像默认接收BT音乐，`volume 0` 才是全局软件静音；`tone off`只控制测试音，不停止BT音乐。

蓝牙控制器必须 `CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY=y`，当前defaults已固定。真板已可发现，连接名VocalCutBox，无开发板端PIN/数字确认。手机、电脑和支持A2DP输出的电视从系统音频设置选择它；配对和音乐已有实测反馈，60分钟稳定性仍待验收。


### 串口名称随重新连接变化

2026-10-28本板串口从此前的`/dev/cu.usbserial-1410`变为`/dev/cu.usbserial-1420`。先列出当前端口，再由烧录脚本核对USB序列号、芯片ESP32-S31及预期MAC；不能只因名称相似就烧录。默认USB和BT镜像现均包含06cb:1595的16bit静音→32bit兼容初始化。启动仍不自动发测试音；USB诊断使用`volume 25`、`tone on`，音乐输入使用`source bt`。
