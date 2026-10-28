#!/usr/bin/env bash
# 写入已构建固件；显式指定串口与预期MAC，首次使用前需用户授权覆盖。
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
python_bin="${BANBAN_PYTHON_ENV:-$HOME/.espressif/python_env/idf6.2_py3.12_env}/bin/python"
exec "$python_bin" "$root/scripts/flash.py" "$@"
