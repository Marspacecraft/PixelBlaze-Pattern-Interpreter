#pragma once

#include <functional>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>

#include "program.h"

namespace pixelblaze_cpp {

class PixelblazeCompiler {
public:
    Program compile(const std::string& source) const;
    bool parse_ok() const { return parse_ok_; }

    using IdentHandler = std::function<void(std::vector<Instruction>& out)>;
    using BuiltinHandler = std::function<void(
        const std::vector<std::string>& args,
        std::vector<Instruction>& out,
        PixelblazeCompiler& compiler
    )>;

    static void registerIdent(const std::string& name, IdentHandler handler);
    static void registerIdentOp(const std::string& name, Op op,
                                std::initializer_list<const char*> aliases = {});
    static void registerBuiltin(const std::string& name, BuiltinHandler handler);
    static void registerBuiltinOp(const std::string& name, Op op,
                                  std::initializer_list<const char*> aliases = {});
    static bool isBuiltin(const std::string& name);
    static bool isIdent(const std::string& name);
    static void initBuiltins();

private:
    void parseError() const { parse_ok_ = false; }

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

    static void compileArgs(const std::vector<std::string>& args,
                            std::vector<Instruction>& out,
                            PixelblazeCompiler& compiler);

    void emitBuiltin(const std::string& name, const std::vector<std::string>& args,
                     std::vector<Instruction>& out) const;
    void compileIdentExpr(const std::string& ident, std::vector<Instruction>& out) const;

    static std::unordered_map<std::string, IdentHandler> ident_registry_;
    static std::unordered_map<std::string, BuiltinHandler> builtin_registry_;

    mutable bool parse_ok_ = true;
    mutable int arr_lit_counter_ = 0;
};

}  // namespace pixelblaze_cpp