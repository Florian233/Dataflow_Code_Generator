#include "Conversion.hpp"
#include "common/include/Exceptions.hpp"

static int evaluate_expr(AST::BaseExpression* e, std::map<std::string, std::string>& symbol_map);

static void flatten_operator(AST::Operator* o,
	std::vector<AST::BaseExpression*>& operands, std::vector<std::string>& ops)
{
	AST::Operator* left_op = dynamic_cast<AST::Operator*>(o->left);
	if (left_op != nullptr) {
		flatten_operator(left_op, operands, ops);
	}
	else {
		operands.push_back(o->left);
	}
	ops.push_back(o->ops);
	operands.push_back(o->right);
}

static int evaluate_expr(AST::BaseExpression* e, std::map<std::string, std::string>& symbol_map)
{
	if (dynamic_cast<AST::Operator*>(e) != nullptr) {
		AST::Operator* o = dynamic_cast<AST::Operator*>(e);

		std::vector<AST::BaseExpression*> operands;
		std::vector<std::string> ops;
		flatten_operator(o, operands, ops);

		std::vector<int> values;
		for (AST::BaseExpression* operand : operands) {
			values.push_back(evaluate_expr(operand, symbol_map));
		}

		std::vector<int> add_values{ values[0] };
		std::vector<std::string> add_ops;
		for (std::size_t i = 0; i < ops.size(); ++i) {
			if (ops[i] == "*") {
				add_values.back() = add_values.back() * values[i + 1];
			}
			else if (ops[i] == "/") {
				if (values[i + 1] == 0) {
					throw Converter_Exception{ "Division by zero in constant expression." };
				}
				add_values.back() = add_values.back() / values[i + 1];
			}
			else if (ops[i] == "%") {
				if (values[i + 1] == 0) {
					throw Converter_Exception{ "Modulo by zero in constant expression." };
				}
				add_values.back() = add_values.back() % values[i + 1];
			}
			else if (ops[i] == "+" || ops[i] == "-") {
				add_ops.push_back(ops[i]);
				add_values.push_back(values[i + 1]);
			}
			else {
				throw Converter_Exception{ "Unknown Operation in const expr: " + ops[i] + "." };
			}
		}

		// Pass 2: additive operators (+ -), left to right.
		int result = add_values[0];
		for (std::size_t j = 0; j < add_ops.size(); ++j) {
			if (add_ops[j] == "+") {
				result = result + add_values[j + 1];
			}
			else {
				result = result - add_values[j + 1];
			}
		}
		return result;
	}
	else if (dynamic_cast<AST::Literal*>(e) != nullptr) {
		const std::string lit = dynamic_cast<AST::Literal*>(e)->literal;
		int tmp;
		try {
			tmp = std::stoi(lit);
		}
		catch (const std::exception&) {
			throw Converter_Exception{ "Literal \"" + lit + "\" is not a valid integer constant." };
		}
		if (dynamic_cast<AST::Literal*>(e)->negation) {
			return -tmp;
		}
		return tmp;
	}
	else if (dynamic_cast<AST::Identifier*>(e) != nullptr) {
		const std::string id = dynamic_cast<AST::Identifier*>(e)->identifier;
		if (!symbol_map.contains(id)) {
			throw Converter_Exception{ "Identifier not in constant list." };
		}
		try {
			return std::stoi(symbol_map[id]);
		}
		catch (const std::exception&) {
			throw Converter_Exception{ "Constant \"" + id + "\" has non-integer value \"" + symbol_map[id] + "\"." };
		}
	}
	else if (dynamic_cast<AST::Expression*>(e) != nullptr) {
		return evaluate_expr(dynamic_cast<AST::Expression*>(e)->child, symbol_map);
	}
	else {
		throw Converter_Exception{ "Unknown AST element in const expr." };
	}
}

int Conversion_Helper::evaluate_constant_expression(
	AST::Expression* expression,
	std::map<std::string, std::string>& symbol_map)
{
	return evaluate_expr(expression, symbol_map);
}

void Conversion_Helper::read_constants(
	AST::VarDefinition *var,
	std::map<std::string, std::string>& symbol_map)
{
	if (!var->constassign || (var->assign == nullptr)) {
		return;
	}
	if (dynamic_cast<AST::ListComprehension*>(var->assign->child) != nullptr) {
		return;
	}
	int value = evaluate_constant_expression(var->assign, symbol_map);
	symbol_map[var->name.name] = std::to_string(value);
}