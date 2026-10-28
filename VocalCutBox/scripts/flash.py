#!/usr/bin/env python3
"""核对构建目标、USB-UART身份与芯片MAC，然后烧录。不擦除整片Flash。"""
import argparse,json,subprocess,sys
from pathlib import Path
import esptool
from serial.tools import list_ports
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--port',required=True)
p.add_argument('--expect-mac',required=True)
p.add_argument('--usb-serial',required=True)
p.add_argument('--build-dir',default='build')
p.add_argument('--dry-run',action='store_true')
a=p.parse_args();root=Path(__file__).resolve().parents[1];build=(root/a.build_dir).resolve()
meta=json.loads((build/'project_description.json').read_text())
assert meta['target']=='esp32s31','构建目标不正确'
args=json.loads((build/'flasher_args.json').read_text())
for off,file in args['flash_files'].items():
 assert int(off,0)>=0x2000 and (build/file).is_file(),'镜像/偏移无效'
print('构建目标 ESP32-S31，写入：'+', '.join(args['flash_files']),flush=True)
if a.dry_run: print('预检完成，未打开串口。');sys.exit(0)
ports=[p for p in list_ports.comports() if p.device==a.port and p.serial_number==a.usb_serial and p.vid==0x10c4 and p.pid==0xea60]
assert len(ports)==1,'USB身份与指定串口不匹配'
e=esptool.detect_chip(a.port)
verified=False
try:
 assert e.CHIP_NAME=='ESP32-S31','芯片不是S31'
 assert ':'.join(f'{x:02x}' for x in e.read_mac())==a.expect_mac.lower(),'芯片MAC不符'
 verified=True
finally:
 if not verified:e.hard_reset()
 e._port.close()
subprocess.run([sys.executable,'-m','esptool','--chip','esp32s31','--port',a.port,'--baud','460800','--before','no-reset','--no-stub',
 'write-flash','@flash_args'],cwd=build,check=True)
print('烧录命令成功；声音、热插拔、稳定性仍须实测。')
