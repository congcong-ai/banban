#!/usr/bin/env bash
# 编译独立固件，不连接串口，不改写其他工程或 SDK。
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
sdk="${BANBAN_IDF_PATH:-$HOME/esp/esp-idf-smartbadge}"
expected=ab95e70748f2eddac6f87217b0cf62533684a6ba
[[ "$(git -C "$sdk" rev-parse HEAD)" == "$expected" ]] || { echo 'ESP-IDF 提交不符，请核对 docs/01_TECH_DECISIONS.md'; exit 1; }
export PATH="${BANBAN_PYTHON_ENV:-$HOME/.espressif/python_env/idf6.2_py3.12_env}/bin:$PATH"
source "$sdk/export.sh"
export IDF_SKIP_CHECK_SUBMODULES=1
cd "$root"
if [[ ! -f sdkconfig ]]; then idf.py --preview set-target esp32s31; fi
if [[ "${1:-}" == "--bt" ]]; then
  export BANBAN_WITH_BT=1
  python "$root/scripts/setup_bt.py"
  idf.py --preview -B build-bt -D BANBAN_WITH_BT=ON -D SDKCONFIG="$root/.local/sdkconfig.bt" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.bt.defaults" build
elif [[ "${1:-}" == "--speaker" ]]; then
  mkdir -p "$root/.local"
  idf.py --preview -B build-speaker -D SDKCONFIG="$root/.local/sdkconfig.speaker" -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.speaker.defaults" build
else
  idf.py --preview build
fi
