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
    OFunction,
    ParenthesesL,
    ParenthesesR,
    OOther
};

typedef std::vector<double> args;

struct CalcOperator {
    std::function<double(args)> function;
    int precedence;
    int argumentCount;
    OpType type;
};

static std::pair<std::string, CalcOperator> basicPostfix(std::string name, std::function<double(args)> function) {
    return { name, CalcOperator{function, 1, 1, OpType::Postfix} };
}

static std::pair<std::string, CalcOperator> basicFunction(std::string name, std::function<double(args)> function) {
    return { name, CalcOperator{function, 0, 1, OpType::OFunction} };
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

void Calculator::AddLine(int index) {
    if (index <= evaluateLine) {
        evaluateLine++;
    }
    inputs.insert(inputs.begin() + index, new InputLine{});
}

void Calculator::SetEvaluateLine(int line) {
    evaluateLine = line;
}

void Calculator::RemoveLine(int index) {
    if (index == evaluateLine) {
        evaluateLine = Last;
    }
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

// postfix operators get treated differently anyways
bool isOperator(OpType& type) {
    return type == OpType::OFunction || type == OpType::Operator;
}

bool isOperand(ItemType& type) {
    return type == ItemType::Variable || type == ItemType::Operand || type == ItemType::OperandSymbol;
}

bool isFunction(ItemType& type) {
    return type == ItemType::Function || type == ItemType::UserFunction;
}

std::string Calculator::GetFormattedLine(int index) { 
    InputLine line = *inputs.at(index);
    if (line.type == InputLineType::Expression) {
        return std::to_string(EvaluatePostfix(line.postfix));
    }
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
    std::set<std::string> temp{};
    for (PostfixItem item : GetExpandedPostfix(line.postfix, temp)) {
        switch (item.type) {
        case ItemType::Function:
        case ItemType::Variable:
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
    // update references of old line
    InputLine& previous = *inputs.at(lineIndex);
    previous.failed = true;
    if (previous.type == InputLineType::ILVariable) {
        variables.erase(previous.identifier);
    } else if (previous.type == InputLineType::ILFunction) {
        functions.erase(previous.identifier);
    }
    // parse left hand of = sign
    InputLine line{};
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
    // update references even if the left hand side might not be valid
    if (variables.find(line.identifier) != variables.end() || functions.find(line.identifier) != functions.end()) {
        plError(line.identifier + " cannot be defined twice");
    }
    if (line.type == InputLineType::ILVariable) {
        variables[line.identifier] = lineIndex;
    } else if (line.type == InputLineType::ILFunction) {
        functions[line.identifier] = lineIndex;
    }
    // store the incompletion state
    previous.source = str;
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
        if (iswspace(it) || it == ',') {
            continue;
        }
        PostfixItem item{};
        item.type = ItemType::Other;
        CalcOperator op{};
        op.type = OpType::OOther;
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
            item.name = '(';
            op.type = OpType::ParenthesesL;
            parenLCount++;
        } else if (it == ')') {
            item.name = ')';
            op.type = OpType::ParenthesesR;
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
                item.type = ItemType::Function;
                item.name = matchedOperator;
            } else if (matchedOperand != "") {
                item.type = ItemType::OperandSymbol;
                item.value = operands.at(matchedOperand);
            } else if (matchedVariable != "") {
                item.type = ItemType::Variable;
                item.name = matchedVariable;
            } else if (matchedUserFunction != "") {
                op.type = OpType::OFunction;
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
        if ((items[i].type == ItemType::OperandSymbol ||
            items[i].type == ItemType::Variable ||
            ops[i].type == OpType::ParenthesesL ||
            ops[i].type == OpType::OFunction) &&
            (isOperand(items[i-1].type) ||
            ops[i-1].type == OpType::ParenthesesR)) {

            items.insert(items.begin() + i, PostfixItem{ItemType::Function, "*", 0});
            ops.insert(ops.begin() + i, operators.at("*"));
            ++i;
        }
    }
    for (size_t i{ 0 }; i < items.size(); ++i) {
        PostfixItem item = items[i];
        CalcOperator op = ops[i];
        // finally use shunting yard procedure
        if (isOperand(item.type)) {
            output.push_back(item);
        } else if (isFunction(item.type)) {
            if (op.type == OpType::Postfix) {
                output.push_back(item);
            } else if (isOperator(op.type)) {
                while (!stack.empty() && 
                    ((stack.back().type == ItemType::Function && operators.at(stack.back().name).precedence >= op.precedence) 
                    || stack.back().type == ItemType::UserFunction)) {
                    output.push_back(stack.back());
                    stack.pop_back();
                }
                stack.push_back(item);
            }
        } else {
            if (op.type == OpType::ParenthesesL) {
                stack.push_back(item);
            } else if (op.type == OpType::ParenthesesR) {
                while (!stack.empty() && stack.back().name[0] != '(') {
                    output.push_back(stack.back());
                    stack.pop_back();
                }
                if (!stack.empty()) {
                    stack.pop_back();
                }
            }
        }

    }
    while (!stack.empty()) {
        output.push_back(stack.back());
        stack.pop_back();
    }
    previous = line;
    previous.failed = false;
    previous.source = "";
}

std::deque<PostfixItem> Calculator::GetExpandedPostfix(std::deque<PostfixItem> items, std::set<std::string>& processed) {
    size_t i{ 0 };
    while (i < items.size()) { // replace all the user functions with their expanded forms
        auto item = items[i];
        if (item.type == ItemType::UserFunction) {
            if (functions.find(item.name) == functions.end()) {
                plError(item.name + " isn't well defined");
            }
            auto& function = *inputs[functions[item.name]];
            auto pCount = function.arguments.size();
            if (i < pCount) {
                plError("Missing arguments");
            }
            if (processed.find(function.identifier) != processed.end()) {
                plError("Recursion detected");
            }
            // attempt to reparse the function if it's previously failed, like if you define a variable after a function uses it
            if (function.failed) {
                ParseLine(function.source, functions[item.name]);
            }
            auto processedCopy = processed; // necessary for the recursion detecting to work properly, thinking of the "call stack" as a tree, each vertice should only have a list of its parents
            processedCopy.insert(function.identifier);
            auto subItems = GetExpandedPostfix(function.postfix, processedCopy);
            // replace parameters
            int j{ 0 };
            while (items[i + j - pCount].type != ItemType::UserFunction) {
                for (PostfixItem& item : subItems) {
                    if (item.name == function.arguments[j]) {
                        item = items[i + j - pCount];
                    }
                }
                ++j;
            }
            // wow this looks disgusting
            items.insert(items.begin() + i + 1, subItems.begin(), subItems.end());
            items.erase(items.begin() + i - pCount, items.begin() + i + 1);
            i += subItems.size() - (pCount + 1);
        }
        ++i;
    }
    return items;
}

std::deque<PostfixItem> Calculator::GetExpandedPostfix(std::deque<PostfixItem> items) {
    std::set<std::string> temp{};
    return GetExpandedPostfix(items, temp);
}

double Calculator::EvaluatePostfix(std::deque<PostfixItem> items, std::set<std::string>& processedIdentifiers, std::map<std::string, double>& calculatedVariables) {
    std::set<std::string> temp{};
    items = GetExpandedPostfix(items, temp);
    for (PostfixItem& item : items) {
        if (item.type == ItemType::Variable) {
            if (variables.find(item.name) == variables.end()) {
                plError(item.name + " isn't well defined");
            }
            if (calculatedVariables.find(item.name) == calculatedVariables.end()) {
                if (processedIdentifiers.find(item.name) != processedIdentifiers.end()) {
                    plError("Recursion detected with variables");
                }
                if (inputs[variables[item.name]]->failed) {
                    ParseLine(inputs[variables[item.name]]->source, variables[item.name]);
                }
                std::set<std::string> copy = processedIdentifiers;
                copy.insert(item.name);
                calculatedVariables[item.name] = EvaluatePostfix(inputs[variables[item.name]]->postfix, copy, calculatedVariables);
            }
            item.type = ItemType::Operand;
            item.value = calculatedVariables.at(item.name);
        }
    }
    size_t i{ 0 };
    while (items.size() > 1 && i < items.size()) {
        switch (items[i].type) {
        case ItemType::OperandSymbol:
            items[i].type = ItemType::Operand;
            break;
        case ItemType::Variable:
        case ItemType::UserFunction:
        case ItemType::Other:
            plError("Invalid symbol");
            break;
        case ItemType::Function:
            size_t argCount = operators.at(items[i].name).argumentCount;
            if (i < argCount) {
                plError("Wrong number of arguments for an operator/function");
            }
            std::vector<double> arguments{};
            for (size_t j{ i - argCount }; j < i; ++j) {
                if (items[j].type != ItemType::Operand) {
                    plError("Wrong number of arguments for an operator/function");
                }
                arguments.push_back(items[j].value);
            }
            double result = operators.at(items[i].name).function(arguments);
            items.insert(items.begin() + i + 1, PostfixItem{ ItemType::Operand, "", result });
            items.erase(items.begin() + i - argCount, items.begin() + i + 1);
            i -= argCount;
            break;
        }
        ++i;
    }
    if (items.size() != 1) {
        plError("Wrong number of arguments for an operator/function");
    }
    return items[0].value;
}

double Calculator::EvaluatePostfix(std::deque<PostfixItem> items) {
    std::set<std::string> temp1{};
    std::map<std::string, double> temp2{};
    return EvaluatePostfix(items, temp1, temp2);
}