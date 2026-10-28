#!/usr/bin/env python3
"""从独立固件归档恢复，先校验文件和设备，写入后再次核验Flash。"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True, help='固件归档目录')
    parser.add_argument('--port', help='可选串口名，仍核对USB身份')
    parser.add_argument('--check', action='store_true', help='仅校验归档，不连接设备')
    args = parser.parse_args()
    bundle = args.bundle.resolve(strict=True)
    manifest = json.loads((bundle / 'manifest.json').read_text())
    if manifest['target'] != 'esp32s31' or manifest['schema'] != 1:
        raise ValueError('归档格式或目标芯片不正确')
    for name, info in manifest['files'].items():
        path = (bundle / name).resolve(strict=True)
        path.relative_to(bundle)
        data = path.read_bytes()
        if len(data) != info['bytes'] or hashlib.sha256(data).hexdigest() != info['sha256']:
            raise ValueError(f'归档校验失败：{name}；没有开始烧录')
    segments = sorted(manifest['segments'], key=lambda item: int(item['offset'], 0))
    if [int(item['offset'], 0) for item in segments] != [0x2000, 0x8000, 0x10000]:
        raise ValueError('不支持此写入布局')
    for i, item in enumerate(segments):
        size = manifest['files'][item['file']]['bytes']
        end = int(item['offset'], 0) + size
        limit = [0x8000, 0x9000, 0x187000][i]
        if end > limit:
            raise ValueError('镜像超出对应区域，拒绝写入')
    print(f"归档完整：{manifest['name']}，ESP32-S31，三个写入区域。", flush=True)
    if args.check:
        print('文件校验通过，未连接串口。')
        return

    import esptool
    from serial.tools import list_ports
    device = manifest['device']
    ports = [p for p in list_ports.comports()
             if p.vid == device['vid'] and p.pid == device['pid']
             and p.serial_number == device['usb_serial']
             and (not args.port or p.device == args.port)]
    if len(ports) != 1:
        raise ValueError('未找到唯一匹配的开发板UART；请检查UART数据线并关闭串口监视器')
    port = ports[0].device
    print(f'已匹配开发板UART：{port}；开始核对芯片与MAC。', flush=True)
    chip = esptool.detect_chip(port)
    verified = False
    try:
        if chip.CHIP_NAME != 'ESP32-S31':
            raise ValueError('目标不是ESP32-S31，拒绝烧录')
        mac = ':'.join(f'{v:02x}' for v in chip.read_mac())
        if mac != device['mac']:
            raise ValueError('芯片MAC不匹配，拒绝烧录')
        verified = True
    finally:
        if not verified:
            chip.hard_reset()
        chip._port.close()
    base = [sys.executable, '-m', 'esptool', '--chip', 'esp32s31', '--port', port,
            '--baud', '460800', '--before', 'no-reset', '--no-stub']
    pairs = [part for item in segments for part in (item['offset'], item['file'])]
    settings = manifest['flash_settings']
    try:
        subprocess.run(base + ['--after', 'no-reset', 'write-flash',
                              '--flash-mode', settings['flash_mode'],
                              '--flash-size', settings['flash_size'],
                              '--flash-freq', settings['flash_freq'], *pairs], cwd=bundle, check=True)
        # 上一次调用已改变ROM串口波特率；新进程必须重新复位进入ROM，
        # 不能用no-reset假定仍处在默认同步波特率。
        verify = base.copy()
        verify[verify.index('--before') + 1] = 'default-reset'
        subprocess.run(verify + ['--after', 'hard-reset', 'verify-flash', *pairs], cwd=bundle, check=True)
    except subprocess.CalledProcessError:
        print('恢复或校验失败；未报告成功。请保留日志并重新运行恢复命令。', file=sys.stderr)
        raise
    print('恢复成功：三个镜像均已核验Flash内容，设备已复位。', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, FileNotFoundError, ModuleNotFoundError) as error:
        sys.exit(f'恢复中止：{error}')
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
