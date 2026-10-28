# 板载喇叭对照测试

日期：2026-10-27。用户确认同一只CX31993与同一副华为原装3.5mm耳机在Mac可出声、可调音量；接S31后仍无声，软件音量10→25也无改善。按用户要求加入板载喇叭对照，继续保留USB问题，不以板载发声代替USB或D100验收。

## 复用依据

只读参考SmartBadge固定官方BSP来源 `esp-dev-kits 6f4b0f6ef304d76246325e9a9f94c1ddad8ed39c` 的 `esp32_s31_korvo.c` 音频初始化段及 `include/bsp/audio.h`、板卡引脚定义；并查阅SmartBadge的实际 `esp_codec_dev_open/write` 与独立音量实现。仅在伴伴新增最小音频封装，没有复制整个参考工程或修改参考文件。

ES8389驱动锁定 `espressif/esp_codec_dev ==1.6.2`，与本机已验证版本相同，Apache-2.0。保留BSP源文件版权与出处，不重写Codec寄存器驱动。

- I2C SDA0 / SCL1，Codec地址采用官方ES8389默认常量。
- I2S MCLK2 / BCLK3 / WS4 / DOUT5 / DIN6，48kHz / 16bit / 双声道，沿用已验证BSP的STD配置。
- PA控制GPIO7，DAC模式，硬件增益补偿与BSP一致（PA5V、Codec3.3V）。
- 按BSP配置共享TX/RX时钟，但不创建麦克风Codec、不读取或保存输入、不启动录音业务。

## 构建与状态

```sh
bash scripts/build.sh --speaker
# 产物 build-speaker/banban.bin
# 烧录仍需核实串口、USB序列号与芯片MAC，flash脚本加 --build-dir build-speaker。
```

独立配置 `.local/sdkconfig.speaker`，`CONFIG_BANBAN_BOARD_SPEAKER_TEST=y`。该镜像只启动板载Codec输出，不同时启动USB或BT，避免两个音频任务并发访问同一测试音状态。

渲染回调与USB版完全相同：左440Hz、右880Hz、双声道、静音各1秒循环。Codec音量80/100，软件测试音默认20/50，但启动时保持静音；显式tone on后才开始，保持淡变与限幅。串口 `tone off` 可静音，`volume 0..50` 可调整软件电平。

编译及烧录已通过，用户确认板载喇叭已出声且很吵；已立即静音并切回USB路径。串口写入成功只能作为数字侧证据，不提前宣称“喇叭通过”。

板载4Ω/3W功放端子不能接D100 RCA。这是对照测试，最终有源音响仍需独立DAC模拟输出或经核验的线路级接口。
