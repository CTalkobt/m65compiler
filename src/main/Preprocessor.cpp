#include "Preprocessor.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

#include <iostream>
#include <ctime>
#include <iomanip>
#include <functional>

Preprocessor::Preprocessor(bool isCompiler) : isCompiler(isCompiler) {
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    
    std::stringstream dateSS;
    dateSS << "\"" << std::put_time(&tm, "%b %d %Y") << "\"";
    preprocDate = dateSS.str();
    
    std::stringstream timeSS;
    timeSS << "\"" << std::put_time(&tm, "%H:%M:%S") << "\"";
    preprocTime = timeSS.str();

    // Standard predefined macros
    symbols["__STDC__"] = "1";
    symbols["__STDC_VERSION__"] = "201112L";
    symbols["__STDC_HOSTED__"] = "0"; // Cross-compiler
}

// Simple recursive descent evaluator for #if expressions
struct ExprPath {
    std::string s;
    size_t pos = 0;
    
    void skipWhitespace() {
        while (pos < s.length() && std::isspace((unsigned char)s[pos])) pos++;
    }
    
    std::string nextToken() {
        skipWhitespace();
        if (pos >= s.length()) return "";
        if (std::isalpha((unsigned char)s[pos]) || s[pos] == '_') {
            size_t start = pos;
            while (pos < s.length() && (std::isalnum((unsigned char)s[pos]) || s[pos] == '_')) pos++;
            return s.substr(start, pos - start);
        }
        if (std::isdigit((unsigned char)s[pos])) {
            size_t start = pos;
            while (pos < s.length() && std::isdigit((unsigned char)s[pos])) pos++;
            return s.substr(start, pos - start);
        }
        if (pos + 1 < s.length()) {
            std::string op2 = s.substr(pos, 2);
            if (op2 == "&&" || op2 == "||" || op2 == "==" || op2 == "!=" || op2 == "<=" || op2 == ">=") {
                pos += 2;
                return op2;
            }
        }
        return std::string(1, s[pos++]);
    }
    
    std::string peekToken() {
        size_t oldPos = pos;
        std::string t = nextToken();
        pos = oldPos;
        return t;
    }
};

long Preprocessor::evaluateExpression(const std::string& expr) {
    // 1. Handle defined(SYMBOL)
    std::string e = expr;
    auto replaceDefined = [&](std::string& s) {
        size_t pos = 0;
        while ((pos = s.find("defined", pos)) != std::string::npos) {
            // Ensure whole word
            bool bBefore = (pos == 0 || (!std::isalnum((unsigned char)s[pos-1]) && s[pos-1] != '_'));
            bool bAfter = (pos + 7 == s.length() || (!std::isalnum((unsigned char)s[pos+7]) && s[pos+7] != '_'));
            if (!bBefore || !bAfter) { pos += 7; continue; }
            
            size_t startPos = pos;
            pos += 7;
            while (pos < s.length() && std::isspace((unsigned char)s[pos])) pos++;
            
            bool hasParen = false;
            if (pos < s.length() && s[pos] == '(') {
                hasParen = true;
                pos++;
                while (pos < s.length() && std::isspace((unsigned char)s[pos])) pos++;
            }
            
            size_t symStart = pos;
            while (pos < s.length() && (std::isalnum((unsigned char)s[pos]) || s[pos] == '_')) pos++;
            std::string sym = s.substr(symStart, pos - symStart);
            
            if (hasParen) {
                while (pos < s.length() && std::isspace((unsigned char)s[pos])) pos++;
                if (pos < s.length() && s[pos] == ')') pos++;
            }
            
            std::string val = (symbols.find(sym) != symbols.end()) ? "1" : "0";
            s.replace(startPos, pos - startPos, val);
            pos = startPos + val.length();
        }
    };
    replaceDefined(e);

    // 2. Simple Recursive Descent (Logical OR -> AND -> Relational -> Additive -> Multiplicative -> Unary -> Primary)
    ExprPath p{e};
    
    std::function<long()> parseOr;
    std::function<long()> parseAnd;
    std::function<long()> parseRel;
    std::function<long()> parseAdd;
    std::function<long()> parseMul;
    std::function<long()> parseUnary;
    std::function<long()> parsePrimary;

    parsePrimary = [&]() -> long {
        std::string t = p.nextToken();
        if (t == "(") {
            long val = parseOr();
            if (p.nextToken() != ")") {} // Error
            return val;
        }
        if (isdigit((unsigned char)t[0])) return std::stol(t, nullptr, 0);
        // Symbols not in defined() but in #if are treated as 0
        if (symbols.count(t)) {
            try { return std::stol(symbols[t], nullptr, 0); } catch(...) { return 0; }
        }
        return 0;
    };

    parseUnary = [&]() -> long {
        std::string t = p.peekToken();
        if (t == "!") { p.nextToken(); return !parseUnary(); }
        if (t == "-") { p.nextToken(); return -parseUnary(); }
        if (t == "+") { p.nextToken(); return parseUnary(); }
        return parsePrimary();
    };

    parseMul = [&]() -> long {
        long left = parseUnary();
        while (true) {
            std::string t = p.peekToken();
            if (t == "*") { p.nextToken(); left *= parseUnary(); }
            else if (t == "/") { p.nextToken(); long right = parseUnary(); left = right ? left / right : 0; }
            else break;
        }
        return left;
    };

    parseAdd = [&]() -> long {
        long left = parseMul();
        while (true) {
            std::string t = p.peekToken();
            if (t == "+") { p.nextToken(); left += parseMul(); }
            else if (t == "-") { p.nextToken(); left -= parseMul(); }
            else break;
        }
        return left;
    };

    parseRel = [&]() -> long {
        long left = parseAdd();
        while (true) {
            std::string t = p.peekToken();
            if (t == "==") { p.nextToken(); left = (left == parseAdd()); }
            else if (t == "!=") { p.nextToken(); left = (left != parseAdd()); }
            else if (t == "<") { p.nextToken(); left = (left < parseAdd()); }
            else if (t == ">") { p.nextToken(); left = (left > parseAdd()); }
            else if (t == "<=") { p.nextToken(); left = (left <= parseAdd()); }
            else if (t == ">=") { p.nextToken(); left = (left >= parseAdd()); }
            else break;
        }
        return left;
    };

    parseAnd = [&]() -> long {
        long left = parseRel();
        while (p.peekToken() == "&&") { p.nextToken(); left = (parseRel() && left); }
        return left;
    };

    parseOr = [&]() -> long {
        long left = parseAnd();
        while (p.peekToken() == "||") { p.nextToken(); left = (parseAnd() || left); }
        return left;
    };

    return parseOr();
}

std::string Preprocessor::process(const std::string& source, 
                                  const std::map<std::string, std::string>& initialSymbols,
                                  const std::vector<std::string>& includePaths,
                                  const std::string& currentFile) {
    this->symbols = initialSymbols;
    this->includePaths = includePaths;
    this->includedFiles.clear();
    this->stateStack.clear();
    
    // Default initial state: everything is active.
    stateStack.push_back({true, true, false});
    
    return processInternal(source, currentFile, 0);
}

bool Preprocessor::isConditionTrue() {
    for (const auto& s : stateStack) {
        if (!s.active) return false;
    }
    return true;
}

std::string Preprocessor::trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string Preprocessor::getDirectory(const std::string& filePath) {
    return fs::path(filePath).parent_path().string();
}

std::string Preprocessor::findIncludeFile(const std::string& fileName, const std::string& currentDir) {
    // 1. Check current directory
    fs::path p1 = fs::path(currentDir) / fileName;
    if (fs::exists(p1)) return p1.string();
    
    // 2. Check include paths
    for (const auto& dir : includePaths) {
        fs::path p = fs::path(dir) / fileName;
        if (fs::exists(p)) return p.string();
    }
    
    return "";
}

std::string Preprocessor::stripComments(const std::string& line, bool& inBlockComment) {
    std::string result;
    bool inString = false;
    bool inChar = false;
    
    for (size_t i = 0; i < line.length(); ++i) {
        if (inBlockComment) {
            if (i + 1 < line.length() && line[i] == '*' && line[i+1] == '/') {
                inBlockComment = false;
                i++;
                result += ' ';
            }
            continue;
        }
        if (inString) {
            result += line[i];
            if (line[i] == '\\' && i + 1 < line.length()) {
                result += line[i+1];
                i++;
            } else if (line[i] == '"') {
                inString = false;
            }
            continue;
        }
        if (inChar) {
            result += line[i];
            if (line[i] == '\\' && i + 1 < line.length()) {
                result += line[i+1];
                i++;
            } else if (line[i] == '\'') {
                inChar = false;
            }
            continue;
        }
        
        if (i + 1 < line.length() && line[i] == '/' && line[i+1] == '*') {
            inBlockComment = true;
            i++;
            result += ' ';
        } else if (i + 1 < line.length() && line[i] == '/' && line[i+1] == '/') {
            result += ' ';
            break;
        } else if (line[i] == '"') {
            inString = true;
            result += '"';
        } else if (line[i] == '\'') {
            inChar = true;
            result += '\'';
        } else {
            result += line[i];
        }
    }
    return result;
}

std::string Preprocessor::processInternal(const std::string& source, const std::string& currentFile, int depth) {
    if (depth > 16) {
        throw std::runtime_error("Maximum include depth exceeded (circular dependency?)");
    }

    std::stringstream input(source);
    std::stringstream output;
    std::string line;
    int lineNum = 0;
    std::string reportedFile = currentFile;
    std::string accumulatedLine = "";
    bool inBlockComment = false;

    while (std::getline(input, line)) {
        lineNum++;
        
        // Handle line continuation (Phase 2)
        std::string trimmedRaw = trim(line);
        if (!trimmedRaw.empty() && trimmedRaw.back() == '\\') {
            accumulatedLine += line.substr(0, line.find_last_of('\\'));
            output << "\n"; // Maintain line counts
            continue;
        }
        
        std::string currentLineRaw = accumulatedLine + line;
        accumulatedLine = "";
        
        // Strip comments (Phase 3)
        std::string currentLine = stripComments(currentLineRaw, inBlockComment);
        std::string trimmed = trim(currentLine);
        
        // Handle compiler pass-through #!
        if (isCompiler && trimmed.substr(0, 2) == "#!") {
            if (isConditionTrue()) {
                std::string directive = "#" + trimmed.substr(2);
                output << "__asm__(\"" << directive << "\");\n";
            } else {
                output << "\n"; // Maintain line numbers
            }
            continue;
        }

        if (!trimmed.empty() && trimmed[0] == '#') {
            std::stringstream ss(trimmed);
            std::string cmd;
            ss >> cmd;
            
            if (cmd == "#define") {
                if (isConditionTrue()) {
                    std::string name;
                    ss >> name;
                    std::string value;
                    std::getline(ss, value);
                    symbols[name] = trim(value);
                }
                output << "\n";
            } else if (cmd == "#undef") {
                if (isConditionTrue()) {
                    std::string name;
                    ss >> name;
                    symbols.erase(name);
                }
                output << "\n";
            } else if (cmd == "#ifdef") {
                std::string name;
                ss >> name;
                bool exists = (symbols.find(name) != symbols.end());
                bool parentActive = isConditionTrue();
                stateStack.push_back({parentActive && exists, exists, false});
                output << "\n";
            } else if (cmd == "#ifndef") {
                std::string name;
                ss >> name;
                bool exists = (symbols.find(name) != symbols.end());
                bool parentActive = isConditionTrue();
                stateStack.push_back({parentActive && !exists, !exists, false});
                output << "\n";
            } else if (cmd == "#if") {
                std::string expr;
                std::getline(ss, expr);
                bool val = evaluateExpression(expr);
                bool parentActive = isConditionTrue();
                stateStack.push_back({parentActive && val, val, false});
                output << "\n";
            } else if (cmd == "#elif") {
                if (stateStack.size() <= 1) throw std::runtime_error("Unexpected #elif");
                State& top = stateStack.back();
                if (top.inElse) throw std::runtime_error("Unexpected #elif after #else");
                
                bool parentActive = true;
                for (size_t i = 0; i < stateStack.size() - 1; ++i) {
                    if (!stateStack[i].active) { parentActive = false; break; }
                }

                std::string expr;
                std::getline(ss, expr);
                bool val = evaluateExpression(expr);

                if (!top.hasSeenTrueBranch && parentActive && val) {
                    top.active = true;
                    top.hasSeenTrueBranch = true;
                } else {
                    top.active = false;
                }
                output << "\n";
            } else if (cmd == "#else") {
                if (stateStack.size() <= 1) throw std::runtime_error("Unexpected #else");
                State& top = stateStack.back();
                if (top.inElse) throw std::runtime_error("Multiple #else for one #ifdef");
                
                bool parentActive = true;
                for (size_t i = 0; i < stateStack.size() - 1; ++i) {
                    if (!stateStack[i].active) { parentActive = false; break; }
                }

                top.active = parentActive && !top.hasSeenTrueBranch;
                top.inElse = true;
                output << "\n";
            } else if (cmd == "#endif") {
                if (stateStack.size() <= 1) throw std::runtime_error("Unexpected #endif");
                stateStack.pop_back();
                output << "\n";
            } else if (cmd == "#include") {
                if (isConditionTrue()) {
                    std::string fileName;
                    // Expect "filename" or <filename>
                    size_t firstQuote = currentLine.find('"');
                    size_t lastQuote = currentLine.rfind('"');
                    if (firstQuote != std::string::npos && lastQuote != std::string::npos && firstQuote < lastQuote) {
                        fileName = currentLine.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                    } else {
                        size_t firstAngle = currentLine.find('<');
                        size_t lastAngle = currentLine.rfind('>');
                        if (firstAngle != std::string::npos && lastAngle != std::string::npos && firstAngle < lastAngle) {
                            fileName = currentLine.substr(firstAngle + 1, lastAngle - firstAngle - 1);
                        }
                    }

                    if (fileName.empty()) throw std::runtime_error("Invalid #include directive");

                    std::string fullPath = findIncludeFile(fileName, getDirectory(currentFile));
                    if (fullPath.empty()) throw std::runtime_error("Could not find include file: " + fileName);

                    if (includedFiles.count(fullPath)) {}
                    
                    std::ifstream includeFile(fullPath);
                    if (!includeFile.is_open()) throw std::runtime_error("Failed to open include file: " + fullPath);
                    
                    std::stringstream buffer;
                    buffer << includeFile.rdbuf();
                    output << processInternal(buffer.str(), fullPath, depth + 1);
                } else {
                    output << "\n";
                }
            } else if (cmd == "#line") {
                if (isConditionTrue()) {
                    int newLineNum;
                    if (ss >> newLineNum) {
                        lineNum = newLineNum - 1;
                        std::string newFile;
                        if (ss >> newFile) {
                            if (newFile.size() >= 2 && newFile.front() == '"' && newFile.back() == '"') {
                                reportedFile = newFile.substr(1, newFile.size() - 2);
                            }
                        }
                    }
                }
                output << "\n";
            } else if (cmd == "#error") {
                if (isConditionTrue()) {
                    std::string msg;
                    std::getline(ss, msg);
                    throw std::runtime_error("#error: " + trim(msg));
                }
                output << "\n";
            } else if (cmd == "#warning") {
                if (isConditionTrue()) {
                    std::string msg;
                    std::getline(ss, msg);
                    std::cerr << "Warning: " << trim(msg) << " at " << reportedFile << ":" << (lineNum + 1) << std::endl;
                }
                output << "\n";
            } else if (cmd == "#pragma") {
                output << "\n";
            } else {
                output << "\n";
            }
        } else {
            if (isConditionTrue()) {
                std::string processed = currentLine;
                
                auto replaceAll = [&](std::string& s, const std::string& from, const std::string& to) {
                    size_t pos = 0;
                    bool inStr = false;
                    while ((pos = s.find(from, pos)) != std::string::npos) {
                        // Semantic awareness: Check if we are inside a string literal
                        inStr = false;
                        for (size_t k = 0; k < pos; ++k) {
                            if (s[k] == '"' && (k == 0 || s[k-1] != '\\')) inStr = !inStr;
                        }
                        
                        if (inStr) {
                            pos += from.length();
                            continue;
                        }

                        bool boundaryBefore = (pos == 0 || (!std::isalnum((unsigned char)s[pos-1]) && s[pos-1] != '_'));
                        bool boundaryAfter = (pos + from.length() == s.length() || (!std::isalnum((unsigned char)s[pos + from.length()]) && s[pos + from.length()] != '_'));
                        
                        if (boundaryBefore && boundaryAfter) {
                            s.replace(pos, from.length(), to);
                            pos += to.length();
                        } else {
                            pos += from.length();
                        }
                    }
                };

                replaceAll(processed, "__FILE__", "\"" + reportedFile + "\"");
                replaceAll(processed, "__LINE__", std::to_string(lineNum));
                replaceAll(processed, "__DATE__", preprocDate);
                replaceAll(processed, "__TIME__", preprocTime);

                output << processed << "\n";
            } else {
                output << "\n";
            }
        }
    }

    return output.str();
}
