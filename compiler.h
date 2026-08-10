#pragma once

#include <functional>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>

#include "program.h"

namespace pixelblaze_cpp {

class PixelblazeCompiler;

class NativeFunctionRegistry {
public:
    static NativeFunctionRegistry& instance();

    // Flow: Host registers -> Compiler type-checks -> VM executes CallNative -> invokes C++ func
    /**
     * @brief Register a native C++ function
     * @param name Function name as used in Pixelblaze source code
     * @param func C++ callable that implements the function
     * @param types Optional parameter types for compile-time type checking
     */
    void registerFunction(const std::string& name, NativeFunc func, std::initializer_list<NativeValueType> types = {});
    /**
     * @brief Check if a native function with the given name exists
     * @param name Function name to look up
     * @return true if the function is registered, false otherwise
     */
    bool hasFunction(const std::string& name) const;
    

    // Flow: Host registers -> VM reads via getVariableValue -> VM writes via setVariableValue
    /**
     * @brief Register a native variable with an initial value
     * @param name Variable name as used in Pixelblaze source code
     * @param value Initial value of the variable
     */
    void registerVariable(const std::string& name, NativeValue value);
    /**
     * @brief Check if a native variable with the given name exists
     * @param name Variable name to look up
     * @return true if the variable is registered, false otherwise
     */
    bool hasVariable(const std::string& name) const;
    

    // Flow: Host registers -> Compiler type-checks -> VM executes Call -> fall through to callFunctionDynamic
    /**
     * @brief Register a dynamic function that will be called via VM virtual method
     * @param name Function name as used in Pixelblaze source code
     * @param types Optional parameter types for compile-time type checking
     */
    void registerDynamicFunction(const std::string& name, std::initializer_list<NativeValueType> types = {});
    /**
     * @brief Check if a dynamic function with the given name exists
     * @param name Function name to look up
     * @return true if the function is registered, false otherwise
     */
    bool hasDynamicFunction(const std::string& name) const;
    
    // Flow: Host registers -> VM checks existence -> reads via getDynamicVarValue -> writes via setDynamicVarValue
    /**
     * @brief Register a dynamic variable managed by VM subclass via virtual methods
     * @param name Variable name as used in Pixelblaze source code
     * @param type Data type of the variable (default: Double)
     */
    void registerDynamicVariable(const std::string& name, NativeValueType type = NativeValueType::Double);
    /**
     * @brief Check if a dynamic variable with the given name exists
     * @param name Variable name to look up
     * @return true if the variable is registered, false otherwise
     */
    bool hasDynamicVariable(const std::string& name) const;
    

    // Access for VM and Compiler
    const std::unordered_map<std::string, IdentHandler>& idents() const { return ident_registry_; }
    const std::unordered_map<std::string, BuiltinHandler>& builtins() const { return builtin_registry_; }

    NativeValue callFunction(const std::string& name, const std::vector<NativeValue>& args);
    const NativeFunctionInfo* getFunctionInfo(const std::string& name) const;
    NativeValue getVariableValue(const std::string& name) const;
    void setVariableValue(const std::string& name, const NativeValue& value);
    const NativeFunctionInfo* getDynamicFunctionInfo(const std::string& name) const;
    NativeValueType getDynamicVariableType(const std::string& name) const;


private:
    bool isBuiltin(const std::string& name) const;
    bool isIdent(const std::string& name) const;
    void registerIdent(const std::string& name, IdentHandler handler);
    void registerIdentOp(const std::string& name, Op op,
                        std::initializer_list<const char*> aliases = {});
    void registerBuiltin(const std::string& name, BuiltinHandler handler);
    void registerBuiltinOp(const std::string& name, Op op,
                          std::initializer_list<const char*> aliases = {});
    NativeFunctionRegistry();
    void initBuiltins();

    NativeFunctionRegistry(const NativeFunctionRegistry&) = delete;
    NativeFunctionRegistry& operator=(const NativeFunctionRegistry&) = delete;

    std::unordered_map<std::string, NativeFunctionInfo> functions_;
    std::unordered_map<std::string, NativeVariableInfo> variables_;
    std::unordered_map<std::string, NativeFunctionInfo> dynamic_functions_;
    std::unordered_map<std::string, NativeValueType> dynamic_variables_;
    std::unordered_map<std::string, IdentHandler> ident_registry_;
    std::unordered_map<std::string, BuiltinHandler> builtin_registry_;
};

class PixelblazeCompiler {
    friend class NativeFunctionRegistry;

public:
    Program compile(const std::string& source) const;
    bool parse_ok() const { return parse_ok_; }
    std::string error_context() const { return error_context_; }
    bool validateProgram(const Program& program) const;

private:
    void stripComments(std::string& source) const;

    void compileTopLevel(std::string source, Program& program) const;
    void compileBlock(const std::string& source, std::vector<Instruction>& out, int loop_depth = 0) const;
    void compileStatement(std::string& src, std::vector<Instruction>& out, int loop_depth = 0) const;
    void compileIf(const std::string& src, std::vector<Instruction>& out, int loop_depth = 0) const;
    void compileFor(const std::string& src, std::vector<Instruction>& out, int loop_depth = 0) const;
    void compileWhile(const std::string& src, std::vector<Instruction>& out, int loop_depth = 0) const;

    void compileExpr(const std::string& expr, std::vector<Instruction>& out) const;
    bool compileAssignExpr(const std::string& expr, std::vector<Instruction>& out, bool as_expression = false) const;

    std::size_t parsePrimary(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const;
    std::size_t parsePostfix(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const;
    std::size_t parseUnary(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const;
    std::size_t parseMul(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const;
    std::size_t parseAdd(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const;
    std::size_t parseRel(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const;
    std::size_t parseEq(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const;
    std::size_t parseAnd(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const;
    std::size_t parseOr(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const;
    std::size_t parseTernary(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const;

    static std::size_t skipWS(std::size_t pos, const std::string& s);
    static bool isIdentChar(char c);
    static bool isIdentStart(char c);
    static std::string toLower(std::string s);
    static std::string trim(const std::string& s);
    static std::string findMatchingBrace(const std::string& s, std::size_t open_pos);
    static std::string findMatchingParen(const std::string& s, std::size_t open_pos);
    static std::vector<std::string> splitTopLevelArgs(const std::string& s);
    static bool matchKeywordAt(const std::string& s, std::size_t pos, const char* kw);
    static std::size_t findStatementEnd(const std::string& s, std::size_t start);
    static std::size_t findBodyEnd(const std::string& s, std::size_t pos, bool stop_at_else);
    static bool isArrayTempName(const std::string& name);

    static void compileArgs(const std::vector<std::string>& args,
                            std::vector<Instruction>& out,
                            PixelblazeCompiler& compiler);

    void emitBuiltin(const std::string& name, const std::vector<std::string>& args,
                     std::vector<Instruction>& out) const;
    void compileIdentExpr(const std::string& ident, std::vector<Instruction>& out) const;

    mutable bool parse_ok_ = true;
    mutable int arr_lit_counter_ = 0;
    mutable std::string error_context_;

    void parseError() const {
        parse_ok_ = false;
        PBZ_ERROR("Parse error at: %s", error_context_.c_str());
    }
};

}  // namespace pixelblaze_cpp