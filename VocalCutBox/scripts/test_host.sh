#!/usr/bin/env bash
# 运行真实纯C算法，产物默认放临时目录。
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$(mktemp -d /tmp/banban-dsp.XXXXXX)}"
mkdir -p "$out"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -I"$root/components/vocal_dsp/include" "$root/components/vocal_dsp/vocal_dsp.c" \
  "$root/tests/host_dsp_tests/test_dsp.c" -lm -o "$out/test_dsp"
"$out/test_dsp" | tee "$out/metrics.csv"
echo "测试产物：$out"
cc -std=c11 -Wall -Wextra -Werror -O2 \
  -I"$root/components/vocal_dsp/include" "$root/components/vocal_dsp/vocal_dsp.c" \
  "$root/tests/host_dsp_tests/dsp_pcm.c" -lm -o "$out/dsp_pcm"
python3 "$root/tests/tools/synthetic_audio.py" --driver "$out/dsp_pcm" --output "$out/audio"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -I"$root/components/vocal_dsp/include" "$root/components/vocal_dsp/test_tone.c" \
  "$root/tests/host_dsp_tests/test_tone.c" -o "$out/test_tone"
"$out/test_tone"
