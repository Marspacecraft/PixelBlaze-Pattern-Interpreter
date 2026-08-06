#!/usr/bin/env python3
"""
Pixelblaze 双帧对比播放器（垂直排列）
用法: python compare_player.py <文件1.json> <文件2.json>
"""

import json
import sys
import tkinter as tk
from tkinter import ttk, filedialog

class ComparePlayer:
    def __init__(self, master, data1, data2, name1="文件1", name2="文件2"):
        self.master = master
        self.frames1 = self._extract_frames(data1)
        self.frames2 = self._extract_frames(data2)
        self.pixel_count1 = data1.get("pixelCount", len(self.frames1[0]) if self.frames1 else 0)
        self.pixel_count2 = data2.get("pixelCount", len(self.frames2[0]) if self.frames2 else 0)

        # 帧数对齐
        self.frame_count = min(len(self.frames1), len(self.frames2))
        if len(self.frames1) != len(self.frames2):
            print(f"警告: 帧数不同 ({len(self.frames1)} vs {len(self.frames2)})，将使用较小值 {self.frame_count}")
        if self.pixel_count1 != self.pixel_count2:
            print(f"警告: 像素数不同 ({self.pixel_count1} vs {self.pixel_count2})，请确保布局一致")

        self.frame_index = 0
        self.running = False
        self.after_id = None
        self.fps = 30

        master.title("Pixelblaze 双帧对比播放器（垂直）")
        master.resizable(False, False)

        # 主框架：上下排列
        main_frame = ttk.Frame(master)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        # 上方（文件1）
        top_frame = ttk.LabelFrame(main_frame, text=name1, padding=5)
        top_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, pady=2)

        self.canvas1 = self._create_canvas(top_frame, self.pixel_count1)
        self.canvas1.pack()

        # 下方（文件2）
        bottom_frame = ttk.LabelFrame(main_frame, text=name2, padding=5)
        bottom_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, pady=2)

        self.canvas2 = self._create_canvas(bottom_frame, self.pixel_count2)
        self.canvas2.pack()

        # 控制区域
        control_frame = ttk.Frame(master)
        control_frame.pack(pady=5)

        self.play_btn = ttk.Button(control_frame, text="▶ 播放", command=self.toggle_play)
        self.play_btn.pack(side=tk.LEFT, padx=5)

        self.reset_btn = ttk.Button(control_frame, text="⏮ 重置", command=self.reset)
        self.reset_btn.pack(side=tk.LEFT, padx=5)

        self.info_label = ttk.Label(control_frame, text="帧 0 / {}".format(self.frame_count))
        self.info_label.pack(side=tk.LEFT, padx=20)

        # 帧率控制
        ttk.Label(control_frame, text="FPS:").pack(side=tk.LEFT, padx=(20,0))
        self.fps_var = tk.StringVar(value=str(self.fps))
        fps_spin = ttk.Spinbox(control_frame, from_=1, to=60, width=4,
                               textvariable=self.fps_var, command=self._update_fps)
        fps_spin.pack(side=tk.LEFT, padx=5)
        self.fps_var.trace("w", self._update_fps)

        master.protocol("WM_DELETE_WINDOW", self.on_close)

        self.update_frame(0)

    def _create_canvas(self, parent, pixel_count):
        """创建画布并绘制水平排列的像素矩形"""
        pixel_size = 30
        spacing = 5
        total_width = max(pixel_count * (pixel_size + spacing) + 20, 200)
        canvas = tk.Canvas(parent, width=total_width, height=pixel_size + 40, bg='#222')
        rects = []
        x0 = 10
        y0 = 20
        for i in range(pixel_count):
            rect = canvas.create_rectangle(
                x0 + i * (pixel_size + spacing), y0,
                x0 + i * (pixel_size + spacing) + pixel_size, y0 + pixel_size,
                fill='#000', outline='#555', width=1
            )
            rects.append(rect)
        canvas.rects = rects
        return canvas

    def _extract_frames(self, data):
        raw = data.get("frames", [])
        if not raw:
            return []
        if isinstance(raw[0], list):
            return raw
        elif isinstance(raw[0], dict) and "pixels" in raw[0]:
            return [item["pixels"] for item in raw]
        else:
            raise ValueError("无法识别的帧格式")

    def _draw_frame(self, canvas, pixels, count):
        if not pixels:
            return
        for i, color in enumerate(pixels[:count]):
            r, g, b = color[:3]
            r = max(0, min(255, r))
            g = max(0, min(255, g))
            b = max(0, min(255, b))
            hex_color = f'#{r:02x}{g:02x}{b:02x}'
            canvas.itemconfig(canvas.rects[i], fill=hex_color)

    def update_frame(self, idx):
        if idx >= self.frame_count:
            return
        pix1 = self.frames1[idx] if idx < len(self.frames1) else []
        self._draw_frame(self.canvas1, pix1, self.pixel_count1)
        pix2 = self.frames2[idx] if idx < len(self.frames2) else []
        self._draw_frame(self.canvas2, pix2, self.pixel_count2)
        self.info_label.config(text=f"帧 {idx+1} / {self.frame_count}")

    def next_frame(self):
        if self.frame_index < self.frame_count - 1:
            self.frame_index += 1
            self.update_frame(self.frame_index)
        else:
            self.frame_index = 0
            self.update_frame(self.frame_index)

    def toggle_play(self):
        if self.running:
            self.pause()
        else:
            self.play()

    def play(self):
        if self.frame_count == 0:
            return
        self.running = True
        self.play_btn.config(text="⏸ 暂停")
        self._schedule_next()

    def _schedule_next(self):
        if self.running:
            self.next_frame()
            self.after_id = self.master.after(int(1000 / self.fps), self._schedule_next)

    def pause(self):
        self.running = False
        self.play_btn.config(text="▶ 播放")
        if self.after_id:
            self.master.after_cancel(self.after_id)
            self.after_id = None

    def reset(self):
        self.pause()
        self.frame_index = 0
        self.update_frame(0)

    def _update_fps(self, *args):
        try:
            val = int(self.fps_var.get())
            if val > 0:
                self.fps = val
        except ValueError:
            pass

    def on_close(self):
        self.pause()
        self.master.destroy()

def main():
    if len(sys.argv) < 3:
        print("用法: python compare_player.py <文件1.json> <文件2.json>")
        print("也可以不带参数，通过对话框选择文件")
        root_temp = tk.Tk()
        root_temp.withdraw()
        files = filedialog.askopenfilenames(title="选择两个JSON文件", filetypes=[("JSON", "*.json")])
        root_temp.destroy()
        if len(files) != 2:
            print("必须选择两个文件")
            sys.exit(1)
        file1, file2 = files[0], files[1]
    else:
        file1, file2 = sys.argv[1], sys.argv[2]

    try:
        with open(file1, 'r') as f:
            data1 = json.load(f)
        with open(file2, 'r') as f:
            data2 = json.load(f)
    except Exception as e:
        print(f"读取文件失败: {e}")
        sys.exit(1)

    root = tk.Tk()
    name1 = file1.split('/')[-1]
    name2 = file2.split('/')[-1]
    app = ComparePlayer(root, data1, data2, name1, name2)
    root.mainloop()

if __name__ == "__main__":
    main()