#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>

class Preprocessor {
public:
    Preprocessor(bool isCompiler = false);
    
    // Process the source code.
    // symbols: initial set of defined symbols (e.g. from -D flags)
    // includePaths: directories to search for #include files
    // currentFile: path to the file being processed (used for relative includes)
    std::string process(const std::string& source, 
                        const std::map<std::string, std::string>& initialSymbols,
                        const std::vector<std::string>& includePaths,
                        const std::string& currentFile);

private:
    bool isCompiler;
    
    struct Macro {
        bool isFunctionLike = false;
        std::vector<std::string> params;
        std::string body;
    };
    std::map<std::string, Macro> macros;
    
    std::vector<std::string> includePaths;
    std::set<std::string> includedFiles; // To prevent infinite recursion
    std::set<std::string> onceFiles;     // To support #includeonce
    std::string preprocDate;
    std::string preprocTime;
    
    struct State {
        bool active = true;
        bool hasSeenTrueBranch = false;
        bool inElse = false;
    };
    std::vector<State> stateStack;

    std::string processInternal(const std::string& source, const std::string& currentFile, int depth);
    bool isConditionTrue();
    long evaluateExpression(const std::string& expr);
    std::string expandMacros(const std::string& line);
    std::string stripComments(const std::string& line, bool& inBlockComment);
    std::string getDirectory(const std::string& filePath);
    std::string findIncludeFile(const std::string& fileName, const std::string& currentDir);
    std::string trim(const std::string& s);
};
