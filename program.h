#pragma once

#include <map>
#include <string>
#include <vector>

#include "ffi.h"

namespace pixelblaze_cpp {
struct LedColor {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
};

struct WS2812Color
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

enum class Op {
    Push,
    GetVar,
    SetVar,
    DeclVar,
    Pop,
    Swap,
    Dup,
    Rot,
    Add, Sub, Mul, Div, Mod, Neg,
    Eq, Ne, Lt, Le, Gt, Ge, Not,
    And, Or,
    Jump,
    JumpIfFalse,
    Call,
    Return,
    Break,
    Continue,
    Sin,
    Cos,
    Tan,
    Asin,
    Acos,
    Atan,
    Wave,
    Triangle,
    Sawtooth,
    Square,
    Noise1D,
    Random,
    RandomRange,
    Time,
    Abs,
    Rgb,
    Hsv,
    Okhsl,
    Log,
    GetPixelIndex,
    GetPixelCount,
    GetPixelX,
    GetPixelY,
    GetPixelZ,
    GetGridWidth,
    GetGridHeight,
    GetGridPixelCount,
    GetDelta,
    Min,
    Max,
    Floor,
    Ceil,
    Round,
    Pow,
    Sqrt,
    Clamp,
    StorageGet,
    StorageSet,
    StorageGetStr,
    StorageSetStr,
    Lerp,
    Map,
    Constrain,
    Mix,
    ArrayGet,
    ArraySet,
    ArrayDecl,
    ArrayLiteral,
    PushString,
};
const char* opToString(Op op);

struct Instruction {
    Op op;
    double value;
    std::string name;
    int offset;
    int index;

    Instruction() : op(Op::Push), value(0.0), offset(0), index(0) {}

    static Instruction push(double v) {
        Instruction i; i.op = Op::Push; i.value = v; return i;
    }
    static Instruction getVar(const std::string& n) {
        Instruction i; i.op = Op::GetVar; i.name = n; return i;
    }
    static Instruction setVar(const std::string& n) {
        Instruction i; i.op = Op::SetVar; i.name = n; return i;
    }
    static Instruction declVar(const std::string& n) {
        Instruction i; i.op = Op::DeclVar; i.name = n; return i;
    }
    static Instruction jump(int off) {
        Instruction i; i.op = Op::Jump; i.offset = off; return i;
    }
    static Instruction jumpIfFalse(int off) {
        Instruction i; i.op = Op::JumpIfFalse; i.offset = off; return i;
    }
    static Instruction call(const std::string& n) {
        Instruction i; i.op = Op::Call; i.name = n; return i;
    }
    static Instruction ret() {
        Instruction i; i.op = Op::Return; return i;
    }
    static Instruction pop() {
        Instruction i; i.op = Op::Pop; return i;
    }
    static Instruction swap() {
        Instruction i; i.op = Op::Swap; return i;
    }
    static Instruction dup() {
        Instruction i; i.op = Op::Dup; return i;
    }
    static Instruction rot() {
        Instruction i; i.op = Op::Rot; return i;
    }
    static Instruction arrayGet(const std::string& n) {
        Instruction i; i.op = Op::ArrayGet; i.name = n; return i;
    }
    static Instruction arraySet(const std::string& n) {
        Instruction i; i.op = Op::ArraySet; i.name = n; return i;
    }
    static Instruction arrayDecl(const std::string& n) {
        Instruction i; i.op = Op::ArrayDecl; i.name = n; return i;
    }
    static Instruction makeOp(Op o) {
        Instruction i; i.op = o; return i;
    }
    static Instruction makeString(const std::string& s) {
        Instruction i; i.op = Op::PushString; i.name = s; return i;
    }
    static Instruction arrayLiteral(const std::string& n = "") {
        Instruction i; i.op = Op::ArrayLiteral; i.name = n; return i;
    }

#if ENABLE_DUMP
    void dump() const
    { 
        LOG_DEBUG("=== Pixelblaze Instruction Dump ===");
        LOG_DEBUG("%s", opToString(op));
        if (op == Op::Push) LOG_DEBUG("value: %f", value);
        if (op == Op::Jump || op == Op::JumpIfFalse) LOG_DEBUG("offset: %d", offset);
        if (!name.empty()) LOG_DEBUG("name: %s", name.c_str());
        LOG_DEBUG("index: %d", index);
    }
#endif
};

struct FunctionDef {
    std::string name;
    std::vector<std::string> params;
    std::string body_source;
    std::vector<Instruction> code;
#if ENABLE_DUMP
    void dump() const;
#endif
};

struct Program {
    std::vector<Instruction> main_code;
    std::map<std::string, FunctionDef> functions;
    std::string before_render_name;
    std::string render_name;
    std::vector<std::string> export_vars;
    std::vector<std::string> export_functions;

#if ENABLE_DUMP
    void dump() const;
#endif
};


}  // namespace pixelblaze_cpp