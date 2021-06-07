#include <cctype>
#include <stdexcept>
#include <functional>
#include <cmath>
#include <limits>

#include "Calculator.h"

static const double NaN = std::numeric_limits<double>::quiet_NaN();

template <typename V_T>
static std::vector<std::string> getMapKeys(std::map<std::string, V_T> map) {
    std::vector<std::string> v;
    for (std::pair<std::string, V_T> p : map) {
        v.push_back(p.first);
    }
    return v;
}

static const std::map<std::string, double> operands = {
    {"pi", 3.14159265358979323846}
};

static const std::vector<std::string> operandKeys = getMapKeys(operands);

enum OpType {
    Operator,
    Postfix,
    Function,
    ParenthesesL,
    ParenthesesR
};

typedef std::vector<double> args;

struct CalcOperator {
    std::function<double(args)> function;
    int precedence;
    int argumentCount;
    OpType optype;
};

static std::pair<std::string, CalcOperator> basicPostfix(std::string name, std::function<double(args)> function) {
    return { name, CalcOperator{function, 1, 1, OpType::Postfix} };
}

static std::pair<std::string, CalcOperator> basicFunction(std::string name, std::function<double(args)> function) {
    return { name, CalcOperator{function, 0, 1, OpType::Function} };
}

static std::pair<std::string, CalcOperator> basicOperator(std::string name, int precedence, std::function<double(args)> function) {
    return { name, CalcOperator{function, precedence, 2, OpType::Operator} };
}

static const std::map<std::string, CalcOperator> operators = {
    basicPostfix("%", [](auto d) { return d[0] / 100; }),
    basicPostfix("deg", [](auto d) { return d[0] * 0.0174533; }),
    basicFunction("sqrt", [](auto d) { return std::sqrt(d[0]); }),
    basicFunction("sin", [](auto d) { return std::sin(d[0]); }),
    basicOperator("+", -4, [](auto d) { return d[0] + d[1]; }),
    basicOperator("-", -4, [](auto d) { return d[0] - d[1]; }),
    basicOperator("*", -3, [](auto d) { return d[0] * d[1]; }),
    basicOperator("/", -3, [](auto d) { return d[0] / d[1]; }),
    basicOperator("^", -2, [](auto d) { return std::pow(d[0], d[1]); }),
};

static const std::vector<std::string> operatorKeys = getMapKeys(operators);

void Calculator::AddLine() {
    inputs.push_back(new InputLine{});
}

void Calculator::RemoveLine(int index) {
    InputLine* input = inputs.at(index);
    if (input->type == InputLineType::ILVariable) {
        variables.erase(input->identifier);
    } else if (input->type == InputLineType::ILFunction) {
        functions.erase(input->identifier);
    }
    delete input;
    inputs.erase(inputs.begin() + index);
}

int Calculator::LineCount() {
    return inputs.size();
}

std::string Calculator::GetFormattedLine(int index) { 
    InputLine line = *inputs.at(index);
    std::string output{ "" };
    if (line.type != InputLineType::Expression) {
        output += line.identifier;
        if (line.type == InputLineType::ILFunction) {
            output += '(';
            for (auto it{ line.arguments.begin() }; it < line.arguments.end(); it++) {
                output += *it;
                if (it != line.arguments.end() - 1) {
                    output += ", ";
                }
            }
            output += ')';
        }
        output += " ";
        output += Calculator::Assignment;
        output += " ";
    }
    for (PostfixItem item : line.postfix) {
        switch (item.type) {
        case ItemType::IFunction:
        case ItemType::Variable:
        case ItemType::IOperator:
        case ItemType::UserFunction:
            output += item.name;
            break;
        case ItemType::Operand:
        case ItemType::OperandSymbol:
            output += std::to_string(item.value);
            break;
        }
        output += " ";
    }
    return output;
}

void plError(std::string message) {
    throw std::runtime_error(message);
}

// the idea is given a string "1 + sin(x)" and an index of 3, 
// need to identify given a list of "sin, cos, *, +, etc" that 
// the operator (or whatever else needs to be indentified) being used is "sin"
// returns "" if none valid
std::string matchToString(const std::string& str, int index, std::vector<std::string> strings) {
    // the concept is: once a string in strings matches completely, it's added to valid 
    // and removed from strings. If a string doesn't match then it's just removed from strings. 
    // Once strings is empty or str has been iterated over, the shortest valid string is taken as a match
    std::vector<std::string> valid;
    for (size_t i{ 0 }; i < str.length() - index && !strings.empty(); i++) {
        size_t j = 0;
        while (j < strings.size()) { // while loop just for convenience with removing items
            std::string string = strings[j];
            if (string.size() <= i) {
                valid.push_back(string);
                strings.erase(strings.begin() + j);
            } else if (string[i] != str[i + index]) {
                strings.erase(strings.begin() + j);
            } else {
                j++;
            }
        }
    }
    for (std::string s : strings) {
        if (str.substr(index, str.length()) == s) {
            valid.push_back(s);
        }
    }
    std::string longest = "";
    for (std::string s : valid) {
        if (s.size() > longest.size()) {
            longest = s;
        }
    }
    return longest;
}

void Calculator::ParseLine(const std::string& str, int lineIndex) {
    InputLine line{};
    // parse left hand of = sign
    int assignment = str.find(Calculator::Assignment);
    if (assignment != std::string::npos) {
        std::string id = str.substr(0, assignment);
        // populate function / variable info
        std::string name{};
        std::vector<std::string> arguments{};
        int part{ 0 }; // 0: parse name, 1: parse arguments, 2: end
        for (auto it{ id.begin() }; it < id.end() && part != 2; it++) {
            if (iswspace(*it)) {
                continue;
            }
            switch (part) {
            case 0:
                if (isalpha(*it)) {
                    name += *it;
                } else if (*it == '(') {
                    part = 1;
                } else {
                    plError("Expected alphabetical or (");
                }
                break;
            case 1:
                if (isalpha(*it)) {
                    if (arguments.empty()) {
                        arguments.push_back("");
                    }
                    arguments.back() += *it;
                } else if (*it == ',') {
                    if (arguments.empty() || arguments.back().empty()) {
                        plError("Argument name length cannot be 0");
                    }
                    arguments.push_back("");
                } else if (*it == ')') {
                    part = 2;
                } else {
                    plError("Expected alphabetical, ',', or )");
                }
                break;
            }
        }
        if (!arguments.empty() && arguments.back().empty()) {
            arguments.pop_back();
        }
        if (name.empty()) {
            plError("Identifier length cannot be 0");
        } else {
            line.identifier = name;
        }
        if (arguments.empty()) {
            line.type = InputLineType::ILVariable;
        } else {
            line.type = InputLineType::ILFunction;
            line.arguments = arguments;
        }
    } else {
        line.type = InputLineType::Expression;
    }
    // parse the righthand side of the equation
    std::string righthand;
    if (assignment == std::string::npos) {
        righthand = str;
    } else {
        righthand = str.substr(assignment + 1, str.size());
    }
    std::vector<PostfixItem> items{};
    std::vector<CalcOperator> ops{};
    std::deque<PostfixItem> stack{};
    std::deque<PostfixItem>& output = line.postfix;
    int parenLCount{ 0 };
    int parenRCount{ 0 };
    for (unsigned i{ 0 }; i < righthand.length(); i++) {
        char it = righthand.at(i);
        // first have to identify the next token in the string
        if (iswspace(it)) {
            continue;
        }
        PostfixItem item{};
        CalcOperator op{};
        if (isdigit(it) || it == '.') { // parse number if there is one
            double operand{ 0 };
            bool leftside{ true };
            int precision = -1;
            while (i < righthand.size()) {
                it = righthand.at(i);
                if (isdigit(it)) {
                    if (leftside) {
                        operand = operand * 10 + it - '0';
                    } else {
                        operand += (it - '0') * std::pow(10, precision);
                        precision--;
                    }
                } else if (it == '.') {
                    if (!leftside) {
                        plError("Cannot parse value, two '.' in a row");
                    } else {
                        leftside = false;
                    }
                } else {
                    break;
                }
                i++;
            }
            i--;
            item.type = ItemType::Operand;
            item.value = operand;
        } else if (it == '(') {
            item.type = ItemType::Other; 
            item.name = '(';
            op.optype = OpType::ParenthesesL;
            parenLCount++;
        } else if (it == ')') {
            item.type = ItemType::Other;
            item.name = ')';
            op.optype = OpType::ParenthesesR;
            parenRCount++;
        } else {
            // try to match existing operators to string
            std::string matchedOperator = matchToString(righthand, i, operatorKeys);
            std::string matchedOperand = matchToString(righthand, i, operandKeys);
            std::vector<std::string> variableKeys = getMapKeys(variables);
            variableKeys.insert(variableKeys.begin(), line.arguments.begin(), line.arguments.end());
            std::string matchedVariable = matchToString(righthand, i, variableKeys);
            std::string matchedUserFunction = matchToString(righthand, i, getMapKeys(functions));
            if (matchedOperator != "") {
                op = operators.at(matchedOperator);
                if (op.optype == OpType::Function || op.optype == OpType::Postfix) {
                    item.type = ItemType::IFunction;
                } else if (op.optype == OpType::Operator) {
                    item.type = ItemType::IOperator;
                }
                item.name = matchedOperator;
            } else if (matchedOperand != "") {
                item.type = ItemType::OperandSymbol;
                item.value = operands.at(matchedOperand);
            } else if (matchedVariable != "") {
                item.type = ItemType::Variable;
                item.name = matchedVariable;
            } else if (matchedUserFunction != "") {
                item.type = ItemType::UserFunction;
                item.name = matchedUserFunction;
            } else {
                plError("Unknown identifier at index " + std::to_string(i));
            }
            int matchedLength = matchedOperator.length() + matchedOperand.length() + matchedVariable.length() + matchedUserFunction.length();
            i += matchedLength - 1; // if i put it on one line it evaluates as an unsigned int and overflows
        } 
        items.push_back(item);
        ops.push_back(op);
    }
    if (parenLCount != parenRCount) {
        plError("Mismatched parentheses");
    }
    // add * when necessary (like 5x = 5*x)
    for (size_t i{ 1 }; i < items.size(); ++i) {
        if (items[i].type != ItemType::IOperator &&
            items[i].type != ItemType::Operand &&
            ops[i].optype != OpType::Postfix &&
            ops[i].optype != OpType::ParenthesesR &&
            items[i - 1].type != ItemType::IOperator &&
            ops[i - 1].optype != OpType::ParenthesesL) {

            items.insert(items.begin() + i, PostfixItem{ItemType::IOperator, "*", 0});
            ops.insert(ops.begin() + i, operators.at("*"));
            ++i;
        }
    }
    for (size_t i{ 0 }; i < items.size(); ++i) {
        PostfixItem item = items[i];
        CalcOperator op = ops[i];
        // finally use shunting yard procedure
        if (item.type == ItemType::Operand || item.type == ItemType::Variable || item.type == ItemType::OperandSymbol) {
            output.push_back(item);
        } else if (item.type == ItemType::UserFunction) {
            stack.push_back(item);
        } else {
            switch (op.optype) {
            case OpType::Postfix:
                output.push_back(item);
                break;
            case OpType::Function:
                stack.push_back(item);
                break;
            case OpType::Operator:
                while (!stack.empty() && (stack.back().type == ItemType::IOperator || stack.back().type == ItemType::IFunction) && operators.at(stack.back().name).precedence >= op.precedence) {
                    output.push_back(stack.back());
                    stack.pop_back();
                }
                stack.push_back(item);
                break;
            case OpType::ParenthesesL:
                stack.push_back(item);
                break;
            case OpType::ParenthesesR:
                while (!stack.empty() && stack.back().name[0] != '(') {
                    output.push_back(stack.back());
                    stack.pop_back();
                }
                if (!stack.empty()) {
                    stack.pop_back();
                }
                if (!stack.empty() && (stack.back().type == ItemType::IFunction || stack.back().type == ItemType::UserFunction)) {
                    output.push_back(stack.back());
                    stack.pop_back();
                }
                break;
            }
        }
    }
    while (!stack.empty()) {
        output.push_back(stack.back());
        stack.pop_back();
    }
    InputLine& previous = *inputs.at(lineIndex);
    if (previous.type == InputLineType::ILVariable) {
        variables.erase(previous.identifier);
    } else if (previous.type == InputLineType::ILFunction) {
        variables.erase(previous.identifier);
    }
    previous = line;
    if (line.type == InputLineType::ILVariable) {
        variables[line.identifier] = NaN;
    } else if (line.type == InputLineType::ILFunction) {
        functions[line.identifier] = lineIndex;
    }
}

double Calculator::Evaluate() {
    return 0;
}