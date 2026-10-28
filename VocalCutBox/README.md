# 伴伴固件工程（VocalCutBox）

项目介绍、硬件、构建与验收状态见[仓库首页](../README.md)。当前目录是固件源码根目录，蓝牙设备名仍为 `VocalCutBox`。

## 一行刷回固件

在本目录执行：

```bash
./scripts/restore_firmware.sh
```

只检查本机归档：`./scripts/restore_firmware.sh --check`。新克隆不含固件归档；此入口用于原开发板的本机恢复。[完整说明](docs/11_FIRMWARE_BACKUP_RESTORE.md)。

[构建与烧录](docs/03_BUILD_FLASH_TEST.md) · [第三方许可](THIRD_PARTY_LICENSES.md) · [MIT](LICENSE)
