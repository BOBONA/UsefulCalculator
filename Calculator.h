#pragma once

#include <vector>
#include <deque>
#include <string>
#include <map>

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
};


class Calculator {
private:
    static const char Assignment = '=';

    std::map<std::string, double> variables; // user variables with their calculated values or NaN if not calculated
    std::map<std::string, int> functions; // user functions with their associated input
    std::vector<InputLine*> inputs;

public:
    void AddLine();
    void RemoveLine(int index);
    int LineCount();
    std::string GetFormattedLine(int index);
    void ParseLine(const std::string& line, int index);
    double Evaluate();

    ~Calculator() {
        while (LineCount() != 0) {
            RemoveLine(0);
        }
    }
};