#pragma once

#include <string>

#include "compiler.h"
#include "runtime.h"

namespace pixelblaze_cpp {

class Pixelblaze : public PixelblazeRuntime {
public:
    ~Pixelblaze() override = default;

    explicit Pixelblaze(std::size_t pixel_count = 16)
        : PixelblazeRuntime(pixel_count) { }
    
    bool begin(const Program& program){ VM::loadProgram(program); return true; }

    void setGridSize(std::size_t w, std::size_t h, std::size_t d = 1) override { VM::setGridSize(w, h, d); }

    std::size_t getPixelCount() const { return static_cast<std::size_t>(VM::pixelCount()); }
    virtual void setColor(uint16_t index, const WS2812Color& color) = 0;
    virtual WS2812Color& getColor(uint16_t index) const = 0;

    void beforeRender(double delta_ms) override { VM::beforeRender(delta_ms); }
    void renderFrame() override { PixelblazeRuntime::renderFrame(); }

};

}  // namespace pixelblaze_cpp