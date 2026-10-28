# 已归档固件的一行恢复

2026-10-28保存四键蓝牙固件，镜像对应源码提交`075df12093070cc0d5cc0f73f1e90bd25339c099`。归档与`build/`、`build-bt/`分开，后续重新开发或编译不会覆盖它。

## 一行刷回

在VocalCutBox工程根目录执行：

```bash
./scripts/restore_firmware.sh
```

无须重新编译。脚本自动从归档读取设备身份、寻找当前UART端口，核对CP2102N序列号、ESP32-S31与芯片MAC；先校验归档全部SHA256，再写入bootloader、分区表、应用，另起一次`verify-flash`逐段核验，最后复位。

需要开发板稳定供电、USB-C UART数据线连接Mac，其他串口监视器关闭。`--port`可显式指定，但不能跳过设备身份校验。只核对文件、不连接板子可用：

```bash
./scripts/restore_firmware.sh --check
```

## 保存位置与内容

- 默认恢复入口：`artifacts/firmware/current`，相对符号链接指向下面的固定归档。
- 固定归档目录：`artifacts/firmware/20261028-buttons-075df12/`。
- 独立压缩包：`artifacts/firmware/20261028-buttons-075df12.tar.gz`，旁边保留压缩包SHA256。
- 包含三个烧录镜像、ELF/MAP、依赖锁和配置默认值、源码快照、逐文件SHA256清单，以及独立`restore.sh`/`restore.py`。

解压后也可以在归档目录直接运行`./restore.sh`。压缩包不依赖本项目当前脚本或构建目录，可另行复制保管。固件及归档含本板身份，留在本机忽略目录，不上传Git仓库；恢复脚本和说明已版本化。

只写0x2000、0x8000、0x10000区域，不整片擦除，不备份/恢复NVS配对数据。原HomeStash的完整Flash备份未改动。恢复后默认原曲，首次PLAY切基础消除100，SET换模式，VOL±改强度，板载喇叭不启动。

本机工具为Python3.12环境中的esptool5.5.0，脚本默认复用现有IDF6.2 Python环境，不调用IDF或编译器。换机器或删除该环境后，可在独立Python虚拟环境安装`esptool==5.5.0`，通过`BANBAN_PYTHON_ENV`指定虚拟环境目录；使用同一块已记录身份的板卡。

## 真实测试

1. 独立压缩包内全部文件SHA256通过。
2. 临时复制归档并翻转应用镜像一个字节：`--check`按预期拒绝，未打开串口。
3. 实际执行同一条`./scripts/restore_firmware.sh`。初版独立校验沿用no-reset，遇到前次ROM通信波特率未复位导致连接失败；已改成校验阶段default-reset，重新执行完整测试。
4. 修正后真实写入成功；独立verify-flash分别确认bootloader 21216字节、分区表3072字节、应用1132320字节，全部digest matched，命令退出码0。
5. 恢复后保持UART观察15秒：S31正常启动、ADC校准与四键驱动启动、BR/EDR及A2DP可发现、CX31993自动16→32初始化成功，48kHz/32bit/2ch，USB错误和跳过包0。未再次要求用户试听，也未将默认静音的PCM0当作音乐输出通过。

精简证据见`docs/evidence/firmware-restore-20261028.log`与`firmware-archive-20261028.json`。本机完整日志：`/tmp/banban-restore-current-test-fixed.log`、`/tmp/banban-restore-boot-check.log`。
