#include "pixelblaze.h"

#include <sstream>

namespace pixelblaze_cpp {

const char* opToString(Op op) {
    switch (op) {
        case Op::Push: return "Push";
        case Op::GetVar: return "GetVar";
        case Op::SetVar: return "SetVar";
        case Op::DeclVar: return "DeclVar";
        case Op::Pop: return "Pop";
        case Op::Swap: return "Swap";
        case Op::Dup: return "Dup";
        case Op::Rot: return "Rot";
        case Op::Add: return "Add";
        case Op::Sub: return "Sub";
        case Op::Mul: return "Mul";
        case Op::Div: return "Div";
        case Op::Mod: return "Mod";
        case Op::Neg: return "Neg";
        case Op::Eq: return "Eq";
        case Op::Ne: return "Ne";
        case Op::Lt: return "Lt";
        case Op::Le: return "Le";
        case Op::Gt: return "Gt";
        case Op::Ge: return "Ge";
        case Op::Not: return "Not";
        case Op::And: return "And";
        case Op::Or: return "Or";
        case Op::Jump: return "Jump";
        case Op::JumpIfFalse: return "JumpIfFalse";
        case Op::Call: return "Call";
        case Op::Return: return "Return";
        case Op::Break: return "Break";
        case Op::Continue: return "Continue";
        case Op::Sin: return "Sin";
        case Op::Cos: return "Cos";
        case Op::Tan: return "Tan";
        case Op::Asin: return "Asin";
        case Op::Acos: return "Acos";
        case Op::Atan: return "Atan";
        case Op::Wave: return "Wave";
        case Op::Triangle: return "Triangle";
        case Op::Sawtooth: return "Sawtooth";
        case Op::Square: return "Square";
        case Op::Noise1D: return "Noise1D";
        case Op::Random: return "Random";
        case Op::RandomRange: return "RandomRange";
        case Op::Time: return "Time";
        case Op::Abs: return "Abs";
        case Op::Rgb: return "Rgb";
        case Op::Hsv: return "Hsv";
        case Op::Okhsl: return "Okhsl";
        case Op::Log: return "Log";
        case Op::GetPixelIndex: return "GetPixelIndex";
        case Op::GetPixelCount: return "GetPixelCount";
        case Op::GetPixelX: return "GetPixelX";
        case Op::GetPixelY: return "GetPixelY";
        case Op::GetPixelZ: return "GetPixelZ";
        case Op::GetGridWidth: return "GetGridWidth";
        case Op::GetGridHeight: return "GetGridHeight";
        case Op::GetGridPixelCount: return "GetGridPixelCount";
        case Op::GetDelta: return "GetDelta";
        case Op::Min: return "Min";
        case Op::Max: return "Max";
        case Op::Floor: return "Floor";
        case Op::Ceil: return "Ceil";
        case Op::Round: return "Round";
        case Op::Pow: return "Pow";
        case Op::Sqrt: return "Sqrt";
        case Op::Clamp: return "Clamp";
        case Op::StorageGet: return "StorageGet";
        case Op::StorageSet: return "StorageSet";
        case Op::StorageGetStr: return "StorageGetStr";
        case Op::StorageSetStr: return "StorageSetStr";
        case Op::Lerp: return "Lerp";
        case Op::Map: return "Map";
        case Op::Constrain: return "Constrain";
        case Op::Mix: return "Mix";
        case Op::ArrayGet: return "ArrayGet";
        case Op::ArraySet: return "ArraySet";
        case Op::ArrayDecl: return "ArrayDecl";
        case Op::ArrayLiteral: return "ArrayLiteral";
        case Op::PushString: return "PushString";
    }
    return "Unknown";
}


#if ENABLE_DUMP
void FunctionDef::dump() const {
    LOG_DEBUG("=== Pixelblaze Function Dump ===");
    LOG_DEBUG("Function '%s' (%zu instructions, %zu params):",
        name.c_str(), code.size(), params.size());
    
    for (const auto& param : params) LOG_DEBUG("  Param: %s", param.c_str());
    for (std::size_t i = 0; i < code.size(); ++i) {
        const auto& instr = code[i];
        std::string line = std::string("  [") + std::to_string(i) + "] " + opToString(instr.op);
        if (instr.op == Op::Push) line += " " + std::to_string(instr.value);
        if (!instr.name.empty()) line += " '" + instr.name + "'";
        if (instr.op == Op::Jump || instr.op == Op::JumpIfFalse) {
            line += " -> " + std::to_string(static_cast<int>(i) + instr.offset);
        }
        LOG_DEBUG("%s", line.c_str());  
    }
}


void Program::dump() const {
    LOG_DEBUG("=== Pixelblaze Program Dump ===");
    LOG_DEBUG("Main code (%zu instructions):", main_code.size());
    for (std::size_t i = 0; i < main_code.size(); ++i) {
        main_code[i].dump();
    }

    for (const auto& pair : functions) {
        LOG_DEBUG("Function '%s': %zu instructions, %zu params",
            pair.first.c_str(), pair.second.code.size(), pair.second.params.size());
        pair.second.dump();
    }

    if (!before_render_name.empty())
        LOG_DEBUG("BeforeRender: %s", before_render_name.c_str());
    if (!render_name.empty())
        LOG_DEBUG("Render: %s", render_name.c_str());

    if (!export_vars.empty()) {
        std::string vars;
        for (const auto& v : export_vars) vars += v + " ";
        LOG_DEBUG("Export vars: %s", vars.c_str());
    }
    if (!export_functions.empty()) {
        std::string funcs;
        for (const auto& f : export_functions) funcs += f + " ";
        LOG_DEBUG("Export functions: %s", funcs.c_str());
    }

}
#endif
}  // namespace pixelblaze_cpp