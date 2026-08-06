#!/bin/bash

# 从命令行参数获取脚本文件名，若未提供则报错
if [ $# -eq 0 ]; then
    echo "Usage: $0 <script_file.js>"
    exit 1
fi
SCRIPT="$1"

BASENAME=$(basename "$SCRIPT" .js)
JSJSON="${BASENAME}.json"

# 检查参考文件是否存在
if [ ! -f "$JSJSON" ]; then
    echo "Error: Reference JSON file '$JSJSON' not found."
    echo "Please generate it from Pixel_IDE with any parameters."
    exit 1
fi

# 从 JSJSON 中读取 pixelCount 和 frameCount
PIXELS=$(python3 -c "import json; print(json.load(open('$JSJSON'))['pixelCount'])")
FRAMES=$(python3 -c "import json; print(json.load(open('$JSJSON'))['frameCount'])")

echo "Using pixelCount=$PIXELS, frameCount=$FRAMES from $JSJSON"

# VM 输出文件名
VMJSON="${BASENAME}_pixelCount${PIXELS}_frameCount${FRAMES}.json"

echo "Generating VM output: $VMJSON"
./pb_vm -s "$SCRIPT" -p "$PIXELS" -f "$FRAMES" -o "$VMJSON"
if [ $? -ne 0 ]; then
    echo "Error: pb_vm execution failed."
    exit 1
fi

echo "Comparing $VMJSON and $JSJSON ..."
python3 compare_pixelblaze.py "$VMJSON" "$JSJSON"

exit $?