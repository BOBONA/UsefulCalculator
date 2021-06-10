#pragma once

#include <vector>
#include <deque>
#include <string>
#include <map>
#include <set>

enum ItemType {
    Operand,
    OperandSymbol,
    Variable,
    Function,
    UserFunction,
    Other
};

enum InputLineType {
    ILVariable,
    ILFunction,
    Expression
};

struct PostfixItem {
    ItemType type;
    std::string name;
    double value;
};

struct InputLine {
    InputLineType type;
    std::string identifier;
    std::vector<std::string> arguments;
    std::deque<PostfixItem> postfix;
    std::string source;
    bool failed;
};


class Calculator {
private:
    static const char Assignment = '=';
    static const int Last = -1;

    std::map<std::string, int> variables; // user variables with their associated input
    std::map<std::string, int> functions; // user functions with their associated input
    std::vector<InputLine*> inputs;
    int evaluateLine = Last;

public:
    void AddLine(int index);
    void RemoveLine(int index);
    int LineCount();
    std::string GetFormattedLine(int index);
    void ParseLine(const std::string& line, int index);
    double EvaluatePostfix(std::deque<PostfixItem> items);
    double EvaluatePostfix(std::deque<PostfixItem> items, std::set<std::string>& processedIdentifiers, std::map<std::string, double>& calculatedVariables);
    void SetEvaluateLine(int index);
    std::deque<PostfixItem> GetExpandedPostfix(std::deque<PostfixItem> items);
    std::deque<PostfixItem> GetExpandedPostfix(std::deque<PostfixItem> items, std::set<std::string>& processed);

    ~Calculator() {
        while (LineCount() != 0) {
            RemoveLine(0);
        }
    }
};