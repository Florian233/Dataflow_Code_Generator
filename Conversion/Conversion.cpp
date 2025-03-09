#include "Conversion.hpp"

static std::string read_braket(Token& t, Token_Container& token_producer) {
	std::string ret = "";
	if (t.str == "[") {
		ret.append(t.str);
		t = token_producer.get_next_token();
		while (t.str != "]") {
			if ((t.str == "[") || (t.str == "(")) {
				ret.append(read_braket(t, token_producer));
			}
			else if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File" };
			}
			else {
				ret.append(t.str);
				t = token_producer.get_next_token();
			}
		}
		ret.append(t.str);
		t = token_producer.get_next_token();
	}
	else if (t.str == "(") {
		ret.append(t.str);
		t = token_producer.get_next_token();
		while (t.str != ")") {
			if ((t.str == "[") || (t.str == "(")) {
				ret.append(read_braket(t, token_producer));
			}
			else if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File" };
			}
			else {
				ret.append(t.str);
				t = token_producer.get_next_token();
			}
		}
		ret.append(t.str);
		t = token_producer.get_next_token();
	}
	return ret;
}

static int get_next_literal(
	Token& t,
	Tokenizer& token_producer,
	std::map<std::string, std::string>& symbol_map)
{
	int return_value{ 0 };
	if (t.str == "++") {
		t = token_producer.get_next_token();
		if (symbol_map.find(t.str) != symbol_map.end()) {
			return_value = std::stoi(symbol_map[t.str]);
		}
		else {
			return_value = std::stoi(t.str);
		}
		return_value++;
		t = token_producer.get_next_token();
	}
	else if (t.str == "--") {
		t = token_producer.get_next_token();
		if (symbol_map.find(t.str) != symbol_map.end()) {
			return_value = std::stoi(symbol_map[t.str]);
		}
		else {
			return_value = std::stoi(t.str);
		}
		return_value--;
		t = token_producer.get_next_token();
	}
	else if (symbol_map.find(t.str) != symbol_map.end()) {
		return_value = std::stoi(symbol_map[t.str]);
		t = token_producer.get_next_token();
		if (t.str == "++") {
			return_value++;
			t = token_producer.get_next_token();
		}
		else if (t.str == "--") {
			return_value--;
			t = token_producer.get_next_token();
		}
	}
	else {
		return_value = std::stoi(t.str);
		t = token_producer.get_next_token();
		if (t.str == "++") {
			return_value++;
			t = token_producer.get_next_token();
		}
		else if (t.str == "--") {
			return_value--;
			t = token_producer.get_next_token();
		}
	}
	return return_value;
}

static int evaluate_plus_minus_followup(
	Token& t,
	Tokenizer& token_producer,
	std::map<std::string, std::string>& symbol_map);

static int evaluate_expression(
	Token& t,
	Tokenizer& token_producer,
	std::map<std::string, std::string>& symbol_map)
{
	t = token_producer.get_next_token();
	int return_value{ 0 };
	while (t.str != ")") {
 		if (t.str == "+") {
			return_value +=
				evaluate_plus_minus_followup(t, token_producer, symbol_map);
		}
		else if (t.str == "-") {
			return_value -=
				evaluate_plus_minus_followup(t, token_producer, symbol_map);
		}
		else if (t.str == "*") {
			t = token_producer.get_next_token();
			if (t.str == "(") {
				return_value =
					return_value * evaluate_expression(t, token_producer, symbol_map);
			}
			else {
				return_value =
					return_value * get_next_literal(t, token_producer, symbol_map);
			}
		}
		else if (t.str == "/") {
			t = token_producer.get_next_token();
			if (t.str == "(") {
				return_value =
					return_value / evaluate_expression(t, token_producer, symbol_map);
			}
			else {
				return_value =
					return_value / get_next_literal(t, token_producer, symbol_map);
			}
		}
		else if (t.str == "") {
			throw Wrong_Token_Exception{ "Unexpected End of File." };
		}
		else {//must be a ( or a variable
			if (t.str == "(") {
				return_value = evaluate_expression(t, token_producer, symbol_map);
			}
			else {
				return_value = get_next_literal(t, token_producer, symbol_map);
			}
		}
	}
	return return_value;
}

static int evaluate_plus_minus_followup(
	Token& t,
	Tokenizer& token_producer,
	std::map<std::string, std::string>& symbol_map)
{
	int return_value{ 0 };
	if ((t.str == "+") || (t.str == "-")) {
		t = token_producer.get_next_token();
		while ((t.str != "+") && (t.str != "-") && (t.str != ")")) {
			if (t.str == "*") {
				t = token_producer.get_next_token();
				if (t.str == "(") {
					return_value =
						return_value * evaluate_expression(t, token_producer, symbol_map);
				}
				else {
					return_value =
						return_value * get_next_literal(t, token_producer, symbol_map);
				}
			}
			else if (t.str == "/") {
				t = token_producer.get_next_token();
				if (t.str == "(") {
					return_value =
						return_value / evaluate_expression(t, token_producer, symbol_map);
				}
				else {
					return_value =
						return_value / get_next_literal(t, token_producer, symbol_map);
				}
			}
			else if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else {//must a ( or a variable
				if (t.str == "(") {
					return_value =
						evaluate_expression(t, token_producer, symbol_map);
				}
				else {
					return_value =
						get_next_literal(t, token_producer, symbol_map);
				}
			}
		}//end while
	}//end if
	return return_value;
}

std::string Conversion_Helper::read_type(
	Token& t,
	Token_Container& token_producer,
	std::map<std::string, std::string>& symbol_map)
{
	std::string ret;

	if (!((t.str != "int") || (t.str != "uint") || (t.str != "bool") || (t.str != "String")
		|| (t.str != "float") || (t.str != "half")))
	{
		throw Wrong_Token_Exception{ "Expected a type specifier, but found:" + t.str };
	}
	ret = t.str;
	t = token_producer.get_next_token();
	if (t.str == "(") {
		ret.append(read_braket(t, token_producer));
	}
	return ret;
}

int Conversion_Helper::evaluate_constant_expression(
	std::string expression,
	std::map<std::string, std::string>& symbol_map)
{
	Tokenizer token_producer{ "(" + expression + ")"};
	Token t = token_producer.get_next_token();
	return evaluate_expression(t, token_producer, symbol_map);
}

void Conversion_Helper::read_constants(
	Token_Container& token_producer,
	std::map<std::string, std::string>& symbol_map,
	std::string symbol)
{
	Token t = token_producer.get_next_token();
	t = token_producer.get_next_token();
	if (t.str == "(") {
		/* read braket and ignore output */
		read_braket(t, token_producer);
	}
	if ((t.str != symbol) && (symbol != "*")) {
		return;
	}
	symbol = t.str;
	while ((t.str != "=") && (t.str != ";") && (t.str != "")) {
		t = token_producer.get_next_token();
	}
	if ((t.str == ";") || (t.str == "")) {
		symbol_map[symbol] = "";
		return;
	}
	t = token_producer.get_next_token();
	std::string val = "";
	while (t.str != ";") {
		val.append(t.str);
		t = token_producer.get_next_token();
		if (t.str == "") {
			throw Wrong_Token_Exception{ "Unexpected end of token stream." };
		}
	}
	symbol_map[symbol] = val;
}

bool Conversion_Helper::symbol_name_match(
	Token_Container& token_producer,
	std::string symbol)
{
	if (symbol == "*") {
		return true;
	}
	Token t = token_producer.get_next_token();
	if ((t.str == "function") || (t.str == "procedure")) {
		t = token_producer.get_next_token();
		return (t.str == symbol);
	}
	else if (t.str == "@native") {
		t = token_producer.get_next_token();
		t = token_producer.get_next_token();
		return (t.str == symbol);
	}
	else {
		/* must be variable */
		if (t.str == "(") {
			/* must be list */
			read_braket(t, token_producer); //ignore output
			return (t.str == symbol);
		}
	}

	return true;
}