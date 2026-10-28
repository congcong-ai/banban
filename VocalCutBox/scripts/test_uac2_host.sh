#!/usr/bin/env bash
# 在主机运行实际复用的UAC2解析器，不模拟控制请求或硬件播放。
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$(mktemp -d /tmp/banban-uac2.XXXXXX)}"
mkdir -p "$out"
python3 - "$root/tests/fixtures/cx31993-06cb-1595.json" "$out/config.bin" <<'PY'
import json,sys
from pathlib import Path
Path(sys.argv[2]).write_bytes(bytes.fromhex(json.loads(Path(sys.argv[1]).read_text())['配置描述符']))
PY
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
 -I"$root/tests/host_uac2_tests/stubs" -I"$root/vendor/uac2_host/include" \
 "$root/vendor/uac2_host/uac2_desc.c" "$root/tests/host_uac2_tests/test_uac2_desc.c" -o "$out/test_uac2"
"$out/test_uac2" "$out/config.bin"
