#include "pixelblaze.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace pixelblaze_cpp {

void PixelblazeCompiler::compileArgs(const std::vector<std::string>& args,
                                      std::vector<Instruction>& out,
                                      PixelblazeCompiler& compiler) {
    for (const auto& arg : args) {
        if (!compiler.compileAssignExpr(PixelblazeCompiler::trim(arg), out, true)) {
            compiler.parseError();
            return;
        }
    }
}

bool PixelblazeCompiler::isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool PixelblazeCompiler::isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

std::string PixelblazeCompiler::toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string PixelblazeCompiler::trim(const std::string& s) {
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

std::size_t PixelblazeCompiler::skipWS(std::size_t pos, const std::string& s) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
        ++pos;
    }
    return pos;
}

bool PixelblazeCompiler::matchKeywordAt(const std::string& s, std::size_t pos, const char* kw) {
    std::size_t kw_len = 0;
    while (kw[kw_len]) ++kw_len;
    if (pos + kw_len > s.size()) return false;
    for (std::size_t i = 0; i < kw_len; ++i) {
        if (std::tolower(static_cast<unsigned char>(s[pos + i])) !=
            std::tolower(static_cast<unsigned char>(kw[i]))) {
            return false;
        }
    }
    if (pos + kw_len < s.size() && isIdentChar(s[pos + kw_len])) {
        return false;
    }
    return true;
}

std::string PixelblazeCompiler::findMatchingBrace(const std::string& s, std::size_t open_pos) {
    if (open_pos >= s.size() || s[open_pos] != '{') {
        return std::string();
    }
    int depth = 1;
    std::size_t i = open_pos + 1;
    bool in_str = false;
    char str_ch = 0;
    while (i < s.size() && depth > 0) {
        if (in_str) {
            if (s[i] == '\\' && i + 1 < s.size()) { i += 2; continue; }
            if (s[i] == str_ch) { in_str = false; str_ch = 0; }
            ++i;
            continue;
        }
        if (s[i] == '"' || s[i] == '\'') { in_str = true; str_ch = s[i]; ++i; continue; }
        if (s[i] == '{') {
            ++depth;
        } else if (s[i] == '}') {
            --depth;
        }
        if (depth == 0) {
            return s.substr(open_pos + 1, i - open_pos - 1);
        }
        ++i;
    }
    return "";
}

std::string PixelblazeCompiler::findMatchingParen(const std::string& s, std::size_t open_pos) {
    if (open_pos >= s.size() || s[open_pos] != '(') return "";
    int depth = 1;
    std::size_t i = open_pos + 1;
    bool in_str = false;
    char str_ch = 0;
    while (i < s.size() && depth > 0) {
        if (in_str) {
            if (s[i] == '\\' && i + 1 < s.size()) { i += 2; continue; }
            if (s[i] == str_ch) { in_str = false; str_ch = 0; }
            ++i;
            continue;
        }
        if (s[i] == '"' || s[i] == '\'') { in_str = true; str_ch = s[i]; ++i; continue; }
        if (s[i] == '(') {
            ++depth;
        } else if (s[i] == ')') {
            --depth;
        }
        if (depth == 0) {
            return s.substr(open_pos + 1, i - open_pos - 1);
        }
        ++i;
    }
    return "";
}

std::vector<std::string> PixelblazeCompiler::splitTopLevelArgs(const std::string& s) {
    std::vector<std::string> out;
    int depth = 0;
    std::size_t start = 0;
    bool in_str = false;
    char str_ch = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (in_str) {
            if (c == '\\' && i + 1 < s.size()) { ++i; continue; }
            if (c == str_ch) { in_str = false; str_ch = 0; }
            continue;
        }
        if (c == '"' || c == '\'') { in_str = true; str_ch = c; continue; }
        if (c == '(' || c == '{' || c == '[') ++depth;
        else if (c == ')' || c == '}' || c == ']') --depth;
        else if (c == ',' && depth == 0) {
            out.push_back(trim(s.substr(start, i - start)));
            start = i + 1;
        }
    }
    std::string last = trim(s.substr(start));
    if (!last.empty()) out.push_back(last);
    return out;
}

std::size_t PixelblazeCompiler::findStatementEnd(const std::string& s, std::size_t start) {
    if (start >= s.size()) return std::string::npos;

    std::size_t pos = skipWS(start, s);
    if (pos >= s.size()) return std::string::npos;

    if (s[pos] == ';') {
        return pos + 1;
    }

    if (s[pos] == '{') {
        std::string inside = findMatchingBrace(s, pos);
        if (inside.empty()) return std::string::npos;
        std::size_t after_brace = pos + 1 + inside.size() + 1;

        while (after_brace < s.size() && std::isspace(static_cast<unsigned char>(s[after_brace]))) {
            ++after_brace;
        }

        std::string lower_rest = toLower(s.substr(after_brace));

        while (!lower_rest.empty()) {
            if (lower_rest.compare(0, 4, "else") == 0 &&
                (lower_rest.size() == 4 || !isIdentChar(lower_rest[4]))) {
                std::size_t ep = after_brace + 4;
                ep = skipWS(ep, s);
                if (ep >= s.size()) return s.size();

                if (matchKeywordAt(s, ep, "if")) {
                    ep += 2;
                    ep = skipWS(ep, s);
                    if (ep < s.size() && s[ep] == '(') {
                        std::string paren_body = findMatchingParen(s, ep);
                        if (paren_body.empty()) return std::string::npos;
                        ep = ep + 1 + paren_body.size() + 1;
                        ep = skipWS(ep, s);
                        if (ep < s.size() && s[ep] == '{') {
                            std::string body = findMatchingBrace(s, ep);
                            if (body.empty()) return std::string::npos;
                            ep = ep + 1 + body.size() + 1;
                            ep = skipWS(ep, s);
                            after_brace = ep;
                            lower_rest = toLower(s.substr(after_brace));
                            continue;
                        }
                        if (ep < s.size() && s[ep] == ';') {
                            after_brace = ep + 1;
                            lower_rest = toLower(s.substr(after_brace));
                            continue;
                        }
                        {
                            std::size_t body_end = findBodyEnd(s, ep, false);
                            after_brace = body_end;
                            if (after_brace < s.size() && (s[after_brace] == ';' || s[after_brace] == '\n')) {
                                ++after_brace;
                            }
                            lower_rest = toLower(s.substr(after_brace));
                            continue;
                        }
                    }
                    return std::string::npos;
                } else if (ep < s.size() && s[ep] == '{') {
                    std::string body = findMatchingBrace(s, ep);
                    if (body.empty()) return std::string::npos;
                    ep = ep + 1 + body.size() + 1;
                    ep = skipWS(ep, s);
                    after_brace = ep;
                    lower_rest = toLower(s.substr(after_brace));
                    continue;
                } else {
                    std::size_t body_end = findBodyEnd(s, ep, false);
                    after_brace = body_end;
                    if (after_brace < s.size() && (s[after_brace] == ';' || s[after_brace] == '\n')) {
                        ++after_brace;
                    }
                    lower_rest = toLower(s.substr(after_brace));
                    continue;
                }
            }
            break;
        }

        return after_brace;
    }

    int depth = 0;
    bool in_str = false;
    char str_ch = 0;
    while (pos < s.size()) {
        char c = s[pos];
        if (in_str) {
            if (c == '\\' && pos + 1 < s.size()) { pos += 2; continue; }
            if (c == str_ch) { in_str = false; str_ch = 0; }
            ++pos;
            continue;
        }
        if (c == '"' || c == '\'') { in_str = true; str_ch = c; ++pos; continue; }
        if (c == '{') {
            if (depth == 0) {
                std::string inside = findMatchingBrace(s, pos);
                if (!inside.empty()) {
                    pos = pos + 1 + inside.size() + 1;
                    pos = skipWS(pos, s);
                    std::size_t next = pos;
                    while (next < s.size() && std::isspace(static_cast<unsigned char>(s[next]))) ++next;
                    if (next < s.size() && matchKeywordAt(s, next, "else")) {
                        pos = next;
                        continue;
                    }
                    return pos;
                }
            }
            ++depth;
        } else if (c == '(' || c == '[') {
            ++depth;
        } else if (c == ')' || c == ']') {
            --depth;
        } else if (depth == 0) {
            if (c == ';' || c == '\n') {
                std::size_t next = pos + 1;
                while (next < s.size() && std::isspace(static_cast<unsigned char>(s[next]))) ++next;
                if (next < s.size() && matchKeywordAt(s, next, "else")) {
                    pos = next;
                    continue;
                }
                return pos + 1;
            }
        }
        ++pos;
    }
    return s.size();
}

std::size_t PixelblazeCompiler::findBodyEnd(const std::string& s, std::size_t pos, bool stop_at_else) {
    if (pos >= s.size()) return s.size();

    int depth = 0;
    bool in_str = false;
    char str_ch = 0;
    for (std::size_t i = pos; i < s.size(); ++i) {
        char c = s[i];
        if (in_str) {
            if (c == '\\' && i + 1 < s.size()) { ++i; continue; }
            if (c == str_ch) { in_str = false; str_ch = 0; }
            continue;
        }
        if (c == '"' || c == '\'') { in_str = true; str_ch = c; continue; }
        if (c == '(' || c == '{' || c == '[') {
            ++depth;
        } else if (c == ')' || c == '}' || c == ']') {
            --depth;
        } else if (depth == 0) {
            if (c == ';' || c == '\n') {
                return i;
            }
            if (stop_at_else && c == 'e' && matchKeywordAt(s, i, "else")) {
                return i;
            }
        }
    }
    return s.size();
}

void PixelblazeCompiler::stripComments(std::string& s) const {
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        char c = s[i];

        if (c == '"') {
            out.push_back(c);
            ++i;
            while (i < s.size()) {
                char ch = s[i];
                out.push_back(ch);
                if (ch == '\\' && i + 1 < s.size()) {
                    out.push_back(s[++i]);
                    ++i;
                } else if (ch == '"') {
                    ++i;
                    break;
                } else {
                    ++i;
                }
            }
            continue;
        }

        if (c == '\'') {
            out.push_back(c);
            ++i;
            while (i < s.size()) {
                char ch = s[i];
                out.push_back(ch);
                if (ch == '\\' && i + 1 < s.size()) {
                    out.push_back(s[++i]);
                    ++i;
                } else if (ch == '\'') {
                    ++i;
                    break;
                } else {
                    ++i;
                }
            }
            continue;
        }

        if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            while (i < s.size() && s[i] != '\n') ++i;
            if (i < s.size() && s[i] == '\n') {
                out.push_back('\n');
            }
            continue;
        }

        if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
            i += 2;
            while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) {
                ++i;
            }
            if (i + 1 < s.size()) {
                i += 2;
            }
            out.push_back(' ');
            continue;
        }

        out.push_back(c);
        ++i;
    }
    s = std::move(out);
}

Program PixelblazeCompiler::compile(const std::string& source) const {
    Program program;
    parse_ok_ = true;
    std::string src = source;
    stripComments(src);

    PBZ_INFO("Compile start: source length=%zu chars", src.size());

    compileTopLevel(std::move(src), program);

    if (parse_ok_) {
        PBZ_INFO("Compile OK: main_code=%zu instructions, functions=%zu",
                 program.main_code.size(), program.functions.size());
    } else {
        PBZ_ERROR("Compile failed: parse error in source");
    }

#if ENABLE_DUMP
    program.dump();
#endif
    return program;
}

void PixelblazeCompiler::compileTopLevel(std::string src, Program& program) const {
    while (!src.empty()) {
        std::size_t pos = skipWS(0, src);
        if (pos >= src.size()) break;

        src.erase(0, pos);
        if (src.empty()) break;

        std::string lower = toLower(src);

        if (matchKeywordAt(src, 0, "function")) {
            std::size_t p = 8;
            p = skipWS(p, src);
            std::size_t name_start = p;
            while (p < src.size() && isIdentChar(src[p])) ++p;
            if (name_start == p) {
                PBZ_ERROR("Parse error: expected function name at pos %zu", name_start);
                parseError(); break;
            }
            std::string name = src.substr(name_start, p - name_start);

            p = skipWS(p, src);
            std::vector<std::string> params;
            if (p < src.size() && src[p] == '(') {
                std::string paren_body = findMatchingParen(src, p);
                if (paren_body.empty()) {
                    PBZ_ERROR("Parse error: unmatched '(' in function '%s'", name.c_str());
                    parseError(); break;
                }
                p += paren_body.size() + 2;
                params = splitTopLevelArgs(paren_body);
            }

            p = skipWS(p, src);
            if (p < src.size() && src[p] == '{') {
                std::string body = findMatchingBrace(src, p);
                if (body.empty()) {
                    PBZ_ERROR("Parse error: unmatched '{' in function '%s'", name.c_str());
                    parseError(); break;
                }
                p += body.size() + 2;

                FunctionDef fn;
                fn.name = name;
                fn.params = params;
                fn.body_source = body;
                compileBlock(body, fn.code);
                fn.code.push_back(Instruction::ret());
                program.functions[name] = fn;

                PBZ_INFO("Function '%s' compiled: %zu instructions, %zu params",
                         name.c_str(), fn.code.size(), params.size());

                std::string lower_name = toLower(name);
                if (lower_name == "beforerender") {
                    program.before_render_name = name;
                } else if (lower_name == "render" ||
                           lower_name == "render2d" ||
                           lower_name == "render3d") {
                    program.render_name = name;
                }

                src.erase(0, p);
            } else {
                PBZ_ERROR("Parse error: expected '{' after function '%s'", name.c_str());
                parseError();
                break;
            }
            continue;
        }

        if (matchKeywordAt(src, 0, "export")) {
            std::size_t p = 6;
            p = skipWS(p, src);

            if (matchKeywordAt(src, p, "function")) {
                std::size_t fn_p = p + 8;
                fn_p = skipWS(fn_p, src);
                std::size_t name_start = fn_p;
                while (fn_p < src.size() && isIdentChar(src[fn_p])) ++fn_p;
                if (name_start == fn_p) {
                    PBZ_ERROR("Parse error: expected function name in export at pos %zu", name_start);
                    parseError(); break;
                }
                std::string fn_name = src.substr(name_start, fn_p - name_start);

                fn_p = skipWS(fn_p, src);
                std::vector<std::string> fn_params;
                if (fn_p < src.size() && src[fn_p] == '(') {
                    std::string paren_body = findMatchingParen(src, fn_p);
                    if (paren_body.empty()) {
                        PBZ_ERROR("Parse error: unmatched '(' in export function '%s'", fn_name.c_str());
                        parseError(); break;
                    }
                    fn_p += paren_body.size() + 2;
                    fn_params = splitTopLevelArgs(paren_body);
                }

                fn_p = skipWS(fn_p, src);
                if (fn_p < src.size() && src[fn_p] == '{') {
                    std::string body = findMatchingBrace(src, fn_p);
                    if (body.empty()) {
                        PBZ_ERROR("Parse error: unmatched '{' in export function '%s'", fn_name.c_str());
                        parseError(); break;
                    }
                    fn_p += body.size() + 2;

                    FunctionDef fn;
                    fn.name = fn_name;
                    fn.params = fn_params;
                    fn.body_source = body;
                    compileBlock(body, fn.code);
                    fn.code.push_back(Instruction::ret());
                    program.functions[fn_name] = fn;

                    PBZ_INFO("Export function '%s' compiled: %zu instructions", fn_name.c_str(), fn.code.size());

                    std::string lower_fn = toLower(fn_name);
                    if (lower_fn == "beforerender") {
                        program.before_render_name = fn_name;
                    } else if (lower_fn == "render" ||
                               lower_fn == "render2d" ||
                               lower_fn == "render3d") {
                        program.render_name = fn_name;
                    }

                    program.export_functions.push_back(fn_name);
                    src.erase(0, fn_p);
                } else {
                    PBZ_ERROR("Parse error: expected '{' after export function '%s'", fn_name.c_str());
                    parseError();
                    break;
                }
                continue;
            }

            if (matchKeywordAt(src, p, "var")) {
                std::size_t vp = p + 3;
                vp = skipWS(vp, src);
                std::size_t line_end = src.find('\n', vp);
                std::string line = line_end == std::string::npos
                    ? src.substr(vp)
                    : src.substr(vp, line_end - vp);
                line = trim(line);
                while (!line.empty() && line.back() == ';') line.pop_back();

                std::vector<std::string> var_names;
                std::size_t eq_pos = line.find('=');
                std::string names_part;
                std::string init_expr;
                if (eq_pos != std::string::npos) {
                    names_part = trim(line.substr(0, eq_pos));
                    init_expr = trim(line.substr(eq_pos + 1));
                    while (!init_expr.empty() && init_expr.back() == ';') init_expr.pop_back();
                } else {
                    names_part = line;
                }

                {
                    std::string current;
                    int depth = 0;
                    bool in_str = false;
                    char str_ch = 0;
                    for (std::size_t i = 0; i < names_part.size(); ++i) {
                        char ch = names_part[i];
                        if (in_str) {
                            current.push_back(ch);
                            if (ch == '\\' && i + 1 < names_part.size()) {
                                current.push_back(names_part[++i]);
                                continue;
                            }
                            if (ch == str_ch) { in_str = false; str_ch = 0; }
                            continue;
                        }
                        if (ch == '"' || ch == '\'') {
                            in_str = true; str_ch = ch; current.push_back(ch); continue;
                        }
                        if (ch == '(' || ch == '{' || ch == '[') ++depth;
                        else if (ch == ')' || ch == '}' || ch == ']') --depth;
                        else if (ch == ',' && depth == 0) {
                            std::string v = trim(current);
                            if (!v.empty()) var_names.push_back(v);
                            current.clear();
                            continue;
                        }
                        current.push_back(ch);
                    }
                    std::string v = trim(current);
                    if (!v.empty()) var_names.push_back(v);
                }

                for (const auto& vn : var_names) {
                    PBZ_INFO("Export var '%s'", vn.c_str());
                    program.export_vars.push_back(vn);
                    program.main_code.push_back(Instruction::declVar(vn));
                }

                std::size_t bracket_pos = line.find('[');
                if (bracket_pos != std::string::npos && line.back() == ']') {
                    std::string size_str = line.substr(bracket_pos + 1, line.size() - bracket_pos - 2);
                    double size_val = 0.0;
                    try { size_val = std::stod(trim(size_str)); } catch (...) { parseError(); }
                    program.main_code.push_back(Instruction::push(size_val));
                    program.main_code.push_back(Instruction::arrayDecl(var_names[0]));
                } else if (!init_expr.empty() && var_names.size() == 1) {
                    std::size_t before = program.main_code.size();
                    bool ok = compileAssignExpr(init_expr, program.main_code, true);
                    if (!ok) { parseError(); break; }
                    if (program.main_code.size() > before &&
                        program.main_code.back().op == Op::ArrayLiteral &&
                        program.main_code.back().name.rfind("__arr_lit_", 0) == 0) {
                        program.main_code.back().name = var_names[0];
                    } else {
                        program.main_code.push_back(Instruction::setVar(var_names[0]));
                    }
                } else if (!init_expr.empty()) {
                    bool ok = compileAssignExpr(init_expr, program.main_code, true);
                    if (!ok) { parseError(); break; }
                    for (const auto& vn : var_names) {
                        program.main_code.push_back(Instruction::dup());
                        program.main_code.push_back(Instruction::setVar(vn));
                    }
                    program.main_code.push_back(Instruction::pop());
                }

                if (line_end == std::string::npos) {
                    src.clear();
                } else {
                    src.erase(0, line_end + 1);
                }
                continue;
            }

            std::size_t line_end = src.find('\n', p);
            std::string line = line_end == std::string::npos
                ? src.substr(p)
                : src.substr(p, line_end - p);
            line = trim(line);
            while (!line.empty() && line.back() == ';') line.pop_back();
            if (line_end == std::string::npos) {
                src.clear();
            } else {
                src.erase(0, line_end + 1);
            }
            compileStatement(line, program.main_code);
            continue;
        }

        if (matchKeywordAt(src, 0, "storage")) {
            std::size_t p = 7;
            p = skipWS(p, src);
            if (p < src.size() && src[p] == '(') {
                std::size_t line_end = src.find('\n', p);
                std::string line = line_end == std::string::npos
                    ? src.substr(p)
                    : src.substr(p, line_end - p);
                line = trim(line);
                while (!line.empty() && line.back() == ';') line.pop_back();
                if (line_end == std::string::npos) src.clear();
                else src.erase(0, line_end + 1);
                compileStatement(line, program.main_code);
                continue;
            }
        }

        std::size_t stmt_end = findStatementEnd(src, 0);
        if (stmt_end == std::string::npos) {
            std::size_t line_end = src.find('\n');
            std::string line = line_end == std::string::npos
                ? src : src.substr(0, line_end);
            std::string stripped = trim(line);
            while (!stripped.empty() && stripped.back() == ';') stripped.pop_back();
            if (!stripped.empty()) {
                compileStatement(stripped, program.main_code);
            }
            if (line_end == std::string::npos) break;
            src.erase(0, line_end + 1);
        } else {
            std::string stmt = src.substr(0, stmt_end);
            stmt = trim(stmt);
            while (!stmt.empty() && stmt.back() == ';') stmt.pop_back();
            if (!stmt.empty()) {
                compileStatement(stmt, program.main_code);
            }
            src.erase(0, stmt_end);
        }
    }
}

void PixelblazeCompiler::compileBlock(const std::string& source, std::vector<Instruction>& out, int loop_depth) const {
    std::string src = source;
    while (!src.empty()) {
        std::size_t pos = skipWS(0, src);
        if (pos >= src.size()) break;
        src.erase(0, pos);
        if (src.empty()) break;

        if (src.front() == ';') {
            src.erase(src.begin());
            continue;
        }

        std::size_t stmt_end = findStatementEnd(src, 0);
        if (stmt_end == std::string::npos) {
            std::size_t line_end = src.find('\n');
            std::string line = line_end == std::string::npos
                ? src : src.substr(0, line_end);
            std::string stripped = trim(line);
            while (!stripped.empty() && stripped.back() == ';') stripped.pop_back();
            if (!stripped.empty()) {
                compileStatement(stripped, out, loop_depth);
            }
            if (line_end == std::string::npos) break;
            src.erase(0, line_end + 1);
        } else {
            std::string stmt = src.substr(0, stmt_end);
            stmt = trim(stmt);
            while (!stmt.empty() && stmt.back() == ';') stmt.pop_back();
            if (!stmt.empty()) {
                compileStatement(stmt, out, loop_depth);
            }
            src.erase(0, stmt_end);
        }
    }
}

void PixelblazeCompiler::compileStatement(std::string& stmt, std::vector<Instruction>& out, int loop_depth) const {
    while (!stmt.empty() && (stmt.back() == ';' || stmt.back() == '\r')) {
        stmt.pop_back();
    }
    stmt = trim(stmt);
    if (stmt.empty()) return;

    std::string lower = toLower(stmt);

    if (lower == "break") {
        if (loop_depth <= 0) {
            PBZ_ERROR("Parse error: 'break' outside loop");
            parseError(); return;
        }
        out.push_back(Instruction::makeOp(Op::Break));
        return;
    }
    if (lower == "continue") {
        if (loop_depth <= 0) {
            PBZ_ERROR("Parse error: 'continue' outside loop");
            parseError(); return;
        }
        out.push_back(Instruction::makeOp(Op::Continue));
        return;
    }

    if (matchKeywordAt(stmt, 0, "var")) {
        std::string rest = trim(stmt.substr(3));
        if (rest.empty()) { parseError(); return; }

        std::vector<std::string> items;
        {
            std::string current;
            int depth = 0;
            bool in_str = false;
            char str_ch = 0;
            for (std::size_t i = 0; i < rest.size(); ++i) {
                char ch = rest[i];
                if (in_str) {
                    current.push_back(ch);
                    if (ch == '\\' && i + 1 < rest.size()) {
                        current.push_back(rest[++i]);
                        continue;
                    }
                    if (ch == str_ch) { in_str = false; str_ch = 0; }
                    continue;
                }
                if (ch == '"' || ch == '\'') {
                    in_str = true; str_ch = ch; current.push_back(ch); continue;
                }
                if (ch == '(' || ch == '{' || ch == '[') ++depth;
                else if (ch == ')' || ch == '}' || ch == ']') --depth;
                else if (ch == ',' && depth == 0) {
                    std::string v = trim(current);
                    if (!v.empty()) items.push_back(v);
                    current.clear();
                    continue;
                }
                current.push_back(ch);
            }
            std::string v = trim(current);
            if (!v.empty()) items.push_back(v);
        }

        if (items.empty()) { parseError(); return; }

        for (const auto& item : items) {
            std::size_t bracket_pos = item.find('[');
            if (bracket_pos != std::string::npos && item.back() == ']') {
                std::string var_name = trim(item.substr(0, bracket_pos));
                if (var_name.empty()) { parseError(); return; }
                std::string size_str = item.substr(bracket_pos + 1, item.size() - bracket_pos - 2);
                double size_val = 0.0;
                try { size_val = std::stod(trim(size_str)); } catch (...) { parseError(); return; }
                out.push_back(Instruction::push(size_val));
                out.push_back(Instruction::arrayDecl(var_name));
                continue;
            }

            std::size_t eq_pos = item.find('=');
            if (eq_pos != std::string::npos) {
                std::string var_name = trim(item.substr(0, eq_pos));
                std::string expr = trim(item.substr(eq_pos + 1));
                if (var_name.empty() || expr.empty()) { parseError(); return; }
                out.push_back(Instruction::declVar(var_name));
                std::size_t before = out.size();
                bool ok = compileAssignExpr(expr, out, true);
                if (!ok) { parseError(); return; }
                if (out.size() > before && out.back().op == Op::ArrayLiteral && out.back().name.rfind("__arr_lit_", 0) == 0) {
                    out.back().name = var_name;
                } else {
                    out.push_back(Instruction::setVar(var_name));
                }
            } else {
                std::string var_name = trim(item);
                if (var_name.empty()) { parseError(); return; }
                out.push_back(Instruction::declVar(var_name));
            }
        }
        return;
    }

    if (matchKeywordAt(stmt, 0, "return")) {
        std::size_t pos = 6;
        pos = skipWS(pos, stmt);
        if (pos < stmt.size()) {
            std::string rest = trim(stmt.substr(pos));
            while (!rest.empty() && rest.back() == ';') rest.pop_back();
            if (!rest.empty()) {
                bool ok = compileAssignExpr(rest, out, true);
                if (!ok) { parseError(); return; }
            }
        }
        out.push_back(Instruction::ret());
        return;
    }

    if (matchKeywordAt(stmt, 0, "if")) {
        compileIf(stmt, out, loop_depth);
        return;
    }
    if (matchKeywordAt(stmt, 0, "for")) {
        compileFor(stmt, out, loop_depth);
        return;
    }
    if (matchKeywordAt(stmt, 0, "while")) {
        compileWhile(stmt, out, loop_depth);
        return;
    }

    bool has_value = compileAssignExpr(stmt, out);
    if (has_value) {
        if (out.empty() || out.back().op != Op::ArrayLiteral) {
            out.push_back(Instruction::pop());
        }
    }
}

void PixelblazeCompiler::compileIf(const std::string& src, std::vector<Instruction>& out, int loop_depth) const {
    std::size_t pos = 2;
    pos = skipWS(pos, src);
    if (pos >= src.size() || src[pos] != '(') { parseError(); return; }

    std::string cond = findMatchingParen(src, pos);
    if (cond.empty()) { parseError(); return; }
    std::size_t after_paren = pos + 1 + cond.size() + 1;

    bool ok = compileAssignExpr(cond, out, true);
    if (!ok) { parseError(); return; }

    after_paren = skipWS(after_paren, src);

    std::string body_str;
    std::size_t consumed = 0;
    if (after_paren < src.size() && src[after_paren] == '{') {
        body_str = findMatchingBrace(src, after_paren);
        if (body_str.empty()) { parseError(); return; }
        consumed = body_str.size() + 2;
    } else {
        std::size_t body_end = findBodyEnd(src, after_paren, true);
        body_str = src.substr(after_paren, body_end - after_paren);
        body_str = trim(body_str);
        while (!body_str.empty() && body_str.back() == ';') body_str.pop_back();
        consumed = body_end - after_paren;
        if (body_end < src.size() && (src[body_end] == ';' || src[body_end] == '\n')) {
            consumed += 1;
        }
    }

    std::vector<Instruction> body_code;
    compileBlock(body_str, body_code, loop_depth);

    std::size_t after_body = after_paren + consumed;
    after_body = skipWS(after_body, src);

    bool has_else = false;
    if (after_body < src.size()) {
        std::string tail = src.substr(after_body);
        if (matchKeywordAt(tail, 0, "else")) {
            has_else = true;
            std::size_t ep = 4;
            ep = skipWS(ep, tail);

            if (matchKeywordAt(tail, ep, "if")) {
                std::string else_rest = trim(tail.substr(4));
                std::vector<Instruction> else_code;
                compileIf(else_rest, else_code, loop_depth);
                int jump_over_else = static_cast<int>(body_code.size()) + 1;
                out.push_back(Instruction::jumpIfFalse(jump_over_else + 1));
                for (auto& instr : body_code) out.push_back(instr);
                out.push_back(Instruction::jump(static_cast<int>(else_code.size()) + 1));
                for (auto& instr : else_code) out.push_back(instr);
                return;
            }

            std::string else_body_str;
            if (ep < tail.size() && tail[ep] == '{') {
                else_body_str = findMatchingBrace(tail, ep);
                if (else_body_str.empty()) { parseError(); return; }
            } else {
                std::size_t else_body_end = findBodyEnd(tail, ep, false);
                else_body_str = tail.substr(ep, else_body_end - ep);
                else_body_str = trim(else_body_str);
                while (!else_body_str.empty() && else_body_str.back() == ';') else_body_str.pop_back();
            }

            std::vector<Instruction> else_code;
            if (!else_body_str.empty()) {
                compileBlock(else_body_str, else_code, loop_depth);
                int jump_over_else = static_cast<int>(body_code.size()) + 1;
                out.push_back(Instruction::jumpIfFalse(jump_over_else + 1));
                for (auto& instr : body_code) out.push_back(instr);
                out.push_back(Instruction::jump(static_cast<int>(else_code.size()) + 1));
                for (auto& instr : else_code) out.push_back(instr);
                return;
            }
        }
    }

    out.push_back(Instruction::jumpIfFalse(static_cast<int>(body_code.size()) + 1));
    for (auto& instr : body_code) out.push_back(instr);
}

void PixelblazeCompiler::compileFor(const std::string& src, std::vector<Instruction>& out, int loop_depth) const {
    std::size_t pos = 3;
    pos = skipWS(pos, src);
    if (pos >= src.size() || src[pos] != '(') { parseError(); return; }

    std::string paren_body = findMatchingParen(src, pos);
    if (paren_body.empty()) { parseError(); return; }
    std::size_t after_paren = pos + 1 + paren_body.size() + 1;

    after_paren = skipWS(after_paren, src);

    std::string body_str;
    if (after_paren < src.size() && src[after_paren] == '{') {
        body_str = findMatchingBrace(src, after_paren);
        if (body_str.empty()) { parseError(); return; }
    } else {
        std::size_t body_end = findBodyEnd(src, after_paren, false);
        body_str = src.substr(after_paren, body_end - after_paren);
        body_str = trim(body_str);
        while (!body_str.empty() && body_str.back() == ';') body_str.pop_back();
    }

    std::vector<std::string> parts;
    {
        std::string current;
        int depth = 0;
        for (std::size_t i = 0; i < paren_body.size(); ++i) {
            char c = paren_body[i];
            if (c == '(' || c == '{' || c == '[') ++depth;
            else if (c == ')' || c == '}' || c == ']') --depth;
            else if (c == ';' && depth == 0) {
                parts.push_back(trim(current));
                current.clear();
                continue;
            }
            current.push_back(c);
        }
        parts.push_back(trim(current));
    }

    if (parts.size() != 3) { parseError(); return; }

    std::vector<Instruction> init_code;
    std::vector<Instruction> cond_code;
    std::vector<Instruction> post_code;
    std::vector<Instruction> body_code;

    if (!parts[0].empty()) compileStatement(parts[0], init_code, loop_depth);
    if (!parts[1].empty()) {
        bool ok = compileAssignExpr(parts[1], cond_code, true);
        if (!ok) { parseError(); return; }
    } else {
        cond_code.push_back(Instruction::push(1.0));
    }
    if (!parts[2].empty()) {
        bool has_val = compileAssignExpr(parts[2], post_code);
        if (has_val && !post_code.empty() && post_code.back().op != Op::ArrayLiteral) post_code.push_back(Instruction::pop());
    }
    compileBlock(body_str, body_code, loop_depth + 1);

    std::vector<std::size_t> break_patches;
    std::vector<std::size_t> continue_patches;
    for (std::size_t i = 0; i < body_code.size(); ++i) {
        if (body_code[i].op == Op::Break) break_patches.push_back(i);
        else if (body_code[i].op == Op::Continue) continue_patches.push_back(i);
    }

    for (auto& instr : init_code) out.push_back(instr);

    int body_size = static_cast<int>(body_code.size());
    int post_size = static_cast<int>(post_code.size());

    int cond_start = static_cast<int>(out.size());
    for (auto& instr : cond_code) out.push_back(instr);

    out.push_back(Instruction::jumpIfFalse(body_size + post_size + 2));

    int body_start = static_cast<int>(out.size());
    for (auto& instr : body_code) out.push_back(instr);

    int post_start = static_cast<int>(out.size());
    for (auto& instr : post_code) out.push_back(instr);

    int j_target = cond_start - static_cast<int>(out.size());
    out.push_back(Instruction::jump(j_target));

    int loop_end = static_cast<int>(out.size());

    for (auto bp : break_patches) {
        out[body_start + bp] = Instruction::jump(loop_end - (body_start + bp));
    }
    for (auto cp : continue_patches) {
        out[body_start + cp] = Instruction::jump(post_start - (body_start + cp));
    }
}

void PixelblazeCompiler::compileWhile(const std::string& src, std::vector<Instruction>& out, int loop_depth) const {
    std::size_t pos = 5;
    pos = skipWS(pos, src);
    if (pos >= src.size() || src[pos] != '(') { parseError(); return; }

    std::string cond = findMatchingParen(src, pos);
    if (cond.empty()) { parseError(); return; }
    std::size_t after_paren = pos + 1 + cond.size() + 1;

    after_paren = skipWS(after_paren, src);

    std::string body_str;
    if (after_paren < src.size() && src[after_paren] == '{') {
        body_str = findMatchingBrace(src, after_paren);
        if (body_str.empty()) { parseError(); return; }
    } else {
        std::size_t body_end = findBodyEnd(src, after_paren, false);
        body_str = src.substr(after_paren, body_end - after_paren);
        body_str = trim(body_str);
        while (!body_str.empty() && body_str.back() == ';') body_str.pop_back();
    }

    std::vector<Instruction> body_code;
    compileBlock(body_str, body_code, loop_depth + 1);

    std::vector<Instruction> cond_code;
    bool ok = compileAssignExpr(cond, cond_code, true);
    if (!ok) { parseError(); return; }

    std::vector<std::size_t> break_patches;
    std::vector<std::size_t> continue_patches;
    for (std::size_t i = 0; i < body_code.size(); ++i) {
        if (body_code[i].op == Op::Break) break_patches.push_back(i);
        else if (body_code[i].op == Op::Continue) continue_patches.push_back(i);
    }

    int cond_size = static_cast<int>(cond_code.size());
    int body_size = static_cast<int>(body_code.size());

    int cond_start = static_cast<int>(out.size());

    for (auto& instr : cond_code) out.push_back(instr);

    out.push_back(Instruction::jumpIfFalse(body_size + 2));

    int body_base = static_cast<int>(out.size());
    for (auto& instr : body_code) out.push_back(instr);

    int jump_back = cond_start - static_cast<int>(out.size());
    out.push_back(Instruction::jump(jump_back));

    int loop_end = static_cast<int>(out.size());

    for (auto bp : break_patches) {
        out[body_base + bp] = Instruction::jump(loop_end - (body_base + bp));
    }
    for (auto cp : continue_patches) {
        out[body_base + cp] = Instruction::jump(cond_start - (body_base + cp));
    }
}

void PixelblazeCompiler::compileExpr(const std::string& expr, std::vector<Instruction>& out) const {
    compileAssignExpr(expr, out, true);
}

void PixelblazeCompiler::compileIdentExpr(const std::string& ident, std::vector<Instruction>& out) const {
    auto& reg = NativeFunctionRegistry::instance();
    auto it = reg.idents().find(ident);
    if (it != reg.idents().end()) {
        it->second(out);
        return;
    }
    if (reg.hasDynamicVariable(ident)) {
        out.push_back(Instruction::getVar(ident));
        return;
    }
    out.push_back(Instruction::getVar(ident));
}

void PixelblazeCompiler::emitBuiltin(const std::string& name, const std::vector<std::string>& args,
                                     std::vector<Instruction>& out) const {
    auto& reg = NativeFunctionRegistry::instance();
    auto it = reg.builtins().find(name);
    if (it != reg.builtins().end()) {
        it->second(args, out, *const_cast<PixelblazeCompiler*>(this));
        return;
    }
    if (reg.hasFunction(name)) {
        const auto* info = reg.getFunctionInfo(name);
        if (info && info->hasTypeInfo) {
            if (args.size() != info->paramTypes.size()) {
                parseError();
                return;
            }
            for (std::size_t i = 0; i < args.size() && i < info->paramTypes.size(); ++i) {
                const std::string& arg = trim(args[i]);
                NativeValueType expected = info->paramTypes[i];
                bool is_str_lit = (!arg.empty() && (arg.front() == '"' || arg.front() == '\''));
                bool is_num_lit = false;
                if (!arg.empty() && !is_str_lit) {
                    char c = arg[0];
                    is_num_lit = std::isdigit(static_cast<unsigned char>(c)) ||
                                 (c == '.' && arg.size() > 1 &&
                                  std::isdigit(static_cast<unsigned char>(arg[1])));
                }
                if (is_str_lit && expected != NativeValueType::String) {
                    parseError();
                    return;
                }
                if (is_num_lit && expected != NativeValueType::Double) {
                    parseError();
                    return;
                }
            }
        }
        compileArgs(args, out, *const_cast<PixelblazeCompiler*>(this));
        out.push_back(Instruction::callNative(name, static_cast<int>(args.size())));
        return;
    }
    if (reg.hasDynamicFunction(name)) {
        const auto* info = reg.getDynamicFunctionInfo(name);
        if (info && info->hasTypeInfo) {
            if (args.size() != info->paramTypes.size()) {
                parseError();
                return;
            }
            for (std::size_t i = 0; i < args.size() && i < info->paramTypes.size(); ++i) {
                const std::string& arg = trim(args[i]);
                NativeValueType expected = info->paramTypes[i];
                bool is_str_lit = (!arg.empty() && (arg.front() == '"' || arg.front() == '\''));
                bool is_num_lit = false;
                if (!arg.empty() && !is_str_lit) {
                    char c = arg[0];
                    is_num_lit = std::isdigit(static_cast<unsigned char>(c)) ||
                                 (c == '.' && arg.size() > 1 &&
                                  std::isdigit(static_cast<unsigned char>(arg[1])));
                }
                if (is_str_lit && expected != NativeValueType::String) {
                    parseError();
                    return;
                }
                if (is_num_lit && expected != NativeValueType::Double) {
                    parseError();
                    return;
                }
            }
        }
        compileArgs(args, out, *const_cast<PixelblazeCompiler*>(this));
        out.push_back(Instruction::call(name));
        out.back().index = static_cast<int>(args.size());
        return;
    }
    out.push_back(Instruction::call(name));
    out.back().index = static_cast<int>(args.size());
}

std::size_t PixelblazeCompiler::parsePrimary(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const {
    pos = skipWS(pos, s);
    if (pos >= s.size()) return pos;
    char c = s[pos];

    if (c == '(') {
        ++pos;
        pos = parseTernary(s, pos, out);
        if (pos == std::string::npos) { parseError(); return std::string::npos; }
        pos = skipWS(pos, s);
        if (pos >= s.size() || s[pos] != ')') { parseError(); return std::string::npos; }
        ++pos;
        return pos;
    }

    if (c == '{') {
        std::string inside = findMatchingBrace(s, pos);
        if (inside.empty()) { parseError(); return std::string::npos; }
        std::string trimmed = trim(inside);
        if (!trimmed.empty()) { parseError(); return std::string::npos; }
        out.push_back(Instruction::push(0.0));
        return pos + 1 + inside.size() + 1;
    }

    if (c == '[') {
        ++pos;
        pos = skipWS(pos, s);
        if (pos < s.size() && s[pos] == ']') {
            ++pos;
            out.push_back(Instruction::push(0.0));
            std::string name = "__arr_lit_" + std::to_string(arr_lit_counter_++);
            out.push_back(Instruction::arrayLiteral());
            out.back().name = name;
            return pos;
        }
        std::vector<std::string> items;
        int depth = 0;
        std::size_t item_start = pos;
        bool in_str = false;
        char str_ch = 0;
        while (pos < s.size()) {
            char ic = s[pos];
            if (in_str) {
                if (ic == '\\' && pos + 1 < s.size()) { pos += 2; continue; }
                if (ic == str_ch) { in_str = false; str_ch = 0; }
                ++pos;
                continue;
            }
            if (ic == '"' || ic == '\'') { in_str = true; str_ch = ic; ++pos; continue; }
            if (ic == '(' || ic == '{' || ic == '[') ++depth;
            else if (ic == ')' || ic == '}' || ic == ']') --depth;
            if (depth == 0 && ic == ',') {
                items.push_back(trim(s.substr(item_start, pos - item_start)));
                item_start = pos + 1;
            } else if (depth == 0 && ic == ']') {
                items.push_back(trim(s.substr(item_start, pos - item_start)));
                ++pos;
                break;
            }
            ++pos;
        }
        if (pos >= s.size()) { parseError(); return std::string::npos; }
        std::size_t n = items.size();
        for (const auto& item : items) {
            if (!compileAssignExpr(trim(item), out, true)) { parseError(); return std::string::npos; }
        }
        out.push_back(Instruction::push(static_cast<double>(n)));
        std::string name = "__arr_lit_" + std::to_string(arr_lit_counter_++);
        out.push_back(Instruction::arrayLiteral());
        out.back().name = name;
        return pos;
    }

    if (c == '"' || c == '\'') {
        char quote = c;
        ++pos;
        std::string str_val;
        while (pos < s.size() && s[pos] != quote) {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                ++pos;
                char ec = s[pos];
                if (ec == 'n') str_val += '\n';
                else if (ec == 't') str_val += '\t';
                else if (ec == 'r') str_val += '\r';
                else if (ec == '\\') str_val += '\\';
                else if (ec == '"') str_val += '"';
                else if (ec == '\'') str_val += '\'';
                else str_val += ec;
            } else {
                str_val += s[pos];
            }
            ++pos;
        }
        if (pos >= s.size() || s[pos] != quote) { parseError(); return std::string::npos; }
        ++pos;
        out.push_back(Instruction::makeString(str_val));
        return pos;
    }

    if (std::isdigit(static_cast<unsigned char>(c)) ||
        (c == '.' && pos + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[pos + 1])))) {
        std::size_t start = pos;
        if (c == '.') ++pos;
        while (pos < s.size() &&
               (std::isdigit(static_cast<unsigned char>(s[pos])) || s[pos] == '.')) ++pos;
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
            ++pos;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos;
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        }
        double val = 0.0;
        try { val = std::stod(s.substr(start, pos - start)); } catch (...) { parseError(); }
        out.push_back(Instruction::push(val));
        return pos;
    }

    if (isIdentStart(c)) {
        std::size_t start = pos;
        while (pos < s.size() && isIdentChar(s[pos])) ++pos;
        std::string ident = s.substr(start, pos - start);
        pos = skipWS(pos, s);

        if (ident == "true" || ident == "false") {
            out.push_back(Instruction::push(ident == "true" ? 1.0 : 0.0));
            return pos;
        }
        if (ident == "null" || ident == "undefined") {
            out.push_back(Instruction::push(0.0));
            return pos;
        }

        if (pos < s.size() && s[pos] == '(') {
            std::vector<std::string> args;
            std::size_t ap = pos + 1;
            ap = skipWS(ap, s);
            if (ap < s.size() && s[ap] == ')') {
                args.clear();
                ++ap;
                pos = ap;
                emitBuiltin(ident, args, out);
                return pos;
            }
            int depth = 1;
            std::size_t arg_start = ap;
            bool in_str = false;
            char str_ch = 0;
            while (ap < s.size() && depth > 0) {
                char ch = s[ap];
                if (in_str) {
                    if (ch == '\\' && ap + 1 < s.size()) { ap += 2; continue; }
                    if (ch == str_ch) { in_str = false; str_ch = 0; }
                    ++ap;
                    continue;
                }
                if (ch == '"' || ch == '\'') { in_str = true; str_ch = ch; ++ap; continue; }
                if (ch == '(' || ch == '{' || ch == '[') ++depth;
                else if (ch == ')') {
                    --depth;
                    if (depth == 0) break;
                } else if (ch == ',' && depth == 1) {
                    args.push_back(trim(s.substr(arg_start, ap - arg_start)));
                    arg_start = ap + 1;
                }
                ++ap;
            }
            if (ap >= s.size() || s[ap] != ')') { parseError(); return std::string::npos; }
            args.push_back(trim(s.substr(arg_start, ap - arg_start)));
            ++ap;
            pos = ap;
            emitBuiltin(ident, args, out);
            return pos;
        }

        if (pos < s.size() && s[pos] == '[') {
            ++pos;
            std::size_t bracket_start = pos;
            int depth = 1;
            while (pos < s.size() && depth > 0) {
                if (s[pos] == '[') ++depth;
                else if (s[pos] == ']') --depth;
                if (depth > 0) ++pos;
            }
            if (pos >= s.size() || s[pos] != ']') { parseError(); return std::string::npos; }
            std::string index_expr = s.substr(bracket_start, pos - bracket_start);
            ++pos;
            if (!compileAssignExpr(trim(index_expr), out, true)) { parseError(); return std::string::npos; }
            out.push_back(Instruction::arrayGet(ident));
            return pos;
        }

        compileIdentExpr(ident, out);
        return pos;
    }

    parseError();
    return std::string::npos;
}

std::size_t PixelblazeCompiler::parsePostfix(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const {
    pos = parsePrimary(s, pos, out);
    if (pos == std::string::npos) return std::string::npos;
    pos = skipWS(pos, s);
    while (pos + 1 < s.size() && ((s[pos] == '+' && s[pos + 1] == '+') ||
                                   (s[pos] == '-' && s[pos + 1] == '-'))) {
        bool is_inc = (s[pos] == '+');
        pos += 2;
        pos = skipWS(pos, s);
        if (out.empty()) { parseError(); return std::string::npos; }
        Instruction last = out.back();
        if (last.op == Op::GetVar) {
            out.pop_back();
            out.push_back(Instruction::getVar(last.name));
            out.push_back(Instruction::dup());
            out.push_back(Instruction::push(1.0));
            out.push_back(Instruction::makeOp(is_inc ? Op::Add : Op::Sub));
            out.push_back(Instruction::setVar(last.name));
        } else if (last.op == Op::ArrayGet) {
            out.pop_back();
            out.push_back(Instruction::dup());
            out.push_back(Instruction::arrayGet(last.name));
            out.push_back(Instruction::dup());
            out.push_back(Instruction::push(1.0));
            out.push_back(Instruction::makeOp(is_inc ? Op::Add : Op::Sub));
            out.push_back(Instruction::rot());
            out.push_back(Instruction::arraySet(last.name));
        } else {
            parseError();
            return std::string::npos;
        }
    }
    return pos;
}

std::size_t PixelblazeCompiler::parseUnary(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const {
    pos = skipWS(pos, s);
    bool pre_inc = false;
    bool pre_dec = false;
    while (pos + 1 < s.size() && ((s[pos] == '+' && s[pos + 1] == '+') ||
                                   (s[pos] == '-' && s[pos + 1] == '-'))) {
        if (s[pos] == '+') pre_inc = true;
        else pre_dec = true;
        pos += 2;
    }
    pos = skipWS(pos, s);
    if (pos < s.size() && (s[pos] == '-' || s[pos] == '!')) {
        char op = s[pos];
        ++pos;
        pos = parseUnary(s, pos, out);
        if (pos == std::string::npos) return std::string::npos;
        out.push_back(Instruction::makeOp(op == '-' ? Op::Neg : Op::Not));
        return pos;
    }
    if (pos < s.size() && s[pos] == '+') {
        bool is_unary = true;
        for (int i = (int)pos - 1; i >= 0; --i) {
            char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
            if (isalnum((unsigned char)c) || c == '_' || c == ')' || c == ']' || c == '.') {
                is_unary = false;
            }
            break;
        }
        if (is_unary) ++pos;
    }
    pos = parsePostfix(s, pos, out);
    if (pos == std::string::npos) return std::string::npos;
    if ((pre_inc || pre_dec) && !out.empty()) {
        Instruction last = out.back();
        if (last.op == Op::GetVar) {
            out.pop_back();
            out.push_back(Instruction::getVar(last.name));
            out.push_back(Instruction::push(1.0));
            out.push_back(Instruction::makeOp(pre_inc ? Op::Add : Op::Sub));
            out.push_back(Instruction::dup());
            out.push_back(Instruction::setVar(last.name));
        } else if (last.op == Op::ArrayGet) {
            out.pop_back();
            out.push_back(Instruction::dup());
            out.push_back(Instruction::arrayGet(last.name));
            out.push_back(Instruction::push(1.0));
            out.push_back(Instruction::makeOp(pre_inc ? Op::Add : Op::Sub));
            out.push_back(Instruction::dup());
            out.push_back(Instruction::rot());
            out.push_back(Instruction::arraySet(last.name));
        } else {
            parseError();
            return std::string::npos;
        }
    }
    return pos;
}

std::size_t PixelblazeCompiler::parseMul(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const {
    pos = parseUnary(s, pos, out);
    if (pos == std::string::npos) return std::string::npos;
    while (true) {
        pos = skipWS(pos, s);
        if (pos >= s.size()) break;
        char op = s[pos];
        if (op != '*' && op != '/' && op != '%') break;
        ++pos;
        pos = skipWS(pos, s);
        pos = parseUnary(s, pos, out);
        if (pos == std::string::npos) return std::string::npos;
        if (op == '*') out.push_back(Instruction::makeOp(Op::Mul));
        else if (op == '/') out.push_back(Instruction::makeOp(Op::Div));
        else out.push_back(Instruction::makeOp(Op::Mod));
    }
    return pos;
}

std::size_t PixelblazeCompiler::parseAdd(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const {
    pos = parseMul(s, pos, out);
    if (pos == std::string::npos) return std::string::npos;
    while (true) {
        pos = skipWS(pos, s);
        if (pos >= s.size()) break;
        char op = s[pos];
        if (op != '+' && op != '-') break;
        // 跳过 ++ 或 --（如果有）
        if (pos + 1 < s.size() && (s[pos + 1] == '+' || s[pos + 1] == '-')) break;
        ++pos;
        pos = skipWS(pos, s);
        pos = parseMul(s, pos, out);
        if (pos == std::string::npos) return std::string::npos;
        if (op == '+') out.push_back(Instruction::makeOp(Op::Add));
        else out.push_back(Instruction::makeOp(Op::Sub));
    }
    return pos;
}

std::size_t PixelblazeCompiler::parseRel(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const {
    while (true) {
        pos = parseAdd(s, pos, out);
        if (pos == std::string::npos) return std::string::npos;
        pos = skipWS(pos, s);
        if (pos + 1 < s.size() && s[pos] == '<' && s[pos + 1] == '=') {
            pos += 2;
            pos = skipWS(pos, s);
            pos = parseAdd(s, pos, out);
            if (pos == std::string::npos) return std::string::npos;
            out.push_back(Instruction::makeOp(Op::Le));
        } else if (pos + 1 < s.size() && s[pos] == '>' && s[pos + 1] == '=') {
            pos += 2;
            pos = skipWS(pos, s);
            pos = parseAdd(s, pos, out);
            if (pos == std::string::npos) return std::string::npos;
            out.push_back(Instruction::makeOp(Op::Ge));
        } else if (pos < s.size() && s[pos] == '<' &&
                   (pos + 1 >= s.size() || s[pos + 1] != '<')) {
            ++pos;
            pos = skipWS(pos, s);
            pos = parseAdd(s, pos, out);
            if (pos == std::string::npos) return std::string::npos;
            out.push_back(Instruction::makeOp(Op::Lt));
        } else if (pos < s.size() && s[pos] == '>' &&
                   (pos + 1 >= s.size() || s[pos + 1] != '>')) {
            ++pos;
            pos = skipWS(pos, s);
            pos = parseAdd(s, pos, out);
            if (pos == std::string::npos) return std::string::npos;
            out.push_back(Instruction::makeOp(Op::Gt));
        } else break;
    }
    return pos;
}

std::size_t PixelblazeCompiler::parseEq(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const {
    while (true) {
        pos = parseRel(s, pos, out);
        if (pos == std::string::npos) return std::string::npos;
        pos = skipWS(pos, s);
        if (pos + 1 < s.size() && s[pos] == '=' && s[pos + 1] == '=') {
            pos += 2;
            pos = skipWS(pos, s);
            pos = parseRel(s, pos, out);
            if (pos == std::string::npos) return std::string::npos;
            out.push_back(Instruction::makeOp(Op::Eq));
        } else if (pos + 1 < s.size() && s[pos] == '!' && s[pos + 1] == '=') {
            pos += 2;
            pos = skipWS(pos, s);
            pos = parseRel(s, pos, out);
            if (pos == std::string::npos) return std::string::npos;
            out.push_back(Instruction::makeOp(Op::Ne));
        } else break;
    }
    return pos;
}

std::size_t PixelblazeCompiler::parseAnd(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const {
    while (true) {
        pos = parseEq(s, pos, out);
        if (pos == std::string::npos) return std::string::npos;
        pos = skipWS(pos, s);
        if (pos + 1 < s.size() && s[pos] == '&' && s[pos + 1] == '&') {
            pos += 2;
            pos = skipWS(pos, s);
            pos = parseEq(s, pos, out);
            if (pos == std::string::npos) return std::string::npos;
            out.push_back(Instruction::makeOp(Op::And));
        } else break;
    }
    return pos;
}

std::size_t PixelblazeCompiler::parseOr(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const {
    while (true) {
        pos = parseAnd(s, pos, out);
        if (pos == std::string::npos) return std::string::npos;
        pos = skipWS(pos, s);
        if (pos + 1 < s.size() && s[pos] == '|' && s[pos + 1] == '|') {
            pos += 2;
            pos = skipWS(pos, s);
            pos = parseAnd(s, pos, out);
            if (pos == std::string::npos) return std::string::npos;
            out.push_back(Instruction::makeOp(Op::Or));
        } else break;
    }
    return pos;
}

std::size_t PixelblazeCompiler::parseTernary(const std::string& s, std::size_t pos, std::vector<Instruction>& out) const {
    pos = parseOr(s, pos, out);
    if (pos == std::string::npos) return std::string::npos;
    pos = skipWS(pos, s);
    if (pos >= s.size() || s[pos] != '?') return pos;

    ++pos;

    std::size_t colon_pos = std::string::npos;
    int depth = 0;
    int qdepth = 1;
    bool in_str = false;
    char str_ch = 0;
    for (std::size_t i = pos; i < s.size(); ++i) {
        char c = s[i];
        if (in_str) {
            if (c == '\\' && i + 1 < s.size()) { ++i; continue; }
            if (c == str_ch) { in_str = false; str_ch = 0; }
            continue;
        }
        if (c == '"' || c == '\'') { in_str = true; str_ch = c; continue; }
        if (c == '(' || c == '{' || c == '[') ++depth;
        else if (c == ')' || c == '}' || c == ']') --depth;
        else if (c == '?' && depth == 0) ++qdepth;
        else if (c == ':' && depth == 0) {
            --qdepth;
            if (qdepth == 0) { colon_pos = i; break; }
        }
    }
    if (colon_pos == std::string::npos) { parseError(); return std::string::npos; }

    std::string then_str = s.substr(pos, colon_pos - pos);
    std::vector<Instruction> then_code;
    std::size_t then_end = parseTernary(then_str, 0, then_code);
    if (then_end == std::string::npos) { parseError(); return std::string::npos; }
    if (then_end != then_str.size()) { parseError(); return std::string::npos; }

    std::string else_str = s.substr(colon_pos + 1);
    std::vector<Instruction> else_code;
    std::size_t else_end = parseTernary(else_str, 0, else_code);
    if (else_end == std::string::npos) { parseError(); return std::string::npos; }
    if (else_end == 0) { parseError(); return std::string::npos; }

    int jump_over_then = static_cast<int>(then_code.size()) + 1;
    out.push_back(Instruction::jumpIfFalse(jump_over_then + 1));
    for (auto& instr : then_code) out.push_back(instr);
    out.push_back(Instruction::jump(static_cast<int>(else_code.size()) + 1));
    for (auto& instr : else_code) out.push_back(instr);

    return colon_pos + 1 + else_end;
}

bool PixelblazeCompiler::compileAssignExpr(const std::string& expr, std::vector<Instruction>& out, bool as_expression) const {
    std::string trimmed = trim(expr);
    if (trimmed.empty()) return false;

    std::vector<std::string> parts;
    {
        std::string current;
        int depth = 0;
        bool in_str = false;
        char str_ch = 0;
        for (std::size_t i = 0; i < trimmed.size(); ++i) {
            char c = trimmed[i];
            if (in_str) {
                current.push_back(c);
                if (c == '\\' && i + 1 < trimmed.size()) {
                    current.push_back(trimmed[++i]);
                    continue;
                }
                if (c == str_ch) {
                    in_str = false;
                    str_ch = 0;
                }
                continue;
            }
            if (c == '"' || c == '\'') {
                in_str = true;
                str_ch = c;
                current.push_back(c);
                continue;
            }
            if (c == '(' || c == '{' || c == '[') ++depth;
            else if (c == ')' || c == '}' || c == ']') --depth;
            else if (c == ',' && depth == 0) {
                parts.push_back(trim(current));
                current.clear();
                continue;
            }
            current.push_back(c);
        }
        parts.push_back(trim(current));
    }

    if (parts.size() > 1) {
        for (std::size_t i = 0; i < parts.size(); ++i) {
            bool is_last = (i == parts.size() - 1);
            bool has_val = compileAssignExpr(parts[i], out, is_last ? as_expression : false);
            if (!is_last) {
                if (has_val) out.push_back(Instruction::pop());
            } else {
                return has_val;
            }
        }
        return false;
    }

    std::string part = parts[0];
    if (part.empty()) return false;

    struct AssignInfo {
        std::size_t pos;
        char op;
    };

    AssignInfo asg;
    bool has_assign = false;
    int depth = 0;
    for (std::size_t i = 0; i < part.size(); ++i) {
        char c = part[i];
        if (c == '(' || c == '{' || c == '[') ++depth;
        else if (c == ')' || c == '}' || c == ']') --depth;
        else if (depth == 0) {
            if (c == '=' && (i + 1 >= part.size() || part[i + 1] != '=')) {
                asg.pos = i;
                asg.op = '=';
                has_assign = true;
                break;
            }
            if ((c == '+' || c == '-' || c == '*' || c == '/' || c == '%') &&
                i + 1 < part.size() && part[i + 1] == '=') {
                asg.pos = i;
                asg.op = c;
                has_assign = true;
                break;
            }
        }
    }

    if (has_assign) {
        std::string target = trim(part.substr(0, asg.pos));
        std::string value = trim(part.substr(asg.pos + (asg.op == '=' ? 1 : 2)));

        std::size_t bracket = target.find('[');
        if (bracket != std::string::npos && target.back() == ']') {
            std::string arr_name = trim(target.substr(0, bracket));
            if (arr_name.empty()) { parseError(); return false; }
            std::string index_expr = target.substr(bracket + 1, target.size() - bracket - 2);

            if (asg.op == '=') {
                std::size_t before_val = out.size();
                bool val_ok = compileAssignExpr(value, out, true);
                if (out.size() > before_val && out.back().op == Op::ArrayLiteral && out.back().name.rfind("__arr_lit_", 0) == 0) {
                    out.back().name = arr_name;
                    return false;
                }
                if (!val_ok) return false;
                if (as_expression) out.push_back(Instruction::dup());
                if (!compileAssignExpr(index_expr, out, true)) return false;
                out.push_back(Instruction::arraySet(arr_name));
                return as_expression;
            } else {
                if (!compileAssignExpr(index_expr, out, true)) return false;
                out.push_back(Instruction::dup());
                out.push_back(Instruction::arrayGet(arr_name));

                std::size_t before_arr = out.size();
                bool arr_val_ok = compileAssignExpr(value, out, true);
                if (out.size() > before_arr && out.back().op == Op::ArrayLiteral && out.back().name.rfind("__arr_lit_", 0) == 0) {
                    out.back().name = arr_name;
                    return false;
                }
                if (!arr_val_ok) return false;

                if (asg.op == '+') out.push_back(Instruction::makeOp(Op::Add));
                else if (asg.op == '-') out.push_back(Instruction::makeOp(Op::Sub));
                else if (asg.op == '*') out.push_back(Instruction::makeOp(Op::Mul));
                else if (asg.op == '/') out.push_back(Instruction::makeOp(Op::Div));
                else if (asg.op == '%') out.push_back(Instruction::makeOp(Op::Mod));

                if (as_expression) {
                    out.push_back(Instruction::dup());
                    out.push_back(Instruction::rot());
                    out.push_back(Instruction::arraySet(arr_name));
                } else {
                    out.push_back(Instruction::swap());
                    out.push_back(Instruction::arraySet(arr_name));
                }
                return as_expression;
            }
        } else {
            if (asg.op == '=') {
                std::size_t before = out.size();
                bool val_ok = compileAssignExpr(value, out, true);
                if (out.size() > before && out.back().op == Op::ArrayLiteral && out.back().name.rfind("__arr_lit_", 0) == 0) {
                    out.back().name = target;
                    return false;
                } else if (val_ok) {
                    if (as_expression) out.push_back(Instruction::dup());
                    out.push_back(Instruction::setVar(target));
                    return as_expression;
                } else {
                    return false;
                }
            } else {
                std::size_t before = out.size();
                bool val_ok = compileAssignExpr(value, out, true);
                if (out.size() > before && out.back().op == Op::ArrayLiteral && out.back().name.rfind("__arr_lit_", 0) == 0) {
                    out.back().name = target;
                    return false;
                }
                out.push_back(Instruction::getVar(target));
                if (!val_ok) return false;
                if (asg.op == '+') out.push_back(Instruction::makeOp(Op::Add));
                else if (asg.op == '-') out.push_back(Instruction::makeOp(Op::Sub));
                else if (asg.op == '*') out.push_back(Instruction::makeOp(Op::Mul));
                else if (asg.op == '/') out.push_back(Instruction::makeOp(Op::Div));
                else if (asg.op == '%') out.push_back(Instruction::makeOp(Op::Mod));
                if (as_expression) out.push_back(Instruction::dup());
                out.push_back(Instruction::setVar(target));
                return as_expression;
            }
        }
    } else {
        std::size_t result = parseTernary(part, 0, out);
        if (result == std::string::npos) return false;
        return true;
    }
}

NativeFunctionRegistry::NativeFunctionRegistry() {
    initBuiltins();
}

NativeFunctionRegistry& NativeFunctionRegistry::instance() {
    static NativeFunctionRegistry inst;
    return inst;
}

void NativeFunctionRegistry::registerFunction(const std::string& name, NativeFunc func,
                                               std::initializer_list<NativeValueType> types) {
    NativeFunctionInfo info;
    info.func = std::move(func);
    info.paramTypes = types;
    info.hasTypeInfo = types.size() > 0;
    functions_[name] = std::move(info);
}

bool NativeFunctionRegistry::hasFunction(const std::string& name) const {
    return functions_.find(name) != functions_.end();
}

NativeValue NativeFunctionRegistry::callFunction(const std::string& name,
                                            const std::vector<NativeValue>& args) {
    auto it = functions_.find(name);
    if (it != functions_.end()) {
        return it->second.func(args);
    }
    return NativeValue(0.0);
}

const NativeFunctionInfo*
NativeFunctionRegistry::getFunctionInfo(const std::string& name) const {
    auto it = functions_.find(name);
    if (it != functions_.end()) {
        return &it->second;
    }
    return nullptr;
}

void NativeFunctionRegistry::registerVariable(const std::string& name, NativeValue value) {
    NativeVariableInfo info;
    info.value = std::move(value);
    variables_[name] = std::move(info);
}

bool NativeFunctionRegistry::hasVariable(const std::string& name) const {
    return variables_.find(name) != variables_.end();
}

NativeValue NativeFunctionRegistry::getVariableValue(const std::string& name) const {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return it->second.value;
    }
    return NativeValue(0.0);
}

void NativeFunctionRegistry::setVariableValue(const std::string& name, const NativeValue& value) {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        it->second.value = value;
    }
}

void NativeFunctionRegistry::registerDynamicFunction(const std::string& name,
                                                     std::initializer_list<NativeValueType> types) {
    NativeFunctionInfo info;
    info.paramTypes = types;
    info.hasTypeInfo = types.size() > 0;
    dynamic_functions_[name] = std::move(info);
}

bool NativeFunctionRegistry::hasDynamicFunction(const std::string& name) const {
    return dynamic_functions_.find(name) != dynamic_functions_.end();
}

const NativeFunctionInfo*
NativeFunctionRegistry::getDynamicFunctionInfo(const std::string& name) const {
    auto it = dynamic_functions_.find(name);
    if (it != dynamic_functions_.end()) {
        return &it->second;
    }
    return nullptr;
}

void NativeFunctionRegistry::registerDynamicVariable(const std::string& name, NativeValueType type) {
    dynamic_variables_[name] = type;
}

bool NativeFunctionRegistry::hasDynamicVariable(const std::string& name) const {
    return dynamic_variables_.find(name) != dynamic_variables_.end();
}

NativeValueType NativeFunctionRegistry::getDynamicVariableType(const std::string& name) const {
    auto it = dynamic_variables_.find(name);
    if (it != dynamic_variables_.end()) {
        return it->second;
    }
    return NativeValueType::Double;
}

void NativeFunctionRegistry::registerIdent(const std::string& name, IdentHandler handler) {
    ident_registry_[name] = std::move(handler);
}

void NativeFunctionRegistry::registerIdentOp(const std::string& name, Op op,
                                          std::initializer_list<const char*> aliases) {
    ident_registry_[name] = [op](std::vector<Instruction>& out) {
        out.push_back(Instruction::makeOp(op));
    };
    for (auto* alias : aliases) {
        ident_registry_[alias] = [op](std::vector<Instruction>& out) {
            out.push_back(Instruction::makeOp(op));
        };
    }
}

void NativeFunctionRegistry::registerBuiltin(const std::string& name, BuiltinHandler handler) {
    builtin_registry_[name] = std::move(handler);
}

void NativeFunctionRegistry::registerBuiltinOp(const std::string& name, Op op,
                                            std::initializer_list<const char*> aliases) {
    auto handler = [op](const std::vector<std::string>& args,
                         std::vector<Instruction>& out,
                         PixelblazeCompiler& compiler) {
        PixelblazeCompiler::compileArgs(args, out, compiler);
        out.push_back(Instruction::makeOp(op));
    };
    builtin_registry_[name] = handler;
    for (auto* alias : aliases) {
        builtin_registry_[alias] = handler;
    }
}

bool NativeFunctionRegistry::isBuiltin(const std::string& name) const {
    return builtin_registry_.count(name) > 0;
}

bool NativeFunctionRegistry::isIdent(const std::string& name) const {
    return ident_registry_.count(name) > 0;
}

void NativeFunctionRegistry::initBuiltins() {
    registerIdentOp("pixelCount", Op::GetPixelCount);
    registerIdentOp("pixelIndex", Op::GetPixelIndex);
    registerIdentOp("pixelX", Op::GetPixelX);
    registerIdentOp("pixelY", Op::GetPixelY);
    registerIdentOp("pixelZ", Op::GetPixelZ);
    registerIdentOp("gridWidth", Op::GetGridWidth);
    registerIdentOp("gridHeight", Op::GetGridHeight);
    registerIdentOp("gridPixelCount", Op::GetGridPixelCount);
    registerIdentOp("delta", Op::GetDelta);
    registerIdent("PI", [](std::vector<Instruction>& out) {
        out.push_back(Instruction::push(M_PI));
    });
    registerIdent("Math.PI", [](std::vector<Instruction>& out) {
        out.push_back(Instruction::push(M_PI));
    });
    registerIdent("TWO_PI", [](std::vector<Instruction>& out) {
        out.push_back(Instruction::push(2.0 * M_PI));
    });
    registerIdent("Math", [](std::vector<Instruction>& out) {
        out.push_back(Instruction::push(0.0));
    });
    registerIdent("performance", [](std::vector<Instruction>& out) {
        out.push_back(Instruction::push(0.0));
    });

    registerBuiltinOp("sin", Op::Sin);
    registerBuiltinOp("cos", Op::Cos);
    registerBuiltinOp("tan", Op::Tan);
    registerBuiltinOp("asin", Op::Asin);
    registerBuiltinOp("acos", Op::Acos);
    registerBuiltinOp("atan", Op::Atan);
    registerBuiltinOp("wave", Op::Wave);
    registerBuiltinOp("triangle", Op::Triangle);
    registerBuiltinOp("sawtooth", Op::Sawtooth);
    registerBuiltinOp("square", Op::Square);
    registerBuiltinOp("noise", Op::Noise1D, {"noise1D", "noise_1d"});
    registerBuiltinOp("abs", Op::Abs);
    registerBuiltinOp("time", Op::Time);
    registerBuiltinOp("rgb", Op::Rgb);
    registerBuiltinOp("hsv", Op::Hsv);
    registerBuiltinOp("okhsl", Op::Okhsl);
    registerBuiltinOp("log", Op::Log, {"console_log"});
    registerBuiltinOp("min", Op::Min);
    registerBuiltinOp("max", Op::Max);
    registerBuiltinOp("clamp", Op::Clamp);
    registerBuiltinOp("floor", Op::Floor);
    registerBuiltinOp("ceil", Op::Ceil);
    registerBuiltinOp("round", Op::Round);
    registerBuiltinOp("pow", Op::Pow);
    registerBuiltinOp("sqrt", Op::Sqrt);
    registerBuiltinOp("lerp", Op::Lerp);
    registerBuiltinOp("map", Op::Map);
    registerBuiltinOp("constrain", Op::Constrain);
    registerBuiltinOp("mix", Op::Mix);
    registerBuiltinOp("randomRange", Op::RandomRange);

    auto random_handler = [](const std::vector<std::string>& args,
                              std::vector<Instruction>& out,
                              PixelblazeCompiler& compiler) {
        if (args.empty()) {
            out.push_back(Instruction::makeOp(Op::Random));
        } else {
            PixelblazeCompiler::compileArgs(args, out, compiler);
            out.push_back(Instruction::makeOp(Op::RandomRange));
        }
    };
    registerBuiltin("random", random_handler);
    registerBuiltin("random16", random_handler);

    registerBuiltin("storage", [](const std::vector<std::string>& args,
                                  std::vector<Instruction>& out,
                                  PixelblazeCompiler& compiler) {
        std::string key;
        if (!args.empty()) {
            key = PixelblazeCompiler::trim(args[0]);
            if (key.size() >= 2 && (key.front() == '"' || key.front() == '\''))
                key = key.substr(1, key.size() - 2);
            for (std::size_t i = 1; i < args.size(); ++i) {
                if (!compiler.compileAssignExpr(PixelblazeCompiler::trim(args[i]), out, true)) {
                    compiler.parseError();
                    return;
                }
            }
        }
        if (args.size() < 2) {
            out.push_back(Instruction::push(0.0));
        }
        out.push_back(Instruction::makeOp(Op::StorageGet));
        out.back().name = key;
    });

    registerBuiltin("storageSet", [](const std::vector<std::string>& args,
                                      std::vector<Instruction>& out,
                                      PixelblazeCompiler& compiler) {
        std::string key;
        if (!args.empty()) {
            key = PixelblazeCompiler::trim(args[0]);
            if (key.size() >= 2 && (key.front() == '"' || key.front() == '\''))
                key = key.substr(1, key.size() - 2);
            for (std::size_t i = 1; i < args.size(); ++i) {
                if (!compiler.compileAssignExpr(PixelblazeCompiler::trim(args[i]), out, true)) {
                    compiler.parseError();
                    return;
                }
            }
        }
        out.push_back(Instruction::makeOp(Op::StorageSet));
        out.back().name = key;
    });

    registerBuiltin("storageGetStr", [](const std::vector<std::string>& args,
                                         std::vector<Instruction>& out,
                                         PixelblazeCompiler& compiler) {
        std::string key;
        if (!args.empty()) {
            key = PixelblazeCompiler::trim(args[0]);
            if (key.size() >= 2 && (key.front() == '"' || key.front() == '\''))
                key = key.substr(1, key.size() - 2);
            for (std::size_t i = 1; i < args.size(); ++i) {
                if (!compiler.compileAssignExpr(PixelblazeCompiler::trim(args[i]), out, true)) {
                    compiler.parseError();
                    return;
                }
            }
        }
        if (args.size() < 2) {
            out.push_back(Instruction::makeString(""));
        }
        out.push_back(Instruction::makeOp(Op::StorageGetStr));
        out.back().name = key;
    });

    registerBuiltin("storageSetStr", [](const std::vector<std::string>& args,
                                         std::vector<Instruction>& out,
                                         PixelblazeCompiler& compiler) {
        std::string key;
        if (!args.empty()) {
            key = PixelblazeCompiler::trim(args[0]);
            if (key.size() >= 2 && (key.front() == '"' || key.front() == '\''))
                key = key.substr(1, key.size() - 2);
            for (std::size_t i = 1; i < args.size(); ++i) {
                if (!compiler.compileAssignExpr(PixelblazeCompiler::trim(args[i]), out, true)) {
                    compiler.parseError();
                    return;
                }
            }
        }
        out.push_back(Instruction::makeOp(Op::StorageSetStr));
        out.back().name = key;
    });

    registerBuiltin("array", [](const std::vector<std::string>& args,
                                 std::vector<Instruction>& out,
                                 PixelblazeCompiler& compiler) {
        for (const auto& arg : args) {
            if (!compiler.compileAssignExpr(PixelblazeCompiler::trim(arg), out, true)) {
                compiler.parseError();
                return;
            }
        }
        out.push_back(Instruction::push(static_cast<double>(args.size())));
        std::string arr_name = "__arr_lit_" + std::to_string(compiler.arr_lit_counter_++);
        out.push_back(Instruction::arrayLiteral(arr_name));
    });
}
}