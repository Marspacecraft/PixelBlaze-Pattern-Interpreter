#!/bin/bash
set -eo pipefail

# ===================== 配置区 =====================
ERROR_THRESHOLD=2   # RGB通道差值超过该值视为错误
REF_FILE="ref.json"
TARGET_FILE="target.json"
# ==================================================

usage() {
cat <<HELP
PB VM 双端对比工具
用法:
  ./pb_compare.sh -f 脚本.js [-n 灯数] [-t 固定时间] [-c 帧数] [-s 时间步]

参数:
  -f FILE    PB脚本文件（必填）
  -n NUM     LED数量，默认 30
  -t TIME    固定全局时间，默认 0.0
  -c FRAMES  渲染帧数，默认 1
  -s STEP    每帧递增时间，默认 0.1
  -h         帮助

示例:
  ./pb_compare.sh -f test.pattern -n 24 -t 0 -c 3 -s 0.1
HELP
exit 1
}

# 默认参数
LED_COUNT=30
FIX_TIME=0.0
FRAME_CNT=1
TIME_STEP=0.1
SCRIPT_FILE=""

# 解析参数
while getopts "f:n:t:c:s:h" opt; do
  case "$opt" in
    f) SCRIPT_FILE="$OPTARG" ;;
    n) LED_COUNT="$OPTARG" ;;
    t) FIX_TIME="$OPTARG" ;;
    c) FRAME_CNT="$OPTARG" ;;
    s) TIME_STEP="$OPTARG" ;;
    h) usage ;;
    *) usage ;;
  esac
done

[[ -z "$SCRIPT_FILE" ]] && echo "错误：必须指定 -f 脚本文件" && usage
[[ ! -f "$SCRIPT_FILE" ]] && echo "错误：脚本 $SCRIPT_FILE 不存在" && exit 2
[[ ! -x ./main ]] && echo "错误：当前目录不存在可执行文件 ./main" && exit 3

# 读取脚本全文
PB_SRC=$(cat "$SCRIPT_FILE")

echo -e "\n===== 阶段1：pixelblaze-client 生成标准参考帧 ${REF_FILE} ====="
python3 <<PY
import json
from pixelblaze.emulator import PixelblazeEmulator

led = int("$LED_COUNT")
t0 = float("$FIX_TIME")
frames = int("$FRAME_CNT")
step = float("$TIME_STEP")
src = '''$PB_SRC'''

emu = PixelblazeEmulator(pixel_count=led)
emu.load_pattern(src)
emu.set_time(t0)
out = []
for _ in range(frames):
    out.append(emu.render_frame())
    emu.advance_time(step)

with open("$REF_FILE", "w", encoding="utf8") as f:
    json.dump(out, f, indent=2)
PY

echo -e "\n===== 阶段2：执行 ./main 生成待校验帧 ${TARGET_FILE} ====="
# 约定你的 ./main 入参规则（和上层对齐）
# ./main  -f 脚本路径 -n LED数 -t 起始时间 -c 帧数 -s 步长 -o 输出json
./main -f "$SCRIPT_FILE" -n "$LED_COUNT" -t "$FIX_TIME" -c "$FRAME_CNT" -s "$TIME_STEP" -o "$TARGET_FILE"

echo -e "\n===== 阶段3：自动对比帧数据 ====="
python3 <<PY
import json
import sys

threshold = int("$ERROR_THRESHOLD")
ref_path = "$REF_FILE"
tgt_path = "$TARGET_FILE"

with open(ref_path, "r", encoding="utf8") as f:
    ref_frames = json.load(f)
with open(tgt_path, "r", encoding="utf8") as f:
    tgt_frames = json.load(f)

total_err = 0
frame_err_list = []

if len(ref_frames) != len(tgt_frames):
    print(f"严重错误：帧数不一致 参考:{len(ref_frames)} 待测:{len(tgt_frames)}")
    sys.exit(1)

for frame_idx, (ref_buf, tgt_buf) in enumerate(zip(ref_frames, tgt_frames)):
    frame_err = 0
    if len(ref_buf) != len(tgt_buf):
        print(f"第{frame_idx}帧像素数量不一致 ref:{len(ref_buf)} tgt:{len(tgt_buf)}")
        frame_err += 9999
    else:
        for pix_idx, (rp, tp) in enumerate(zip(ref_buf, tgt_buf)):
            for ch, cr in enumerate(zip(rp, tp)):
                rv, tv = cr
                diff = abs(int(rv) - int(tv))
                if diff > threshold:
                    frame_err += 1
                    print(f"帧{frame_idx} 像素{pix_idx} 通道{R,G,B[ch]} | 标准:{rv} 实测:{tv} 差值:{diff}")
    if frame_err > 0:
        frame_err_list.append((frame_idx, frame_err))
        total_err += frame_err

print("\n===== 对比汇总 =====")
if total_err == 0:
    print("✅ 全部帧完全匹配，VM 渲染结果一致")
else:
    print(f"❌ 总计异常通道数：{total_err}")
    for fid, cnt in frame_err_list:
        print(f"   第{fid}帧异常通道：{cnt}")
    sys.exit(1)
PY

echo -e "\n对比完成，参考文件:$REF_FILE 待测文件:$TARGET_FILE"
exit 0