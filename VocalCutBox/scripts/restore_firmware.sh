#!/usr/bin/env bash
# 恢复已归档版本，不使用当前build目录，也不重新编译。
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
python_bin="${BANBAN_PYTHON_ENV:-$HOME/.espressif/python_env/idf6.2_py3.12_env}/bin/python"
exec "$python_bin" "$root/scripts/firmware_restore.py" --bundle "$root/artifacts/firmware/current" "$@"
