#!/bin/bash

total=0
pass=0
fail=0
failed_scripts=()

echo "开始批量测试..."
echo "=============================="

for jsfile in *.js; do
    # 如果没有匹配到任何 .js 文件，退出
    [ -e "$jsfile" ] || { echo "没有找到任何 .js 文件"; exit 1; }

    ((total++))
    echo
    echo ">>> 测试 $jsfile ..."
    if ./test.sh "$jsfile"; then
        echo "✅ PASS: $jsfile"
        ((pass++))
    else
        echo "❌ FAIL: $jsfile"
        failed_scripts+=("$jsfile")
        ((fail++))
    fi
done

echo
if [ ${#failed_scripts[@]} -gt 0 ]; then
    echo "=============================="
    echo "失败的脚本:"
    for f in "${failed_scripts[@]}"; do
        echo "  - $f"
    done
    echo
fi
echo "=============================="
echo "测试汇总: 总计 $total, 通过 $pass, 失败 $fail"