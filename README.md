# Pixelblaze C++ Compiler & VM

这是一个用 C++17 编写的 **Pixelblaze 脚本编译器与虚拟机**，可以解析、编译和执行 Pixelblaze 风格的 JavaScript 代码，并驱动 LED 像素阵列。它完全独立，无外部依赖，只需实现适当的接口即可使用。

## 📦 项目结构

.
├── compiler.h/cpp     # 编译器实现（递归下降解析 + 代码生成）
├── vm.h/cpp           # 虚拟机实现（指令解释器）
├── program.h/cpp      # 程序数据结构（指令、函数定义等）
├── runtime.h          # 运行时基类（LED 颜色转换、帧渲染）
├── pixelblaze.h       # 顶层集成类（继承 Runtime + VM）
├── ffi.h              # 日志宏和打印接口
└── README.md          # 本文档

## 🚀 快速开始

### 1.定义日志输出

实现`log_print`日志输出功能，ppi日志和pattern日志都是用该日志输出。

```cpp
namespace pixelblaze_cpp {

void log_print(uint8_t level, const char *fmt, ...) {
    const char* tag;
    switch (level) {
        case LOG_LEVEL_DEBUG: tag = "[DBG]"; break;
        case LOG_LEVEL_INFO:  tag = "[INF]"; break;
        case LOG_LEVEL_WARN:  tag = "[WRN]"; break;
        case LOG_LEVEL_ERROR: tag = "[ERR]"; break;
        case LOG_LEVEL_SCRIPT: tag = "[SCR]"; break;
        default: tag = "[UNK]"; break;
    }
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, "%s ", tag);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fprintf(stdout, "\n");
}

}
```

### 2. 实现颜色输出回调

继承 `Pixelblaze` 并实现 `setColor()` 和 `getColor()`

```cpp
class MyLEDStrip : public pixelblaze_cpp::Pixelblaze {
public:
    MyLEDStrip(Program& prog, size_t num_pixels)
        : Pixelblaze(prog, num_pixels), colors_(num_pixels) {}

    void setColor(uint16_t index, const WS2812Color& color) override {
        colors_[index] = color;
    }
    WS2812Color& getColor(uint16_t index) const override {
        return colors_[index];
    }

private:
    mutable std::vector<WS2812Color> colors_;
};
```

### 3. 加载pattern及Pixelblaze初始化

```cpp
pixelblaze_cpp::PixelblazeCompiler compiler;
pixelblaze_cpp::Program program = compiler.compile(source);
if (!compiler.parse_ok()) {
    std::cerr << "Failed to compile script.\n";
    return 1;
}
TestPixelblaze vm(program, pixelCount);
```

### 3. 循环渲染帧

```cpp
vm.beforeRender(1000.0f/120.0f);
vm.renderFrame();
```

### 4. 例程

参考arduino程序`PixelBlaze-Pattern-Interpreter.ino`或测试程序`test/main.cpp`

## 🧪 测试

`test`目录包含一个简单的测试框架，用于验证编译器和 VM 行为。
