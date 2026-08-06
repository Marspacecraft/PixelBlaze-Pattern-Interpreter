#include <algorithm>
#include <cstdint>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_timer.h"
#include <Adafruit_NeoPixel.h>
#include "pixelblaze.h"

namespace pixelblaze_cpp {

namespace {
char log_buffer[256];
}

void log_output_impl(uint8_t level, const char *fmt, va_list args)
{
    const char* tag;
    switch (level)
    {
        case LOG_LEVEL_DEBUG: tag = "[DBG]"; break;
        case LOG_LEVEL_INFO:  tag = "[INF]"; break;
        case LOG_LEVEL_WARN:  tag = "[WRN]"; break;
        case LOG_LEVEL_ERROR: tag = "[ERR]"; break;
        case LOG_LEVEL_SCRIPT: tag = "[SCR]"; break;
        default: tag = "[UNK]"; break;
    }
    vsnprintf(log_buffer, sizeof(log_buffer), fmt, args);
    Serial.print(tag);
    Serial.print(" ");
    Serial.println(log_buffer);
}

void log_print(uint8_t level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_output_impl(level, fmt, ap);
    va_end(ap);
}

}

#define DATA_PIN     48
#define PIXEL_COUNT  32
#define BRIGHTNESS   200

Adafruit_NeoPixel strip(PIXEL_COUNT, DATA_PIN, NEO_GRB + NEO_KHZ800);

using namespace pixelblaze_cpp;

class NeoPixelPixelblaze : public Pixelblaze {
public:
    NeoPixelPixelblaze(Program& program, std::size_t pixel_count = PIXEL_COUNT)
        : Pixelblaze(program, pixel_count), leds_(pixel_count) {}

    void setColor(uint16_t index, const WS2812Color& color) override {
        if (index < leds_.size()) {
            leds_[index] = color;
        }
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

static const char* g_script_source = R"(

export function render(index) {
  var t = time(0.02);
  var pos = index / pixelCount;
  hsv(pos + t, 1, 0.8);
}

)";

void compileTask(void* param) {
    Serial.println("[compileTask] Starting compilation...");
    PixelblazeCompiler compiler;

    Program program = compiler.compile(g_script_source);

    if(compiler.parse_ok())
    {
        pbvm = new NeoPixelPixelblaze(program);
        Serial.println("[compileTask] Loading script...");
        g_compile_done = true;
    } 
    else
    {
        Serial.println("[compileTask] Failed to compile script!");
        g_compile_done = false;
    } 
    
    vTaskDelete(NULL);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    strip.begin();
    strip.setBrightness(BRIGHTNESS);
    strip.clear();
    strip.show();
    delay(500);

    Serial.println("Starting compilation task with large stack (64KB)...");
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
        while (true) { delay(1000); }
    }

    while (!g_compile_done) {
        delay(10);
    }

    Serial.println("Pixelblaze script loaded");
}

void loop() {
    if (pbvm) 
    {
        pbvm->beforeRender(100);
        pbvm->renderFrame();
        pbvm->updateHardware();
    }
    delay(100);
}