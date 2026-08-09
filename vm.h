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
    virtual double pixelCount() const { return pixel_count_; }
    virtual void beforeRender(double delta_ms);

    /**
     * @brief Read a dynamic variable, called by VM when executing GetVar opcode
     *
     * Flow: VM::getVar -> Registry.lookup(dynamic) -> virtual getDynamicVarValue -> subclass implements
     *
     * @param name Variable name to read
     * @return Current value of the variable
     */
    virtual NativeValue getDynamicVarValue(const std::string& name) const { return NativeValue(); }

    /**
     * @brief Write a dynamic variable, called by VM when executing SetVar opcode
     *
     * Flow: VM::setVar -> Registry.lookup(dynamic) -> virtual setDynamicVarValue -> subclass implements
     *
     * @param name Variable name to write
     * @param value New value for the variable
     */
    virtual void setDynamicVarValue(const std::string& name, const NativeValue& value) { }

    /**
     * @brief Call a dynamic function, called by VM when executing Call opcode
     *
     * Flow: VM::executeInstruction(Call) -> Registry.lookup(dynamic) -> virtual callFunctionDynamic -> subclass implements
     *
     * @param name Function name to call
     * @param args Arguments to pass to the function
     * @return The return value of the function call
     */
    virtual NativeValue callFunctionDynamic(const std::string& name, const std::vector<NativeValue>& args) { return NativeValue(); }

    virtual void setGridSize(std::size_t w, std::size_t h, std::size_t d = 1);
    
protected:
    virtual void onPixelColor(std::size_t index, const LedColor& color) = 0;
    virtual void onScriptLog(const std::string& msg) { PBZ_SCRIPT("%s", msg.c_str()); }

    virtual double onStorageGet(const std::string& key, double def) = 0;
    virtual void onStorageSet(const std::string& key, double value) = 0;
    virtual std::string onStorageGetStr(const std::string& key, const std::string& def) = 0;
    virtual void onStorageSetStr(const std::string& key, const std::string& value) = 0;

#if ENABLE_DUMP
    void dump() const;
    void dump_exports() const;
    virtual void dump_storage_vars() const;
    void dump_render_vars() const;
#endif

    VM(const VM&) = delete;
    VM& operator=(const VM&) = delete;

    void renderPixel(std::size_t pixel_index);

    double stackPop();
    void stackPush(double v);
    std::string resolveString(double id);
    void logMessage(const std::string& msg);
	
    void setVar(const std::string& name, double value);
    double getVar(const std::string& name) const;
    void declVar(const std::string& name);

private:
    void run();
    void executeInstruction(const Instruction& instr);
    double popStack();
    void pushStack(double v);

    
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
            PBZ_DEBUG("=== Pixelblaze Call Frame Dump ===");
            PBZ_DEBUG("IP: %zu", ip);
            PBZ_DEBUG("Stack: %zu", stack.size());
            for (std::size_t i = 0; i < stack.size(); ++i) {
                PBZ_DEBUG("  %zu: %f", i, stack[i]);
            }
            PBZ_DEBUG("Return Stack: %zu", return_stack.size());
            for (std::size_t i = 0; i < return_stack.size(); ++i) {
                PBZ_DEBUG("  %zu: %zu", i, return_stack[i]);
            }
            PBZ_DEBUG("Code: %zu", code.size());
            for (std::size_t i = 0; i < code.size(); ++i) {
                code[i].dump();
            }
        }
#endif
    };
    std::vector<CallFrame> call_frames_;
};

}  // namespace pixelblaze_cpp