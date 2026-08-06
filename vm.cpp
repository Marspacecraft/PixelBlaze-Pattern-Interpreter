#include "pixelblaze.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <random>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace pixelblaze_cpp {

namespace {

std::mt19937& rng() {
    static std::mt19937 engine(0xC0FFEEu);
    return engine;
}

double rand_double(double lo, double hi) {
    std::uniform_real_distribution<double> dist(lo, hi);
    return dist(rng());
}

double noise1D(double x) {
    double xi = std::floor(x);
    double xf = x - xi;
    double a = std::sin(xi * 0.1) * 0.5 + 0.5;
    double b = std::sin((xi + 1.0) * 0.1) * 0.5 + 0.5;
    double t = xf * xf * (3.0 - 2.0 * xf);
    return a + (b - a) * t;
}

double triangleWave(double x) {
    double v = std::fmod(x, 1.0);
    if (v < 0.0) v += 1.0;
    return v < 0.5 ? v * 2.0 : 2.0 - v * 2.0;
}

double sawtoothWave(double x) {
    double v = std::fmod(x, 1.0);
    if (v < 0.0) v += 1.0;
    return v;
}

double squareWave(double x) {
    double v = std::fmod(x, 1.0);
    if (v < 0.0) v += 1.0;
    return v < 0.5 ? 1.0 : 0.0;
}

double lerp_double(double a, double b, double t) {
    return a + (b - a) * t;
}

double map_double(double v, double in_min, double in_max, double out_min, double out_max) {
    if (in_max == in_min) return out_min;
    double t = (v - in_min) / (in_max - in_min);
    t = std::max(0.0, std::min(1.0, t));
    return out_min + (out_max - out_min) * t;
}

double constrain_double(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}

double mix_double(double a, double b, double t) {
    return a * (1.0 - t) + b * t;
}

}  // namespace

VM::VM(std::size_t count) : ip_(0), has_color_(false), time_ms_(0.0), delta_ms_(0.0),
           pixel_index_(0), pixel_count_(static_cast<double>(count)) {}

void VM::deinit() {
 
    code_.clear();
    ip_ = 0;
    stack_.clear();
    return_stack_.clear();
    call_frames_.clear();

    globals_.clear();
    locals_.clear();
    functions_.clear();

    before_render_name_.clear();
    render_name_.clear();
    current_color_ = LedColor{0.0, 0.0, 0.0};
    has_color_ = false;
    time_ms_ = 0.0;
    delta_ms_ = 0.0;
    pixel_index_ = 0;

    arrays_.clear();
    string_consts_.clear();
    string_id_counter_ = 0.0;

}

void VM::loadProgram(const Program& program) {
    LOG_INFO("VM::loadProgram: main_code=%zu instructions, functions=%zu",
             program.main_code.size(), program.functions.size());
    
    deinit();

    for (const auto& kv : program.functions) {
        functions_[kv.first] = kv.second;
        LOG_INFO("  Registered function '%s' (%zu instructions)",
                 kv.first.c_str(), kv.second.code.size());
    }
    before_render_name_ = program.before_render_name;
    render_name_ = program.render_name;

    if (!before_render_name_.empty())
        LOG_INFO("  BeforeRender: '%s'", before_render_name_.c_str());
    if (!render_name_.empty())
        LOG_INFO("  Render: '%s'", render_name_.c_str());

    for (const auto& instr : program.main_code) {
        if (instr.op == Op::DeclVar) {
            globals_[instr.name] = 0.0;
        }
    }
    code_ = program.main_code;
    ip_ = 0;
    for (const auto& kv : exports_) {
        globals_[kv.first] = kv.second;
    }

    string_id_counter_ = 0.0;
    for (auto& instr : code_) {
        if (instr.op == Op::PushString) {
            string_consts_[string_id_counter_] = instr.name;
            instr.index = static_cast<int>(string_id_counter_);
            ++string_id_counter_;
        }
    }
    for (auto& kv : functions_) {
        for (auto& instr : kv.second.code) {
            if (instr.op == Op::PushString) {
                string_consts_[string_id_counter_] = instr.name;
                instr.index = static_cast<int>(string_id_counter_);
                ++string_id_counter_;
            }
        }
    }

    run();

    #if ENABLE_DUMP
        dump();
    #endif
}

void VM::setGridSize(std::size_t w, std::size_t h, std::size_t d) {
    grid_width_ = w;
    grid_height_ = h;
    grid_depth_ = d;
}

void VM::callFunction(const std::string& name, const std::vector<double>& args) {
    auto it = functions_.find(name);
    if (it == functions_.end()) {
        LOG_ERROR("VM::callFunction: function '%s' not found", name.c_str());
        return;
    }

    if (call_frames_.size() >= 256) {
        LOG_ERROR("VM::callFunction: call stack overflow (depth=%zu) calling '%s'",
                  call_frames_.size(), name.c_str());
        return;
    }

    std::vector<double> fn_args = args;
    const auto& params = it->second.params;
    while (fn_args.size() < params.size()) fn_args.push_back(0.0);

    CallFrame frame;
    frame.code = code_;
    frame.ip = ip_;
    frame.stack = stack_;
    frame.return_stack = return_stack_;
    call_frames_.push_back(std::move(frame));

    code_ = it->second.code;
    ip_ = 0;
    stack_.clear();
    return_stack_.clear();
    locals_.push_back(std::map<std::string, double>());

    for (std::size_t i = 0; i < params.size() && i < fn_args.size(); ++i) {
        locals_.back()[params[i]] = fn_args[i];
    }
    for (std::size_t i = fn_args.size(); i < params.size(); ++i) {
        locals_.back()[params[i]] = 0.0;
    }

    run();

    if (!locals_.empty()) locals_.pop_back();
    if (!call_frames_.empty()) {
        CallFrame frame = std::move(call_frames_.back());
        call_frames_.pop_back();
        code_ = std::move(frame.code);
        ip_ = frame.ip;
        stack_ = std::move(frame.stack);
        return_stack_ = std::move(frame.return_stack);
    }
}

void VM::beforeRender(double delta_ms) {
    time_ms_ += delta_ms;
    delta_ms_ = delta_ms;
    if (!before_render_name_.empty()) {
        callFunction(before_render_name_, {delta_ms});
    }
}

void VM::renderPixel(std::size_t pixel_index) {
    pixel_index_ = pixel_index;
    has_color_ = false;
    current_color_ = LedColor{};
    if (!render_name_.empty()) {
        std::string lower_name = render_name_;
        for (auto& c : lower_name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower_name.size() >= 8 && lower_name.compare(lower_name.size() - 8, 8, "render2d") == 0) {
            std::size_t x = pixel_index % grid_width_;
            std::size_t y = grid_height_ > 0 ? pixel_index / grid_width_ : 0;
            callFunction(render_name_, {static_cast<double>(x), static_cast<double>(y)});
        } else if (lower_name.size() >= 8 && lower_name.compare(lower_name.size() - 8, 8, "render3d") == 0) {
            std::size_t x = pixel_index % grid_width_;
            std::size_t y = grid_height_ > 0 ? (pixel_index / grid_width_) % grid_height_ : 0;
            std::size_t z = grid_width_ * grid_height_ > 0 ? pixel_index / (grid_width_ * grid_height_) : 0;
            callFunction(render_name_, {static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)});
        } else {
            callFunction(render_name_, {static_cast<double>(pixel_index)});
        }
    }
    onPixelColor(pixel_index_, current_color_);
}

// void VM::setExportVar(const std::string& name, double value) {
//     exports_[name] = value;
//     globals_[name] = value;
// }

// double VM::getExportVar(const std::string& name) const {
//     auto it = exports_.find(name);
//     return it != exports_.end() ? it->second : 0.0;
// }

void VM::declVar(const std::string& name) {
    if (!locals_.empty()) {
        locals_.back()[name] = 0.0;
    } else {
        globals_[name] = 0.0;
    }
}

void VM::setVar(const std::string& name, double value) {
    if (!locals_.empty()) {
        auto lit = locals_.back().find(name);
        if (lit != locals_.back().end()) {
            locals_.back()[name] = value;
            return;
        }
    }
    auto it = globals_.find(name);
    if (it != globals_.end()) {
        globals_[name] = value;
        return;
    }
    globals_[name] = value;
}

double VM::getVar(const std::string& name) const {
    if (!locals_.empty()) {
        auto it = locals_.back().find(name);
        if (it != locals_.back().end()) return it->second;
    }
    auto it = globals_.find(name);
    return it != globals_.end() ? it->second : 0.0;
}

void VM::pushStack(double v) {
    stack_.push_back(v);
}

double VM::popStack() {
    if (stack_.empty()) return 0.0;
    double v = stack_.back();
    stack_.pop_back();
    return v;
}

// void VM::setStorageValue(const std::string& key, double value) {
//     storage_[key] = value;
// }

// double VM::getStorageValue(const std::string& key) const {
//     auto it = storage_.find(key);
//     return it != storage_.end() ? it->second : 0.0;
// }

// void VM::setStorageStrValue(const std::string& key, const std::string& value) {
//     storage_str_[key] = value;
// }

// std::string VM::getStorageStrValue(const std::string& key) const {
//     auto it = storage_str_.find(key);
//     return it != storage_str_.end() ? it->second : std::string();
// }

void VM::run() {
    std::size_t max_steps = 10000000;
    std::size_t steps = 0;
    while (ip_ < code_.size()) {
        executeInstruction(code_[ip_]);
        if (ip_ >= code_.size()) break;
        if (++steps >= max_steps) {
            LOG_ERROR("VM::run: step limit exceeded (%zu steps), possible infinite loop", max_steps);
            break;
        }
    }
}

void VM::executeInstruction(const Instruction& instr) {
    switch (instr.op) {
        case Op::Push:
            pushStack(static_cast<double>(instr.value));
            ++ip_;
            break;
        case Op::PushString: {
            double str_id = -(static_cast<double>(instr.index) + 1.0);
            pushStack(str_id);
            ++ip_;
            break;
        }
        case Op::GetVar: {
            double v = getVar(instr.name);
            pushStack(v);
            ++ip_;
            break;
        }
        case Op::SetVar: {
            double v = popStack();
            setVar(instr.name, v);
            ++ip_;
            break;
        }
        case Op::DeclVar:
            declVar(instr.name);
            ++ip_;
            break;
        case Op::Pop:
            popStack();
            ++ip_;
            break;
        case Op::Add: {
            double b = popStack();
            double a = popStack();
            pushStack(a + b);
            ++ip_;
            break;
        }
        case Op::Sub: {
            double b = popStack();
            double a = popStack();
            pushStack(a - b);
            ++ip_;
            break;
        }
        case Op::Mul: {
            double b = popStack();
            double a = popStack();
            pushStack(a * b);
            ++ip_;
            break;
        }
        case Op::Div: {
            double b = popStack();
            double a = popStack();
            pushStack(b != 0.0 ? a / b : 0.0);
            ++ip_;
            break;
        }
        case Op::Mod: {
            double b = popStack();
            double a = popStack();
            pushStack(b != 0.0 ? std::fmod(a, b) : 0.0);
            ++ip_;
            break;
        }
        case Op::Neg: {
            double a = popStack();
            pushStack(-a);
            ++ip_;
            break;
        }
        case Op::Eq: {
            double b = popStack();
            double a = popStack();
            pushStack(a == b ? 1.0 : 0.0);
            ++ip_;
            break;
        }
        case Op::Ne: {
            double b = popStack();
            double a = popStack();
            pushStack(a != b ? 1.0 : 0.0);
            ++ip_;
            break;
        }
        case Op::Lt: {
            double b = popStack();
            double a = popStack();
            pushStack(a < b ? 1.0 : 0.0);
            ++ip_;
            break;
        }
        case Op::Le: {
            double b = popStack();
            double a = popStack();
            pushStack(a <= b ? 1.0 : 0.0);
            ++ip_;
            break;
        }
        case Op::Gt: {
            double b = popStack();
            double a = popStack();
            pushStack(a > b ? 1.0 : 0.0);
            ++ip_;
            break;
        }
        case Op::Ge: {
            double b = popStack();
            double a = popStack();
            pushStack(a >= b ? 1.0 : 0.0);
            ++ip_;
            break;
        }
        case Op::Not: {
            double a = popStack();
            pushStack(a == 0.0 ? 1.0 : 0.0);
            ++ip_;
            break;
        }
        case Op::And: {
            double b = popStack();
            double a = popStack();
            pushStack((a != 0.0 && b != 0.0) ? 1.0 : 0.0);
            ++ip_;
            break;
        }
        case Op::Or: {
            double b = popStack();
            double a = popStack();
            pushStack((a != 0.0 || b != 0.0) ? 1.0 : 0.0);
            ++ip_;
            break;
        }
        case Op::Jump:
            ip_ += instr.offset;
            break;
        case Op::JumpIfFalse: {
            double v = popStack();
            if (v == 0.0) {
                ip_ += instr.offset;
            } else {
                ++ip_;
            }
            break;
        }
        case Op::Call: {
            auto it = functions_.find(instr.name);
            if (it == functions_.end()) {
                LOG_ERROR("VM::Call: function '%s' not found", instr.name.c_str());
                ++ip_;
                break;
            }
            if (call_frames_.size() >= 256) {
                LOG_ERROR("VM::Call: call stack overflow (depth=%zu) calling '%s'",
                          call_frames_.size(), instr.name.c_str());
                ++ip_;
                break;
            }
            std::vector<double> args;
            for (std::size_t i = 0; i < it->second.params.size() && !stack_.empty(); ++i) {
                args.insert(args.begin(), stack_.back());
                stack_.pop_back();
            }

            CallFrame frame;
            frame.code = code_;
            frame.ip = ip_ + 1;
            frame.stack = stack_;
            frame.return_stack = return_stack_;
            call_frames_.push_back(std::move(frame));

            code_ = it->second.code;
            ip_ = 0;
            stack_.clear();
            return_stack_.clear();
            locals_.push_back(std::map<std::string, double>());

            const auto& params = it->second.params;
            for (std::size_t i = 0; i < params.size() && i < args.size(); ++i) {
                locals_.back()[params[i]] = args[i];
            }
            for (std::size_t i = args.size(); i < params.size(); ++i) {
                locals_.back()[params[i]] = 0.0;
            }
            break;
        }
        case Op::Return: {
            ip_ = code_.size();
            if (!locals_.empty()) locals_.pop_back();
            if (!call_frames_.empty()) {
                double ret_val = 0.0;
                bool has_ret = false;
                if (!stack_.empty()) {
                    ret_val = stack_.back();
                    has_ret = true;
                }
                CallFrame frame = std::move(call_frames_.back());
                call_frames_.pop_back();
                code_ = std::move(frame.code);
                ip_ = frame.ip;
                stack_ = std::move(frame.stack);
                return_stack_ = std::move(frame.return_stack);
                if (has_ret) {
                    stack_.push_back(ret_val);
                }
            }
            break;
        }
        case Op::Break:
        case Op::Continue:
            LOG_ERROR("VM: unexpected Break/Continue at ip=%zu (should have been patched to jump)", ip_);
            ip_ = code_.size();
            break;
        case Op::Sin: {
            double a = popStack();
            pushStack(std::sin(a));
            ++ip_;
            break;
        }
        case Op::Cos: {
            double a = popStack();
            pushStack(std::cos(a));
            ++ip_;
            break;
        }
        case Op::Tan: {
            double a = popStack();
            pushStack(std::tan(a));
            ++ip_;
            break;
        }
        case Op::Asin: {
            double a = popStack();
            pushStack(std::asin(a));
            ++ip_;
            break;
        }
        case Op::Acos: {
            double a = popStack();
            pushStack(std::acos(a));
            ++ip_;
            break;
        }
        case Op::Atan: {
            double a = popStack();
            pushStack(std::atan(a));
            ++ip_;
            break;
        }
        case Op::Time: {
            double speed = popStack();
            if (speed <= 0.0) {
                pushStack(0.0);
            } else {
                double t = std::fmod(time_ms_, 3600000.0);
                pushStack(std::fmod(t / (speed * 65536.0), 1.0));
            }
            ++ip_;
            break;
        }
        case Op::Wave: {
            double a = popStack();
            pushStack((1.0 + std::sin(a * 2.0 * M_PI)) / 2.0);
            ++ip_;
            break;
        }
        case Op::Triangle: {
            double a = popStack();
            pushStack(triangleWave(a));
            ++ip_;
            break;
        }
        case Op::Sawtooth: {
            double a = popStack();
            pushStack(sawtoothWave(a));
            ++ip_;
            break;
        }
        case Op::Square: {
            double a = popStack();
            pushStack(squareWave(a));
            ++ip_;
            break;
        }
        case Op::Noise1D: {
            double a = popStack();
            pushStack(noise1D(a));
            ++ip_;
            break;
        }
        case Op::Random: {
            pushStack(rand_double(0.0, 1.0));
            ++ip_;
            break;
        }
        case Op::RandomRange: {
            double hi = popStack();
            double lo = popStack();
            pushStack(rand_double(lo, hi));
            ++ip_;
            break;
        }
        case Op::Abs: {
            double a = popStack();
            pushStack(std::fabs(a));
            ++ip_;
            break;
        }
        case Op::Rgb: {
            double b = popStack();
            double g = popStack();
            double r = popStack();
            current_color_.r = r;
            current_color_.g = g;
            current_color_.b = b;
            has_color_ = true;
            ++ip_;
            break;
        }
        case Op::Hsv: {
            double v = popStack();
            double s = popStack();
            double h = popStack();
            double hue = std::fmod(h, 1.0);
            double c = v * s;
            double x = c * (1.0 - std::fabs(std::fmod(hue * 6.0, 2.0) - 1.0));
            double m = v - c;
            double r = 0.0, g = 0.0, b = 0.0;
            if (hue < 1.0/6.0) { r = c; g = x; }
            else if (hue < 2.0/6.0) { r = x; g = c; }
            else if (hue < 3.0/6.0) { g = c; b = x; }
            else if (hue < 4.0/6.0) { g = x; b = c; }
            else if (hue < 5.0/6.0) { r = x; b = c; }
            else { r = c; b = x; }
            current_color_.r = r + m;
            current_color_.g = g + m;
            current_color_.b = b + m;
            has_color_ = true;
            ++ip_;
            break;
        }
        case Op::Okhsl: {
            double l = popStack();
            double s = popStack();
            double h = popStack();
            double hue = std::fmod(h, 1.0);
            double sat = std::max(0.0, std::min(1.0, s));
            double light = std::max(0.0, std::min(1.0, l));
            double c = (1.0 - std::fabs(2.0 * light - 1.0)) * sat;
            double x = c * (1.0 - std::fabs(std::fmod(hue * 6.0, 2.0) - 1.0));
            double m = light - c / 2.0;
            double r = 0.0, g = 0.0, b = 0.0;
            if (hue < 1.0/6.0) { r = c; g = x; }
            else if (hue < 2.0/6.0) { r = x; g = c; }
            else if (hue < 3.0/6.0) { g = c; b = x; }
            else if (hue < 4.0/6.0) { g = x; b = c; }
            else if (hue < 5.0/6.0) { r = x; b = c; }
            else { r = c; b = x; }
            current_color_.r = r + m;
            current_color_.g = g + m;
            current_color_.b = b + m;
            has_color_ = true;
            ++ip_;
            break;
        }
        case Op::Log: {
            std::vector<double> params;
            params.reserve(stack_.size());
            while (!stack_.empty()) {
                params.push_back(stack_.back());
                stack_.pop_back();
            }
            std::reverse(params.begin(), params.end());

            std::string msg;
            for (double v : params) {
                if (v < 0) {
                    int id = -static_cast<int>(v) - 1;
                    auto it = string_consts_.find(static_cast<double>(id));
                    if (it != string_consts_.end()) {
                        msg += it->second;
                    } else {
                        msg += "[unknown]";
                    }
                } else {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%f", v);
                    msg += buf;
                }
                msg += " ";
            }
            if (!msg.empty()) msg.pop_back();
            onScriptLog(msg);
            ++ip_;
            break;
        }
        case Op::GetPixelIndex: {
            pushStack(static_cast<double>(pixel_index_));
            ++ip_;
            break;
        }
        case Op::GetPixelCount: {
            pushStack(pixel_count_);
            ++ip_;
            break;
        }
        case Op::GetPixelX: {
            double x = pixel_index_ % grid_width_;
            pushStack(x);
            ++ip_;
            break;
        }
        case Op::GetPixelY: {
            if (grid_height_ <= 1) {
                pushStack(0.0);
            } else {
                double y = std::fmod(std::floor(pixel_index_ / grid_width_), static_cast<double>(grid_height_));
                pushStack(y);
            }
            ++ip_;
            break;
        }
        case Op::GetPixelZ: {
            if (grid_depth_ <= 1) {
                pushStack(0.0);
            } else {
                double z = std::fmod(std::floor(pixel_index_ / (grid_width_ * grid_height_)), static_cast<double>(grid_depth_));
                pushStack(z);
            }
            ++ip_;
            break;
        }
        case Op::GetGridWidth: {
            pushStack(static_cast<double>(grid_width_));
            ++ip_;
            break;
        }
        case Op::GetGridHeight: {
            pushStack(static_cast<double>(grid_height_));
            ++ip_;
            break;
        }
        case Op::GetGridPixelCount: {
            pushStack(static_cast<double>(grid_width_ * grid_height_ * grid_depth_));
            ++ip_;
            break;
        }
        case Op::GetDelta: {
            pushStack(delta_ms_);
            ++ip_;
            break;
        }
        case Op::Min: {
            double b = popStack();
            double a = popStack();
            pushStack(std::min(a, b));
            ++ip_;
            break;
        }
        case Op::Max: {
            double b = popStack();
            double a = popStack();
            pushStack(std::max(a, b));
            ++ip_;
            break;
        }
        case Op::Clamp: {
            double hi = popStack();
            double lo = popStack();
            double v = popStack();
            pushStack(std::max(lo, std::min(hi, v)));
            ++ip_;
            break;
        }
        case Op::Lerp: {
            double t = popStack();
            double b = popStack();
            double a = popStack();
            pushStack(lerp_double(a, b, t));
            ++ip_;
            break;
        }
        case Op::Map: {
            double out_max = popStack();
            double out_min = popStack();
            double in_max = popStack();
            double in_min = popStack();
            double v = popStack();
            pushStack(map_double(v, in_min, in_max, out_min, out_max));
            ++ip_;
            break;
        }
        case Op::Constrain: {
            double hi = popStack();
            double lo = popStack();
            double v = popStack();
            pushStack(constrain_double(v, lo, hi));
            ++ip_;
            break;
        }
        case Op::Mix: {
            double t = popStack();
            double b = popStack();
            double a = popStack();
            pushStack(mix_double(a, b, t));
            ++ip_;
            break;
        }
        case Op::Floor: {
            double a = popStack();
            pushStack(std::floor(a));
            ++ip_;
            break;
        }
        case Op::Ceil: {
            double a = popStack();
            pushStack(std::ceil(a));
            ++ip_;
            break;
        }
        case Op::Round: {
            double a = popStack();
            pushStack(std::round(a));
            ++ip_;
            break;
        }
        case Op::Pow: {
            double b = popStack();
            double a = popStack();
            pushStack(std::pow(a, b));
            ++ip_;
            break;
        }
        case Op::Sqrt: {
            double a = popStack();
            pushStack(std::sqrt(a));
            ++ip_;
            break;
        }
        case Op::StorageGet: {
            double def = popStack();
            std::string key = instr.name;
            auto it = storage_.find(key);
            pushStack(it != storage_.end() ? it->second : def);
            ++ip_;
            break;
        }
        case Op::StorageSet: {
            double val = popStack();
            std::string key = instr.name;
            storage_[key] = val;
            ++ip_;
            break;
        }
        case Op::StorageGetStr: {
            double def_id = popStack();
            std::string key = instr.name;
            auto it = storage_str_.find(key);
            if (it != storage_str_.end()) {
                double new_id = string_id_counter_;
                string_consts_[new_id] = it->second;
                ++string_id_counter_;
                pushStack(new_id);
            } else {
                pushStack(def_id);
            }
            ++ip_;
            break;
        }
        case Op::StorageSetStr: {
            double str_id = popStack();
            std::string key = instr.name;
            auto it = string_consts_.find(str_id);
            if (it != string_consts_.end()) {
                storage_str_[key] = it->second;
            }
            ++ip_;
            break;
        }
        case Op::Swap: {
            if (stack_.size() >= 2) {
                std::swap(stack_[stack_.size() - 1], stack_[stack_.size() - 2]);
            }
            ++ip_;
            break;
        }
        case Op::Dup: {
            if (!stack_.empty()) {
                pushStack(stack_.back());
            }
            ++ip_;
            break;
        }
        case Op::Rot: {
            if (stack_.size() >= 3) {
                double a = stack_[stack_.size() - 3];
                double b = stack_[stack_.size() - 2];
                double c = stack_[stack_.size() - 1];
                stack_[stack_.size() - 3] = b;
                stack_[stack_.size() - 2] = c;
                stack_[stack_.size() - 1] = a;
            }
            ++ip_;
            break;
        }
        case Op::ArrayGet: {
            double idx = popStack();
            auto it = arrays_.find(instr.name);
            if (it == arrays_.end()) {
                LOG_ERROR("ArrayGet: array '%s' not found", instr.name.c_str());
                pushStack(0.0);
                ++ip_;
                break;
            }
            std::size_t i = static_cast<std::size_t>(idx);
            if (i >= it->second.size()) {
                LOG_ERROR("ArrayGet: index %zu out of bounds for array '%s' (size=%zu)",
                          i, instr.name.c_str(), it->second.size());
                pushStack(0.0);
            } else {
                pushStack(it->second[i]);
            }
            ++ip_;
            break;
        }
        case Op::ArraySet: {
            double idx = popStack();
            double val = popStack();
            auto& arr = arrays_[instr.name];
            std::size_t i = static_cast<std::size_t>(idx);
            if (i >= arr.size()) {
                LOG_ERROR("ArraySet: index %zu out of bounds for array '%s' (size=%zu), resizing",
                          i, instr.name.c_str(), arr.size());
                arr.resize(i + 1, 0.0);
            }
            arr[i] = val;
            ++ip_;
            break;
        }
        case Op::ArrayDecl: {
            double size = popStack();
            arrays_[instr.name].assign(static_cast<std::size_t>(size), 0.0);
            ++ip_;
            break;
        }
        case Op::ArrayLiteral: {
            double count = popStack();
            std::size_t n = static_cast<std::size_t>(count);
            std::string name = instr.name;
            auto& arr = arrays_[name];
            arr.resize(n, 0.0);
            for (std::size_t i = 0; i < n && !stack_.empty(); ++i) {
                arr[n - 1 - i] = popStack();
            }
            ++ip_;
            break;
        }
        default:
            LOG_ERROR("VM: unknown opcode %d at ip=%zu", static_cast<int>(instr.op), ip_);
            ++ip_;
            break;
    }
}
#if ENABLE_DUMP

void VM::dump_render_vars() const
{ 
    LOG_DEBUG("==Render vars :");
    LOG_DEBUG("  current_color_: has[%d] = %d  color[%f, %f, %f]", has_color_, current_color_.r, current_color_.g, current_color_.b);
    LOG_DEBUG("  time_ms_: %f delta_ms_: %f", time_ms_, delta_ms_);
    LOG_DEBUG("  pixel_index_: %zu  pixel_count_: %f", pixel_index_, pixel_count_);
    LOG_DEBUG("  grid_width_: %zu grid_height_: %zu grid_depth_: %zu", grid_width_, grid_height_, grid_depth_);
}

void VM::dump_vars() const
{
    LOG_DEBUG("==Globals  vars(%zu):", globals_.size());
    for (const auto& kv : globals_) {
        LOG_DEBUG("  %s = %f", kv.first.c_str(), kv.second);
    }

    LOG_DEBUG("==Locals  vars(%zu frames):", locals_.size());
    for (std::size_t f = 0; f < locals_.size(); ++f) {
        LOG_DEBUG("  Frame %zu (%zu vars):", f, locals_[f].size());
        for (const auto& kv : locals_[f]) {
            LOG_DEBUG("    %s = %f", kv.first.c_str(), kv.second);
        }
    }

    LOG_DEBUG("==String constants (%zu):", string_consts_.size());
    for (const auto& kv : string_consts_) {
        LOG_DEBUG("  id=%f -> \"%s\"", kv.first, kv.second.c_str());
    }

    LOG_DEBUG("==Arrays (%zu):", arrays_.size());
    for (const auto& kv : arrays_) {
        LOG_DEBUG("  %s (%zu): [", kv.first.c_str(), kv.second.size());
        for (std::size_t i = 0; i < kv.second.size() && i < 10; ++i) {
            if (i > 0) LOG_DEBUG(", ");
            LOG_DEBUG("%f", kv.second[i]);
        }
        if (kv.second.size() > 10) LOG_DEBUG(", ...");
        LOG_DEBUG("]");
    } 
}

void VM::dump_storage_vars() const
{
    LOG_DEBUG("==Storage (numeric, %zu):", storage_.size());
    for (const auto& kv : storage_) {
        LOG_DEBUG("  %s = %f", kv.first.c_str(), kv.second);
    }

    LOG_DEBUG("==Storage (string, %zu):", storage_str_.size());
    for (const auto& kv : storage_str_) {
        LOG_DEBUG("  %s = \"%s\"", kv.first.c_str(), kv.second.c_str());
    }
}

void VM::dump_exports() const
{
    LOG_DEBUG("==Exports (%zu):", exports_.size());
    for (const auto& kv : exports_) {
        LOG_DEBUG("  %s = %f", kv.first.c_str(), kv.second);
    }
}

void VM::dump_functions() const
{
    LOG_DEBUG("==Functions (%zu):", functions_.size());
    for (const auto& kv : functions_) {
        LOG_DEBUG("  name: %s", kv.first.c_str());
        kv.second.dump();
    }
    LOG_DEBUG("  BeforeRender: %s", before_render_name_.empty() ? "(none)" : before_render_name_.c_str());
    LOG_DEBUG("  Render: %s", render_name_.empty() ? "(none)" : render_name_.c_str());
}

void VM::dump_current_frame() const
{
    LOG_DEBUG("==Current frame");

    LOG_DEBUG("  Instruction: ip=%zu", ip_);
    for (std::size_t i = 0; i < code_.size(); ++i) {
        code_[i].dump();
    }

    LOG_DEBUG("  Stack (%zu items):", stack_.size());
    for (std::size_t i = 0; i < stack_.size(); ++i) {
        LOG_DEBUG("    [%zu] %f", i, stack_[i]);
    }

    LOG_DEBUG(" Return stack (%zu items):", return_stack_.size());
    for (std::size_t i = 0; i < return_stack_.size(); ++i) {
        LOG_DEBUG("    [%zu] ip=%zu", i, return_stack_[i]);
    }
}

void VM::dump_frametrace() const
{ 
    LOG_DEBUG("==Call frames (%zu):", call_frames_.size());
    for (std::size_t f = 0; f < call_frames_.size(); ++f) {
        call_frames_[f].dump();
    }
}


void VM::dump() const {
    LOG_DEBUG("=== VM Dump ===");
    
    dump_vars();
    dump_exports();
    dump_storage_vars();
    dump_render_vars();
    dump_functions();
    dump_current_frame();
    dump_frametrace();

    LOG_DEBUG("=== End VM Dump ===");
}
#endif
}  // namespace pixelblaze_cpp