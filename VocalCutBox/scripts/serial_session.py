#!/usr/bin/env python3
"""保持一个UART会话，输出日志并接受标准输入中的控制命令；Ctrl+C结束。"""
import argparse,select,sys,time
from pathlib import Path
import serial
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--port',required=True);p.add_argument('--log',required=True);p.add_argument('--seconds',type=float,default=0);a=p.parse_args()
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.05;s.dtr=False;s.rts=False
Path(a.log).parent.mkdir(exist_ok=True,parents=True)
s.open();start=time.monotonic()
try:
 with open(a.log,'ab',buffering=0) as f:
  while not a.seconds or time.monotonic()-start<a.seconds:
   b=s.read(4096)
   if b:f.write(b);sys.stdout.write(b.decode(errors='replace'));sys.stdout.flush()
   if select.select([sys.stdin],[],[],0)[0]:
    cmd=sys.stdin.readline()
    if cmd:s.write(cmd.encode())
except KeyboardInterrupt:pass
finally:s.close()
