#!/usr/bin/env bash
# 测试真实描述符解析层；不模拟硬件播放成功。
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$(mktemp -d /tmp/banban-usb.XXXXXX)}"
mkdir -p "$out"
python3 - "$root/tests/fixtures/cx31993-06cb-1595.json" "$out/config.bin" <<'PY'
import json,sys
from pathlib import Path
Path(sys.argv[2]).write_bytes(bytes.fromhex(json.loads(Path(sys.argv[1]).read_text())['配置描述符']))
PY
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
 -I"$root/components/usb_audio_out/include" "$root/components/usb_audio_out/usb_descriptor.c" \
 "$root/tests/host_usb_tests/test_descriptors.c" -o "$out/test_usb"
"$out/test_usb" "$out/config.bin"
