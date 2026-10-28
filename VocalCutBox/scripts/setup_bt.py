#!/usr/bin/env python3
"""仅在新工程私有目录补齐固定版本的官方S31控制器库，不修改全局SDK。"""
import hashlib,os,shutil,tarfile,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];sdk=Path(os.environ.get('BANBAN_IDF_PATH',str(Path.home()/'esp/esp-idf-smartbadge')))
base=root/'.local/sdk-components';base.mkdir(parents=True,exist_ok=True);bt=base/'bt'
if not bt.exists():shutil.copytree(sdk/'components/bt',bt)
commit='cc027ecd288b31198f82331faabb972a37d6800e';archive=base/'esp32s31-bt-lib.tar.gz'
if not archive.exists():subprocess.run(['curl','-fLsS',f'https://codeload.github.com/espressif/esp32s31-bt-lib/tar.gz/{commit}','-o',str(archive)],check=True)
assert hashlib.sha256(archive.read_bytes()).hexdigest()=='f95851bf72da02b929f2c120306a58275b844a3275eee94b5af39bf4d876ff19','官方归档摘要不一致'
with tarfile.open(archive) as t:t.extractall(base,filter='data')
source=base/f'esp32s31-bt-lib-{commit}';dest=bt/'controller/lib_esp32s31/esp32s31-bt-lib'
shutil.copytree(source,dest,dirs_exist_ok=True)
print('已在工程私有目录准备官方BT组件与固定控制器库；未修改SDK。')
