#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Pixelblaze 控制器 - GUI 版本
基于 pyserial 实现简单串口通信
"""

import sys
import os
import time
import struct
import threading
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("请先安装依赖库：")
    print("pip install pyserial")
    sys.exit(1)

# 命令字节（必须与 Arduino 端一致）
CMD_START = 0x01
CMD_CLOSE = 0x02
CMD_UPLOAD_START = 0x03
CMD_UPLOAD_DATA = 0x04

MAX_SCRIPT_SIZE = 8192
BAUDRATE = 115200


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Pixelblaze 控制器")
        self.geometry("600x500")
        self.resizable(False, False)

        self.selected_port = tk.StringVar()
        self.script_path = tk.StringVar()

        self.create_widgets()
        self.refresh_ports()
        self.protocol("WM_DELETE_WINDOW", self.on_closing)

    def create_widgets(self):
        main_frame = ttk.Frame(self, padding="10")
        main_frame.pack(fill=tk.BOTH, expand=True)

        port_frame = ttk.LabelFrame(main_frame, text="串口设置", padding="5")
        port_frame.pack(fill=tk.X, pady=5)

        ttk.Label(port_frame, text="串口:").grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
        self.port_combo = ttk.Combobox(port_frame, textvariable=self.selected_port, width=30)
        self.port_combo.grid(row=0, column=1, padx=5, pady=5)

        refresh_btn = ttk.Button(port_frame, text="刷新", command=self.refresh_ports)
        refresh_btn.grid(row=0, column=2, padx=5, pady=5)

        file_frame = ttk.LabelFrame(main_frame, text="脚本文件", padding="5")
        file_frame.pack(fill=tk.X, pady=5)

        ttk.Label(file_frame, text="文件:").grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
        self.file_entry = ttk.Entry(file_frame, textvariable=self.script_path, width=40)
        self.file_entry.grid(row=0, column=1, padx=5, pady=5)

        browse_btn = ttk.Button(file_frame, text="浏览...", command=self.browse_file)
        browse_btn.grid(row=0, column=2, padx=5, pady=5)

        btn_frame = ttk.Frame(main_frame)
        btn_frame.pack(fill=tk.X, pady=10)

        self.start_btn = ttk.Button(btn_frame, text="启动灯效", command=self.start_pattern)
        self.start_btn.pack(side=tk.LEFT, padx=5)

        self.close_btn = ttk.Button(btn_frame, text="关闭灯效", command=self.close_pattern)
        self.close_btn.pack(side=tk.LEFT, padx=5)

        self.upload_btn = ttk.Button(btn_frame, text="上传脚本", command=self.upload_script)
        self.upload_btn.pack(side=tk.LEFT, padx=5)

        log_frame = ttk.LabelFrame(main_frame, text="日志输出", padding="5")
        log_frame.pack(fill=tk.BOTH, expand=True, pady=5)

        log_btn_frame = ttk.Frame(log_frame)
        log_btn_frame.pack(fill=tk.X, pady=(0, 5))

        clear_btn = ttk.Button(log_btn_frame, text="清空日志", command=self.clear_log)
        clear_btn.pack(side=tk.RIGHT)

        self.log_text = scrolledtext.ScrolledText(log_frame, height=10, state='disabled')
        self.log_text.pack(fill=tk.BOTH, expand=True)

        self.status_var = tk.StringVar(value="就绪")
        status_bar = ttk.Label(main_frame, textvariable=self.status_var, relief=tk.SUNKEN, anchor=tk.W)
        status_bar.pack(fill=tk.X, pady=(5, 0))

    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports and not self.selected_port.get():
            self.selected_port.set(ports[0])
        self.log(f"已刷新串口列表，发现 {len(ports)} 个端口")

    def browse_file(self):
        filename = filedialog.askopenfilename(
            title="选择 Pixelblaze 脚本文件",
            filetypes=[("Pixelblaze 脚本", "*.js"), ("所有文件", "*.*")]
        )
        if filename:
            self.script_path.set(filename)
            self.log(f"选择文件: {filename}")

    def log(self, msg):
        import datetime
        ts = datetime.datetime.now().strftime("%H:%M:%S")
        self.log_text.config(state='normal')
        self.log_text.insert(tk.END, f"[{ts}] {msg}\n")
        self.log_text.see(tk.END)
        self.log_text.config(state='disabled')

    def clear_log(self):
        self.log_text.config(state='normal')
        self.log_text.delete('1.0', tk.END)
        self.log_text.config(state='disabled')

    def set_status(self, text):
        self.status_var.set(text)

    def send_serial(self, data):
        port = self.selected_port.get().strip()
        if not port:
            raise ValueError("请先选择串口")
        ser = serial.Serial(port, BAUDRATE, timeout=2)
        try:
            ser.write(data)
            ser.flush()
            time.sleep(0.1)
        finally:
            ser.close()

    def send_command(self, cmd_byte):
        def _send():
            try:
                self.set_status(f"正在发送命令 0x{cmd_byte:02X} ...")
                self.send_serial(bytes([cmd_byte]))
                self.log(f"命令 0x{cmd_byte:02X} 发送成功")
                self.set_status("就绪")
            except Exception as e:
                self.log(f"发送命令失败: {e}")
                self.set_status("错误")
                messagebox.showerror("错误", f"发送命令失败:\n{e}")

        threading.Thread(target=_send, daemon=True).start()

    def start_pattern(self):
        self.send_command(CMD_START)

    def close_pattern(self):
        self.send_command(CMD_CLOSE)

    def upload_script(self):
        port = self.selected_port.get().strip()
        script = self.script_path.get().strip()

        if not port:
            messagebox.showwarning("警告", "请先选择串口")
            return
        if not script:
            messagebox.showwarning("警告", "请先选择脚本文件")
            return
        if not os.path.exists(script):
            messagebox.showerror("错误", "文件不存在")
            return

        try:
            with open(script, 'r', encoding='utf-8') as f:
                content = f.read()
            data = content.encode('utf-8')
            total_len = len(data)
            if total_len > MAX_SCRIPT_SIZE:
                messagebox.showerror("错误", f"脚本大小 {total_len} 超过最大限制 {MAX_SCRIPT_SIZE} 字节")
                return
        except Exception as e:
            messagebox.showerror("错误", f"读取文件失败:\n{e}")
            return

        def _upload():
            try:
                self.set_status("正在上传脚本...")
                self.disable_buttons()

                ser = serial.Serial(port, BAUDRATE, timeout=5)
                try:
                    # 1. 发送 UPLOAD_START: cmd(1) + total_len(4)
                    ser.write(bytes([CMD_UPLOAD_START]) + struct.pack('<I', total_len))
                    ser.flush()
                    self.log(f"开始上传，总长度 {total_len} 字节")
                    time.sleep(0.05)

                    # 2. 分片发送数据: cmd(1) + offset(4) + chunk_size(2) + data(N)
                    chunk_size = 128
                    offset = 0
                    while offset < total_len:
                        chunk = data[offset:offset+chunk_size]
                        pkt = bytes([CMD_UPLOAD_DATA]) + struct.pack('<I', offset) + struct.pack('<H', len(chunk)) + chunk
                        ser.write(pkt)
                        ser.flush()
                        self.log(f"发送分片: 偏移 {offset}, 长度 {len(chunk)}")
                        offset += len(chunk)
                        time.sleep(0.02)

                    self.log("上传完成，等待设备编译...")
                    self.set_status("上传完成")

                    # 3. 读取设备响应日志
                    ser.timeout = 2
                    start_time = time.time()
                    while time.time() - start_time < 5:
                        if ser.in_waiting > 0:
                            line = ser.readline().decode('utf-8', errors='replace').rstrip()
                            if line:
                                self.log(f"[设备] {line}")
                        else:
                            break
                finally:
                    ser.close()

                messagebox.showinfo("提示", "脚本上传完成。")
            except Exception as e:
                self.log(f"上传失败: {e}")
                self.set_status("错误")
                messagebox.showerror("错误", f"上传脚本失败:\n{e}")
            finally:
                self.enable_buttons()
                self.set_status("就绪")

        threading.Thread(target=_upload, daemon=True).start()

    def disable_buttons(self):
        self.start_btn.config(state='disabled')
        self.close_btn.config(state='disabled')
        self.upload_btn.config(state='disabled')

    def enable_buttons(self):
        self.start_btn.config(state='normal')
        self.close_btn.config(state='normal')
        self.upload_btn.config(state='normal')

    def on_closing(self):
        self.log("正在关闭程序...")
        self.destroy()


if __name__ == "__main__":
    app = App()
    app.mainloop()