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

Preprocessor::Preprocessor(bool isCompiler) : isCompiler(isCompiler) {
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    
    std::stringstream dateSS;
    dateSS << "\"" << std::put_time(&tm, "%b %d %Y") << "\"";
    preprocDate = dateSS.str();
    
    std::stringstream timeSS;
    timeSS << "\"" << std::put_time(&tm, "%H:%M:%S") << "\"";
    preprocTime = timeSS.str();
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

std::string Preprocessor::processInternal(const std::string& source, const std::string& currentFile, int depth) {
    if (depth > 16) {
        throw std::runtime_error("Maximum include depth exceeded (circular dependency?)");
    }

    std::stringstream input(source);
    std::stringstream output;
    std::string line;
    int lineNum = 0;
    std::string reportedFile = currentFile;

    while (std::getline(input, line)) {
        lineNum++;
        std::string trimmed = trim(line);
        
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
            } else if (cmd == "#elif") {
                if (stateStack.size() <= 1) throw std::runtime_error("Unexpected #elif");
                State& top = stateStack.back();
                if (top.inElse) throw std::runtime_error("Unexpected #elif after #else");
                
                bool parentActive = true;
                for (size_t i = 0; i < stateStack.size() - 1; ++i) {
                    if (!stateStack[i].active) { parentActive = false; break; }
                }

                std::string name;
                ss >> name;
                bool exists = (symbols.find(name) != symbols.end());

                if (!top.hasSeenTrueBranch && parentActive && exists) {
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
                    size_t firstQuote = line.find('"');
                    size_t lastQuote = line.rfind('"');
                    if (firstQuote != std::string::npos && lastQuote != std::string::npos && firstQuote < lastQuote) {
                        fileName = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                    } else {
                        size_t firstAngle = line.find('<');
                        size_t lastAngle = line.rfind('>');
                        if (firstAngle != std::string::npos && lastAngle != std::string::npos && firstAngle < lastAngle) {
                            fileName = line.substr(firstAngle + 1, lastAngle - firstAngle - 1);
                        }
                    }

                    if (fileName.empty()) throw std::runtime_error("Invalid #include directive");

                    std::string fullPath = findIncludeFile(fileName, getDirectory(currentFile));
                    if (fullPath.empty()) throw std::runtime_error("Could not find include file: " + fileName);

                    // To prevent infinite recursion of same file at different levels
                    if (includedFiles.count(fullPath)) {
                         // Some preprocessors allow this if it's not a direct recursion, but here we'll be strict
                         // Actually, common practice is to use include guards. 
                         // Let's just allow it and let the depth check handle infinite recursion.
                    }
                    
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
                // Pragmas are implementation-defined, we'll just ignore for now
                output << "\n";
            } else {
                // Unknown directive, if active just pass it through or ignore?
                // For now, ignore if it starts with # but isn't one of ours
                output << "\n";
            }
        } else {
            if (isConditionTrue()) {
                std::string processed = line;
                
                // Expand standard predefined macros
                auto replaceAll = [&](std::string& s, const std::string& from, const std::string& to) {
                    size_t pos = 0;
                    while ((pos = s.find(from, pos)) != std::string::npos) {
                        // Ensure it's a whole word to avoid replacing parts of other tokens
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
