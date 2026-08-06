#pragma once

#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "vm.h"
#include "program.h"

namespace pixelblaze_cpp {

class PixelblazeRuntime : public VM {
public:
    ~PixelblazeRuntime() override = default;

    explicit PixelblazeRuntime(std::size_t pixel_count = 16) : 
        VM(pixel_count) {}

    virtual void renderFrame() {
        for (std::size_t i = 0; i < static_cast<std::size_t>(VM::pixelCount()); ++i) {
            VM::renderPixel(i);
        }
    }

    virtual void setColor(uint16_t index, const WS2812Color& color) = 0;
    virtual WS2812Color& getColor(uint16_t index) const = 0;

protected:
    void onPixelColor(std::size_t index, const LedColor& color) override {
        WS2812Color c;
        c.r = toByte(color.r);
        c.g = toByte(color.g);
        c.b = toByte(color.b);
        setColor(static_cast<uint16_t>(index), c);
    }

    void onScriptLog(const std::string& msg) override {
        LOG_SCRIPT("%s", msg.c_str());
    }

private:
    static uint8_t toByte(double v) {
        return static_cast<uint8_t>(std::max(0.0, std::min(1.0, v)) * 255.0 + 0.5);
    }
};

}  // namespace pixelblaze_cpp