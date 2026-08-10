#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_timer.h"
#include <Adafruit_NeoPixel.h>
#include "pixelblaze.h"

// ================== 日志 ==================
namespace pixelblaze_cpp {
namespace {
char log_buffer[256];
}
void log_print(uint8_t level, const char *fmt, va_list args) {
    const char* tag;
    switch (level) {
        case PBZ_LOG_LEVEL_DEBUG: tag = "[DBG]"; break;
        case PBZ_LOG_LEVEL_INFO:  tag = "[INF]"; break;
        case PBZ_LOG_LEVEL_WARN:  tag = "[WRN]"; break;
        case PBZ_LOG_LEVEL_ERROR: tag = "[ERR]"; break;
        case PBZ_LOG_LEVEL_SCRIPT: tag = "[SCR]"; break;
        default: tag = "[UNK]"; break;
    }
    vsnprintf(log_buffer, sizeof(log_buffer), fmt, args);
    Serial.print(tag);
    Serial.print(" ");
    Serial.println(log_buffer);
}
void pbz_log_print(uint8_t level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_print(level, fmt, ap);
    va_end(ap);
}
}
using namespace pixelblaze_cpp;

// ================== LED 配置 ==================
#define DATA_PIN     4
#define PIXEL_COUNT  32
#define BRIGHTNESS   100
Adafruit_NeoPixel strip(PIXEL_COUNT, DATA_PIN, NEO_GRB + NEO_KHZ800);

// ================== Pixelblaze 输出类 ==================
class NeoPixelPixelblaze : public Pixelblaze {
public:
    NeoPixelPixelblaze(Program& program, std::size_t pixel_count = PIXEL_COUNT)
        : Pixelblaze(pixel_count), leds_(pixel_count) {
        begin(program);
    }
    void setColor(uint16_t index, const WS2812Color& color) override {
        if (index < leds_.size()) leds_[index] = color;
    }
    WS2812Color& getColor(uint16_t index) const override {
        return const_cast<std::vector<WS2812Color>&>(leds_)[index];
    }
    void updateHardware() {
        for (std::size_t i = 0; i < leds_.size(); ++i) {
            strip.setPixelColor(static_cast<uint16_t>(i),
                leds_[i].r, leds_[i].g, leds_[i].b);
        }
        strip.show();
    }
private:
    mutable std::vector<WS2812Color> leds_;
};

static NeoPixelPixelblaze* pbvm = nullptr;
static volatile bool g_compile_done = false;
static volatile bool g_flash_led = true;

// ================== 模式编译任务 ==================
static char* g_pending_pattern = nullptr;
static volatile bool g_pattern_requested = false;

void patternTask(void* param) {
    static PixelblazeCompiler compiler;
    static Program program;
    while (true) {
        if (g_pattern_requested) {
            g_pattern_requested = false;
            if (g_pending_pattern) {
                Serial.println("[patternTask] Compiling new pattern...");
                program = compiler.compile(g_pending_pattern);
                if (compiler.parse_ok()) {
                    Serial.println("[patternTask] Pattern loaded successfully");
                    if (pbvm) {
                        pbvm->begin(program);
                        g_flash_led = true;
                    }
                } else {
                    Serial.println("[patternTask] Failed to compile pattern!");
                }
                delete[] g_pending_pattern;
                g_pending_pattern = nullptr;
            }
        }
        delay(50);
    }
}

// ================== 默认脚本 ==================
// static const char* g_script_source = R"(
// export function render(index) {
//   var t = time(0.02);
//   var pos = index / pixelCount;
//   hsv(pos + t, 1, 0.8);
// }
// )";

static const char* g_script_source = R"(
export var speed = 0.01;

function beforeRender(delta) {
  t = time(speed);
  //log("t =", t, "speed =", speed);
}

function render(index) {
    
  if (t < 0.5) {
    //log("t =", t, "index =", index)
    hsv(0, 1, 1);   // 红
  } else {
    //log("t =", t, "index =", index)
    hsv(0.6, 1, 1); // 蓝
  }
}
)";

// ================== 编译任务 ==================
void compileTask(void* param) {
    Serial.println("[compileTask] Starting compilation...");
    PixelblazeCompiler compiler;
    Program program = compiler.compile(g_script_source);
    if(compiler.parse_ok()) {
        pbvm = new NeoPixelPixelblaze(program);
        Serial.println("[compileTask] Script loaded.");
        g_compile_done = true;
        g_flash_led = true;
    } else {
        Serial.println("[compileTask] Failed to compile script!");
        g_compile_done = false;
    }
    vTaskDelete(NULL);
}

// ================== 控制函数 ==================
void ClosePattern() {
    g_flash_led = false;
    strip.clear();
    strip.show();
}
void startPattern() {
    g_flash_led = true;
}
void testPattern(const char* pattern) {
    if(!pbvm) {
        Serial.println("testPattern: pbvm is null");
        return;
    }
    size_t len = strlen(pattern);
    g_pending_pattern = new char[len + 1];
    memcpy(g_pending_pattern, pattern, len + 1);
    g_pattern_requested = true;
    Serial.printf("testPattern: %u bytes queued for compilation\n%s", (unsigned)len, pattern);
}

// ================== 简单串口协议解析 ==================
enum {
    CMD_START = 0x01,
    CMD_CLOSE = 0x02,
    CMD_UPLOAD_START = 0x03,
    CMD_UPLOAD_DATA  = 0x04
};

#define MAX_SCRIPT_SIZE 8192
static uint8_t script_buffer[MAX_SCRIPT_SIZE];
static uint32_t script_total_len = 0;
static uint32_t script_received_len = 0;
static bool receiving_script = false;

// 解析状态机
enum ParseState {
    PARSE_WAIT_CMD,
    PARSE_WATCH_LEN,
    PARSE_WATCH_OFFSET,
    PARSE_WATCH_CHUNK_SIZE,
    PARSE_WATCH_DATA
};

static ParseState parse_state = PARSE_WAIT_CMD;
static uint8_t  parse_cmd = 0;
static uint32_t parse_field = 0;
static uint8_t  parse_byte_idx = 0;
static uint16_t parse_data_len = 0;
static uint32_t parse_offset = 0;

void parseSerial() {
    while (Serial.available() > 0) {
        uint8_t b = (uint8_t)Serial.read();

        switch (parse_state) {
            case PARSE_WAIT_CMD: {
                parse_cmd = b;
                parse_byte_idx = 0;
                parse_field = 0;

                if (b == CMD_START) {
                    startPattern();
                    Serial.println("Start command received");
                } else if (b == CMD_CLOSE) {
                    ClosePattern();
                    Serial.println("Close command received");
                } else if (b == CMD_UPLOAD_START) {
                    parse_state = PARSE_WATCH_LEN;
                } else if (b == CMD_UPLOAD_DATA) {
                    parse_state = PARSE_WATCH_OFFSET;
                } else {
                    Serial.printf("Unknown command: 0x%02X\n", b);
                }
                break;
            }

            case PARSE_WATCH_LEN: {
                parse_field |= ((uint32_t)b) << (parse_byte_idx * 8);
                parse_byte_idx++;
                if (parse_byte_idx >= 4) {
                    if (parse_field > MAX_SCRIPT_SIZE) {
                        Serial.printf("Script too large: %u > %u\n", (unsigned)parse_field, MAX_SCRIPT_SIZE);
                        parse_state = PARSE_WAIT_CMD;
                        break;
                    }
                    script_total_len = parse_field;
                    script_received_len = 0;
                    receiving_script = true;
                    Serial.printf("Receiving script, total %u bytes\n", (unsigned)script_total_len);
                    parse_state = PARSE_WAIT_CMD;
                }
                break;
            }

            case PARSE_WATCH_OFFSET: {
                parse_field |= ((uint32_t)b) << (parse_byte_idx * 8);
                parse_byte_idx++;
                if (parse_byte_idx >= 4) {
                    parse_offset = parse_field;
                    if (parse_offset != script_received_len) {
                        Serial.printf("Offset mismatch: expected %u, got %u\n",
                                      (unsigned)script_received_len, (unsigned)parse_offset);
                        receiving_script = false;
                        parse_state = PARSE_WAIT_CMD;
                        break;
                    }
                    parse_byte_idx = 0;
                    parse_field = 0;
                    parse_state = PARSE_WATCH_CHUNK_SIZE;
                }
                break;
            }

            case PARSE_WATCH_CHUNK_SIZE: {
                parse_field |= ((uint16_t)b) << (parse_byte_idx * 8);
                parse_byte_idx++;
                if (parse_byte_idx >= 2) {
                    parse_data_len = (uint16_t)parse_field;
                    parse_byte_idx = 0;
                    if (parse_offset + parse_data_len > script_total_len) {
                        Serial.println("Data exceeds total length");
                        receiving_script = false;
                        parse_state = PARSE_WAIT_CMD;
                        break;
                    }
                    parse_state = PARSE_WATCH_DATA;
                }
                break;
            }

            case PARSE_WATCH_DATA: {
                script_buffer[parse_offset + parse_byte_idx] = b;
                parse_byte_idx++;
                if (parse_byte_idx >= parse_data_len) {
                    script_received_len += parse_byte_idx;
                    Serial.printf("Received chunk offset %u, len %u, total %u/%u\n",
                                  (unsigned)parse_offset, (unsigned)parse_byte_idx,
                                  (unsigned)script_received_len, (unsigned)script_total_len);
                    parse_state = PARSE_WAIT_CMD;

                    if (script_received_len >= script_total_len) {
                        receiving_script = false;
                        script_buffer[script_total_len] = '\0';
                        Serial.println("Script fully received, compiling...");
                        testPattern((const char*)script_buffer);
                    }
                }
                break;
            }

            default:
                parse_state = PARSE_WAIT_CMD;
                break;
        }
    }
}

// ================== Setup ==================
void setup() {
    Serial.begin(115200);
    delay(1000);

    strip.begin();
    strip.setBrightness(BRIGHTNESS);
    strip.clear();
    strip.show();

    Serial.println("Starting compilation task...");
    TaskHandle_t taskHandle = nullptr;
    BaseType_t result = xTaskCreatePinnedToCore(
        compileTask,
        "compileTask",
        65536,
        NULL,
        1,
        &taskHandle,
        0
    );
    if (result != pdPASS) {
        Serial.println("Failed to create compile task!");
        while (true) delay(1000);
    }
    while (!g_compile_done) delay(10);

    result = xTaskCreatePinnedToCore(
        patternTask,
        "patternTask",
        65536,
        NULL,
        1,
        NULL,
        0
    );
    if (result != pdPASS) {
        Serial.println("Failed to create pattern task!");
        while (true) delay(1000);
    }

    Serial.println("System ready. Commands: start, close, upload via raw serial.");
}

// ================== Loop ==================
void loop() {
    parseSerial();

    if (pbvm && g_flash_led) {
        pbvm->beforeRender(1000.0f / 120.0f);
        pbvm->renderFrame();
        pbvm->updateHardware();
    }

    delay(7);
}