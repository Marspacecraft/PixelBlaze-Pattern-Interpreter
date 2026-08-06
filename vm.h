#pragma once

#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ffi.h"
#include "program.h"

namespace pixelblaze_cpp {

class VM {
public:
    VM(std::size_t count);
    virtual ~VM() = default;

    virtual void loadProgram(const Program& program);
    
    virtual void setGridSize(std::size_t w, std::size_t h, std::size_t d = 1);
    virtual double pixelCount() const { return pixel_count_; }

    virtual void beforeRender(double delta_ms);

protected:
    virtual void onPixelColor(std::size_t index, const LedColor& color) = 0;
    virtual void onScriptLog(const std::string& msg) { LOG_SCRIPT("%s", msg.c_str()); }

#if ENABLE_DUMP
    void dump() const;
    void dump_exports() const;
    void dump_storage_vars() const;
    void dump_render_vars() const;
#endif

    VM(const VM&) = delete;
    VM& operator=(const VM&) = delete;

    void renderPixel(std::size_t pixel_index);

private:
    void run();
    void executeInstruction(const Instruction& instr);
    double popStack();
    void pushStack(double v);
    void setVar(const std::string& name, double value);
    double getVar(const std::string& name) const;
    void declVar(const std::string& name);
    
    void callFunction(const std::string& name, const std::vector<double>& args = {});

    void deinit();

#if ENABLE_DUMP
    void dump_vars() const;
    void dump_functions() const;
    void dump_current_frame() const;
    void dump_frametrace() const;
#endif

    std::vector<Instruction> code_;
    std::size_t ip_;
    std::vector<double> stack_;
    std::vector<std::size_t> return_stack_;

    std::map<std::string, double> globals_;
    std::vector<std::map<std::string, double>> locals_;
    std::map<std::string, FunctionDef> functions_;

    std::string before_render_name_;
    std::string render_name_;

    LedColor current_color_;
    bool has_color_;
    double time_ms_;
    double delta_ms_;
    std::size_t pixel_index_;
    double pixel_count_;

    std::size_t grid_width_ = 1;
    std::size_t grid_height_ = 1;
    std::size_t grid_depth_ = 1;

    std::map<std::string, double> storage_;
    std::map<std::string, std::string> storage_str_;
    std::map<std::string, double> exports_;

    std::map<std::string, std::vector<double>> arrays_;
    std::map<double, std::string> string_consts_;

    double string_id_counter_ = 0.0;

    struct CallFrame {
        std::vector<Instruction> code;
        std::size_t ip;
        std::vector<double> stack;
        std::vector<std::size_t> return_stack;

#if ENABLE_DUMP
        void dump() const {
            LOG_DEBUG("=== Pixelblaze Call Frame Dump ===");
            LOG_DEBUG("IP: %zu", ip);
            LOG_DEBUG("Stack: %zu", stack.size());
            for (std::size_t i = 0; i < stack.size(); ++i) {
                LOG_DEBUG("  %zu: %f", i, stack[i]);
            }
            LOG_DEBUG("Return Stack: %zu", return_stack.size());
            for (std::size_t i = 0; i < return_stack.size(); ++i) {
                LOG_DEBUG("  %zu: %zu", i, return_stack[i]);
            }
            LOG_DEBUG("Code: %zu", code.size());
            for (std::size_t i = 0; i < code.size(); ++i) {
                code[i].dump();
            }
        }
#endif
    };
    std::vector<CallFrame> call_frames_;
};

}  // namespace pixelblaze_cpp