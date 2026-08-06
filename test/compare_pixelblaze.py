#!/usr/bin/env python3
"""
比较两个 Pixelblaze JSON 输出文件，逐帧逐像素比较 RGB 值，输出详细统计。

格式要求：
{
  "frameCount": N,
  "pixelCount": M,
  "frames": [
    {"frame": i, "pixels": [[r,g,b], ...]},
    ...
  ]
}
也支持简化格式（frames 为纯数组）。

用法：
  python compare_pixelblaze.py <文件1> <文件2> [容差值] [--quiet]
  容差值默认为 5（每个通道允许的绝对差上限）。
  使用 --quiet 可仅输出统计信息，不显示详细差异。
"""

import json
import sys
import argparse

def load_json(path):
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)

def extract_frames(data):
    """从 JSON 数据中提取帧列表，统一为 [(frame_index, pixels_list), ...]"""
    frames = []
    raw = data.get("frames", []) if isinstance(data, dict) else []
    if not raw and isinstance(data, list):
        raw = data
    for idx, item in enumerate(raw):
        if isinstance(item, dict) and "pixels" in item:
            # 标准格式：{"frame": i, "pixels": [...]}
            frame_idx = item.get("frame", idx)
            pixels = item["pixels"]
        elif isinstance(item, list):
            # 简化格式：直接像素数组
            frame_idx = idx
            pixels = item
        else:
            raise ValueError(f"帧 {idx} 格式无效: {type(item)}")
        frames.append((frame_idx, pixels))
    return frames

def compare_frames(frames1, frames2, tolerance=5, verbose=True):
    if len(frames1) != len(frames2):
        print(f"帧数量不匹配: {len(frames1)} vs {len(frames2)}")
        return False

    total_pixels = 0
    mismatched_pixels = 0
    max_diff_r = 0
    max_diff_g = 0
    max_diff_b = 0
    max_diff_pixel = None  # 存储最大差异的像素位置

    all_match = True

    for (idx1, pix1), (idx2, pix2) in zip(frames1, frames2):
        if idx1 != idx2:
            print(f"警告：帧索引不匹配 ({idx1} vs {idx2})，按顺序比较")

        if len(pix1) != len(pix2):
            print(f"帧 {idx1}: 像素数量不匹配: {len(pix1)} vs {len(pix2)}")
            all_match = False
            continue

        total_pixels += len(pix1)

        for i, (p1, p2) in enumerate(zip(pix1, pix2)):
            # 计算每个通道的绝对差
            dr = abs(p1[0] - p2[0])
            dg = abs(p1[1] - p2[1])
            db = abs(p1[2] - p2[2])
            max_diff = max(dr, dg, db)

            if max_diff > tolerance:
                mismatched_pixels += 1
                all_match = False
            # 更新全局最大差异
            if dr > max_diff_r: max_diff_r = dr
            if dg > max_diff_g: max_diff_g = dg
            if db > max_diff_b: max_diff_b = db
            # 记录最大差异像素的位置（只保留最后一个，但不重要）
            if max_diff > max(max_diff_r, max_diff_g, max_diff_b):
                max_diff_pixel = (idx1, i, p1, p2)

    # 输出统计
    print("\n=== 统计结果 ===")
    print(f"总像素数: {total_pixels}")
    print(f"差异像素数: {mismatched_pixels}")
    print(f"差异比例: {mismatched_pixels / total_pixels * 100:.2f}%")
    print(f"容差值: {tolerance}")
    print(f"最大差异 (R,G,B): ({max_diff_r}, {max_diff_g}, {max_diff_b})")
    if max_diff_pixel:
        f, i, p1, p2 = max_diff_pixel
        print(f"最大差异位置: 帧 {f}, 像素 {i}, 期望 {p1}, 实际 {p2}")

    return all_match

def main():
    parser = argparse.ArgumentParser(description="比较 Pixelblaze JSON 输出")
    parser.add_argument("file1", help="第一个 JSON 文件")
    parser.add_argument("file2", help="第二个 JSON 文件")
    parser.add_argument("tolerance", nargs="?", type=int, default=5,
                        help="容差值，默认 5")
    parser.add_argument("--quiet", "-q", action="store_true",
                        help="仅输出统计信息，不显示每个差异像素")
    args = parser.parse_args()

    try:
        data1 = load_json(args.file1)
        data2 = load_json(args.file2)
        frames1 = extract_frames(data1)
        frames2 = extract_frames(data2)
        ret = compare_frames(frames1, frames2, args.tolerance, verbose=not args.quiet)
        if(ret == False):
            sys.exit(1)
        else:
            sys.exit(0) 
    except Exception as e:
        print(f"错误: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()