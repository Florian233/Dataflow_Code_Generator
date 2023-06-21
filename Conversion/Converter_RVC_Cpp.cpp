#include "Converter_RVC_Cpp.hpp"
#include <iostream>
#include <algorithm>
namespace Converter_RVC_Cpp {
	std::string convert_inline_if_with_list_assignment(
		std::string string_to_convert,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::string prefix,
		std::string outer_expression);

	std::string find_unused_name(
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map)
	{
		for (int i = 1;; ++i) {
			std::string output{ "_" + std::to_string(i) };
			if ((global_map.find(output) == global_map.end())
				&& (local_map.find(output) == local_map.end()))
			{
				local_map[output] = "";
				return output;
			}
		}
	}

	std::string convert_string(
		Token& t,
		Token_Container& token_producer)
	{
		std::string output{};
		if (t.str == "\"") {
			output.append("\"");
			t = token_producer.get_next_Token();
			while (t.str != "\"") {
				if (t.str == "\\") {
					t = token_producer.get_next_Token();
					output.append("\\" + t.str);
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {
					output.append(t.str);
				}
				t = token_producer.get_next_Token();
				if (t.str != "\"") {
					output.append(" ");
				}
			}
			output.append("\"");
			//must be string termination
			t = token_producer.get_next_Token();
		}
		return output;
	}

	namespace {
		std::string convert_list_comprehension(
			std::string string_to_convert,
			std::string list_name,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map,
			std::string prefix = "",
			std::string outer_expression = "",
			bool nested = false);


		std::string convert_function_call_brakets(
			Token& t,
			Token_Container& token_producer,
			bool println = false)
		{
			std::string output{};
			if (println) {
				output.append(" << ");
			}
			else {
				output.append("(");
			}
			t = token_producer.get_next_Token();
			bool something_to_print{ false };
			while (t.str != ")") {
				if (t.str == "(") {
					output.append(convert_function_call_brakets(t, token_producer));
				}
				else if (t.str == "\"") {
					std::string tmp{ convert_string(t, token_producer) };
					output.append(tmp);
					something_to_print = true;
				}
				else if (t.str == "+" && println) {
					output.append(" << ");
					t = token_producer.get_next_Token();
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {
					output.append(t.str);
					something_to_print = true;
					t = token_producer.get_next_Token();
				}
			}
			if (!println) {
				output.append(")");
			}
			t = token_producer.get_next_Token();
			if (println && !something_to_print) {
				//if the brackets are empty there is nothing to print,
				//so a empty string will be returned to avoid << << without anything in between
				return "";
			}
			return output;
		}
	}

	namespace {
		
		std::string get_full_list(
			Token& t,
			Token_Container& token_prod)
		{
			std::string output{ "{" };
			t = token_prod.get_next_Token();
			while ((t.str != "]") && (t.str != "}")) {
				if ((t.str == "[") || (t.str == "{")) {
					output.append(get_full_list(t, token_prod));
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {
					output.append(t.str);
					t = token_prod.get_next_Token();
				}
			}
			output.append("}");
			t = token_prod.get_next_Token();
			return output;
		}

		//only applicable for the size evaluation!!!
		//Don't use in other methods, because ++ and -- might be wrong there
		int get_next_literal(
			Token &t,
			Token_Container& token_producer,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map)
		{
			int return_value{ 0 };
			if (t.str == "++") {
				t = token_producer.get_next_Token();
				if (local_map.find(t.str) != local_map.end()) {
					return_value = std::stoi(local_map[t.str]);
				}
				else if (global_map.find(t.str) != global_map.end()) {
					return_value = std::stoi(global_map[t.str]);
				}
				else {
					return_value = std::stoi(t.str);
				}
				return_value++;
				t = token_producer.get_next_Token();
			}
			else if (t.str == "--") {
				t = token_producer.get_next_Token();
				if (local_map.find(t.str) != local_map.end()) {
					return_value = std::stoi(local_map[t.str]);
				}
				else if (global_map.find(t.str) != global_map.end()) {
					return_value = std::stoi(global_map[t.str]);
				}
				else {
					return_value = std::stoi(t.str);
				}
				return_value--;
				t = token_producer.get_next_Token();
			}
			else if (local_map.find(t.str) != local_map.end()) {
				return_value = std::stoi(local_map[t.str]);
				t = token_producer.get_next_Token();
				if (t.str == "++") {
					return_value++;
					t = token_producer.get_next_Token();
				}
				else if (t.str == "--") {
					return_value--;
					t = token_producer.get_next_Token();
				}
			}
			else if (global_map.find(t.str) != global_map.end()) {
				return_value = std::stoi(global_map[t.str]);
				t = token_producer.get_next_Token();
				if (t.str == "++") {
					return_value++;
					t = token_producer.get_next_Token();
				}
				else if (t.str == "--") {
					return_value--;
					t = token_producer.get_next_Token();
				}
			}
			else {
				return_value = std::stoi(t.str);
				t = token_producer.get_next_Token();
				if (t.str == "++") {
					return_value++;
					t = token_producer.get_next_Token();
				}
				else if (t.str == "--") {
					return_value--;
					t = token_producer.get_next_Token();
				}
			}
			return return_value;
		}

		
		int evaluate_size(
			Token &t,
			Token_Container& token_producer,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map,
			bool rekursive = false);

		int evaluate_plus_minus_followup(
			Token& t,
			Token_Container& token_producer,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map)
		{
			int return_value{ 0 };
			if ((t.str == "+") || (t.str == "-")) {
				t = token_producer.get_next_Token();
				while ((t.str != "+") && (t.str != "-") && (t.str != ")")) {
					if (t.str == "*") {
						t = token_producer.get_next_Token();
						if (t.str == "(") {
							return_value =
								return_value * evaluate_size(t, token_producer, global_map, local_map, true);
						}
						else {
							return_value =
								return_value * get_next_literal(t, token_producer, global_map, local_map);
						}
					}
					else if (t.str == "/") {
						t = token_producer.get_next_Token();
						if (t.str == "(") {
							return_value =
								return_value / evaluate_size(t, token_producer, global_map, local_map, true);
						}
						else {
							return_value =
								return_value / get_next_literal(t, token_producer, global_map, local_map);
						}
					}
					else if (t.str == "") {
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					else {//must a ( or a variable
						if (t.str == "(") {
							return_value =
								evaluate_size(t, token_producer, global_map, local_map, true);
						}
						else {
							return_value =
								get_next_literal(t, token_producer, global_map, local_map);
						}
					}
				}//end while
			}//end if
			return return_value;
		}
	}
		int evaluate_constant_expression(
			std::string expression,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map)
		{
			Tokenizer token_producer{ "(" + expression + ")"}; //place brackets around the expression to reuse evaluate_size
			Token t = token_producer.get_next_Token();
			return evaluate_size(t, token_producer, global_map, local_map, true);
		}
	
	namespace{
		int evaluate_size(
			Token &t,
			Token_Container& token_producer,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map,
			bool rekursive)
		{
			Token iads = t;
			if (!rekursive) {
				if (t.str != "(") {
					return 32;
				}
				t = token_producer.get_next_Token();//size
				t = token_producer.get_next_Token();//=
			}
			t = token_producer.get_next_Token();
			int return_value{ 0 };
			while (t.str != ")") {
				if (t.str == "+") {
					return_value +=
						evaluate_plus_minus_followup(t, token_producer, global_map, local_map);
				}
				else if (t.str == "-") {
					return_value -=
						evaluate_plus_minus_followup(t, token_producer, global_map, local_map);
				}
				else if (t.str == "*") {
					t = token_producer.get_next_Token();
					if (t.str == "(") {
						return_value =
							return_value * evaluate_size(t, token_producer, global_map, local_map, true);
					}
					else {
						return_value =
							return_value * get_next_literal(t, token_producer, global_map, local_map);
					}
				}
				else if (t.str == "/") {
					t = token_producer.get_next_Token();
					if (t.str == "(") {
						return_value =
							return_value / evaluate_size(t, token_producer, global_map, local_map, true);
					}
					else {
						return_value =
							return_value / get_next_literal(t, token_producer, global_map, local_map);
					}
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {//must be a ( or a variable
					if (t.str == "(") {
						return_value = evaluate_size(t, token_producer, global_map, local_map, true);
					}
					else {
						return_value = get_next_literal(t, token_producer, global_map, local_map);
					}
				}
			}
			t = token_producer.get_next_Token();
			return return_value;
		}
	
		std::pair <std::string, std::string> convert_for_head(
			Token& t,
			Token_Container& token_prod,
			std::map<std::string, std::string>& local_map,
			std::string prefix = "")
		{
			std::string head{};
			std::string tail{};
			std::string adjusted_prefix{ prefix };
			while ((t.str != "do") && (t.str != "}") && (t.str != ":")) { // } due to list comprehension 
				if ((t.str == "for") || (t.str == "foreach")) {
					head.append(adjusted_prefix + "for (");
					t = token_prod.get_next_Token();
					if ((t.str == "uint") || (t.str == "int") || (t.str == "String")
						|| (t.str == "bool") || (t.str == "half") || (t.str == "float"))
					{
						head.append(convert_type(t, token_prod, local_map));
						head.append(" " + t.str + " = ");
						std::string var_name{ t.str };
						local_map[var_name] = "";
						t = token_prod.get_next_Token(); //in
						t = token_prod.get_next_Token(); //start value
						while (t.str != "..") {
							if (t.str == "") {
								throw Wrong_Token_Exception{ "Unexpected End of File." };
							}
							head.append(t.str);
							t = token_prod.get_next_Token();
						}
						head.append(";");
						t = token_prod.get_next_Token(); //skip .. 
						head.append(var_name + " <= ");
						while (t.str != "do" && t.str != "," && t.str != ":" && t.str != "}") {
							if (t.str == "") {
								throw Wrong_Token_Exception{ "Unexpected End of File." };
							}
							head.append(t.str);
							t = token_prod.get_next_Token();
						}
						head.append("; ++" + var_name + ") {\n");
					}
					else {//no type, directly variable name, indicates foreach loop
						head.append("auto " + t.str);
						local_map[t.str] = "";
						t = token_prod.get_next_Token();//in
						head.append(":");
						t = token_prod.get_next_Token();
						if ((t.str == "[") || (t.str == "{")) {
							head.append(get_full_list(t, token_prod));
						}
						head.append("){\n");
					}
					tail.insert(0, adjusted_prefix + "}\n");
					if (t.str == ",") {
						//a komma indicates a further loop head, thus the next token has to be inspected
						t = token_prod.get_next_Token();
						adjusted_prefix.append("\t");
					}
				}
			}
			return std::make_pair(head, tail);
		}
	}

	namespace {
		/**
		Converts the list declaration
		*/
		std::string convert_sub_list(
			Token& t,
			Token_Container& token_producer,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map,
			std::string symbol,
			std::string& type)
		{
			std::string output{};
			if (t.str == "List") {
				t = token_producer.get_next_Token();//(
				t = token_producer.get_next_Token(); //type
				t = token_producer.get_next_Token(); //:
				t = token_producer.get_next_Token(); // type argument
				if (t.str == "List") {
					output = convert_sub_list(t, token_producer, global_map, local_map, symbol, type);
					t = token_producer.get_next_Token();//size
					t = token_producer.get_next_Token();//=
					t = token_producer.get_next_Token(); //start of size value
					size_t insert_point = output.find_first_of("[");
					std::string insert_string{ "[" };
					while(t.str != ")"){
						if (t.str == "(") {
							insert_string.append(convert_function_call_brakets(t, token_producer));
						}
						else if ((global_map.find(t.str) != global_map.end())
							&& (global_map[t.str] != "") && (global_map[t.str] != "function"))
						{
							insert_string.append(global_map[t.str]);
							t = token_producer.get_next_Token();
						}
						else if ((local_map.find(t.str) != local_map.end())
							&& (local_map[t.str] != "") && (local_map[t.str] != "function"))
						{
							insert_string.append(local_map[t.str]);
							t = token_producer.get_next_Token();
						}
						else if (t.str == "") {
							throw Wrong_Token_Exception{ "Unexpected End of File." };
						}
						else {
							insert_string.append(t.str);
							t = token_producer.get_next_Token();
						}
					}
					insert_string.append("]");
					output.insert(insert_point, insert_string);
					t = token_producer.get_next_Token();
				}
				else {
					output = convert_type(t, token_producer, global_map, local_map) + " ";//after the function the token should contain a komma
					type.append(output);
					t = token_producer.get_next_Token(); //size
					t = token_producer.get_next_Token(); //=
					t = token_producer.get_next_Token(); //start of size value
					output.append("[" );
					while (t.str != ")") {
						if (t.str == "(") {
							output.append(convert_function_call_brakets(t, token_producer));
						}
						else if ((global_map.find(t.str) != global_map.end())
							&& (global_map[t.str] != "") && (global_map[t.str] != "function"))
						{
							output.append(global_map[t.str]);
							t = token_producer.get_next_Token();
						}
						else if ((local_map.find(t.str) != local_map.end())
							&& (local_map[t.str] != "") && (local_map[t.str] != "function"))
						{
							output.append(local_map[t.str]);
							t = token_producer.get_next_Token();
						}
						else if (t.str == "") {
							throw Wrong_Token_Exception{ "Unexpected End of File." };
						}
						else {
							output.append(t.str);
							t = token_producer.get_next_Token();
						}
					}
					output.append("]");
					t = token_producer.get_next_Token();
				}
			}
			return output;
		}
	}

	std::string convert_inline_if_with_list_assignment(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::string prefix,
		std::string outer_expression)
	{
		std::string condition;
		std::string expression1;
		std::string expression2;
		std::string output;

		while (t.str != "?") {
			if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			condition.append(t.str + " ");
			t = token_producer.get_next_Token();
		}
		t = token_producer.get_next_Token(); // skip "?"
		int count{ 1 };
		bool nested{ false };
		while (count != 0) {
			if (t.str == "?") {
				++count;
				nested = true;
				expression1.append(t.str + " ");
				t = token_producer.get_next_Token();
			}
			else if (t.str == ":") {
				--count;
				expression1.append(t.str + " ");
				t = token_producer.get_next_Token();
			}
			else if (t.str == "[") {
				expression1.append(convert_brackets(t, token_producer, true, global_map, local_map).first);
			}
			else if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else {
				expression1.append(t.str + " ");
				t = token_producer.get_next_Token();
			}
		}
		expression1.erase(expression1.size() - 2, 2);//remove last :
		if (nested) {
			expression1 =
				convert_inline_if_with_list_assignment(expression1, global_map, local_map,
														prefix + "\t", outer_expression);
		}
		else {
			//auf listenzuweiseung prüfen, wenn ja mit convert_list_comprehension in C++ Code umwandeln
			if ((expression1[0] == '[') || (expression1[0] == '{')) {
				expression1 =
					convert_list_comprehension(expression1, outer_expression, global_map,
												local_map, prefix + "\t");
			}
			else {
				expression1 = prefix + "\t" + outer_expression + " = " + expression1 + ";\n";
			}
		}
		//-----------------
		count= 1;
		nested=false;
		while (count != 0) {
			if (t.str == "?") {
				++count;
				nested = true;
				expression2.append(t.str + " ");
				t = token_producer.get_next_Token();
			}
			else if (t.str == ":") {
				--count;
				expression2.append(t.str + " ");
				t = token_producer.get_next_Token();
			}
			else if (t.str == "") {
				break;
			}
			else if (t.str == "[") {
				expression2.append(convert_brackets(t, token_producer, true, global_map, local_map).first);
			}
			else if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else {
				expression2.append(t.str + " ");
				t = token_producer.get_next_Token();
			}
		}
		if (nested) {
			expression2 =
				convert_inline_if_with_list_assignment(expression2, global_map, local_map,
														prefix + "\t", outer_expression);
		}
		else {
			//auf listenzuweiseung prüfen, wenn ja mit convert_list_comprehension in C++ Code umwandeln
			if ((expression2[0] == '[') || (expression2[0] == '{')) {
				expression2 =
					convert_list_comprehension(expression2, outer_expression, global_map,
												local_map, prefix + "\t");
			}
			else {
				expression2 = prefix + "\t" + outer_expression + " = " + expression2+";\n";
			}
		}

		//Build expression
		output.append(prefix + "if (" + condition + ") {\n");
		output.append(expression1);
		output.append(prefix + "}\n");
		output.append(prefix + "else {\n");
		output.append(expression2);
		output.append(prefix + "}\n");
		return output;
	}

	std::string convert_inline_if_with_list_assignment(
		std::string string_to_convert,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::string prefix,
		std::string outer_expression)
	{
		Tokenizer tok{ string_to_convert };
		Token t = tok.get_next_Token();
		return convert_inline_if_with_list_assignment(t, tok, global_map, local_map, prefix, outer_expression);
	}


		std::pair<std::string,bool> convert_inline_if(
			Token& t,
			Token_Container& token_producer)
		{
			std::string output{};
			std::string previous_token_string;
			bool convert_to_if{ false };
			bool condition_done{ false };
			if (t.str == "if") {
				t = token_producer.get_next_Token();
				while ((t.str != "end") && (t.str != "endif")) {
					if (t.str == "then") {
						condition_done = true;
						output.append("?");
						t = token_producer.get_next_Token();
						previous_token_string = "?";
					}
					else if (t.str == "else") {
						output.append(":");
						t = token_producer.get_next_Token();
						previous_token_string = ":";
					}
					else if (t.str == "if") {
						auto tmp = convert_inline_if(t, token_producer);
						output.append(tmp.first);
						if (tmp.second) {
							convert_to_if = true;
						}
					}
					else if (t.str == "") {
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					else {
						if ((t.str == "[") || (t.str == "{")) {
							// ? oder : davor bedeuten, dass es kein array sein kann und
							// somit ist das eine listenzuweisung, was in C++ nicht funktioniert!
							if ((previous_token_string == "?") || (previous_token_string == ":")) {
								convert_to_if = true;
							}
						}

						if (t.str == "=") {
							output.append(" == ");
						}
						else {
							output.append(" " + t.str);
						}
						previous_token_string = t.str;
						t = token_producer.get_next_Token();
					}
				}
				t = token_producer.get_next_Token();
			}
			return std::make_pair(output,convert_to_if);
		}

	namespace {
		std::string read_brace(
			Token& t,
			Token_Container& token_producer,
			bool convert_if = true)
		{
			std::string output;
			if (t.str == "{") {
				output.append(t.str);
				t = token_producer.get_next_Token();
				while (t.str != "}") {
					if (t.str == "{") {
						output.append(read_brace(t, token_producer));
					}
					else if (convert_if && t.str == "if") {
						output.append(convert_inline_if(t, token_producer).first);
					}
					else if (t.str == "") {
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					else {
						output.append(t.str + " ");
						t = token_producer.get_next_Token();
					}
				}
				output.append(t.str);
				t = token_producer.get_next_Token();
			}
			return output;
		}



		std::string convert_list_comprehension(
			Token& t,
			Token_Container& token_producer,
			std::string list_name,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map,
			std::string prefix = "",
			std::string outer_expression = "",
			bool nested = false)
		{
			//find whitespaces and remove them, because it doesn't look nice
			while (list_name.find(" ") != std::string::npos) {
				list_name = list_name.erase(list_name.find(" "), 1);
			}
			t = token_producer.get_next_Token(); //drop {
			std::string index_name{ find_unused_name(global_map, local_map) };
			std::string output{ prefix + "int " + index_name + "{0};\n" };
			std::string command{ };
			bool had_inner_list_comprehension{ false };
			while (t.str != "}" ) {
				if (t.str == "{" ) {//nested list comprehension
					std::string buffer{ read_brace(t,token_producer,false) };
					Tokenizer tok{ buffer };
					Token token_tok = tok.get_next_Token();
					std::string expr;
					if (nested) {
						expr = outer_expression + "[" + index_name + "]";
					}
					else {
						expr = list_name + "[" + index_name + "]";
					}
					command.append(convert_list_comprehension(token_tok, tok, list_name,
																global_map, local_map, prefix+"\t",
																expr, true));
					had_inner_list_comprehension = true;
					//t = token_producer.get_next_Token(); // skip closing }
				}
				else {
					//must be the listcomprehension
					if (!had_inner_list_comprehension) {
						if (outer_expression == "") {
							command.append(prefix + "\t" + list_name + "[" + index_name + "]");
						}
						else {
							command.append(prefix + "\t" + outer_expression + "[" + index_name + "]");
						}
					}
					std::string tmp_command{};
					while ((t.str != ":") && (t.str != "}")) {
						if (t.str == "{") {
							std::string tmp{ read_brace(t,token_producer) };
							std::replace(tmp.begin(), tmp.end(), '{', '[');
							std::replace(tmp.begin(), tmp.end(), '}', ']');
							tmp_command.append(tmp);
						}
						else if (t.str == "if") {
							auto buf = convert_inline_if(t, token_producer);
							if (buf.second) {
								//remove prefix because the corresponding prefix will be added later
								std::string tmp_str{ command };
								while (tmp_str.find("\t") != std::string::npos) {
									tmp_str.erase(tmp_str.find("\t"),1);
								}
								command =
									convert_inline_if_with_list_assignment(buf.first, global_map,
																			local_map, prefix + "\t",
																			tmp_str);
								tmp_command = "";
							}
							else {
								tmp_command.append(buf.first);
							}
						}
						else if (t.str == "") {
							throw Wrong_Token_Exception{ "Unexpected End of File." };
						}
						else {
							tmp_command.append(t.str);
							t = token_producer.get_next_Token();
						}
					}
					if (t.str == ":") {
						t = token_producer.get_next_Token();
						std::pair<std::string, std::string> head_tail =
								convert_for_head(t, token_producer, local_map, prefix);
						output.append(head_tail.first);
						//insert the corresponding number of \t before the command, to get the spacing right
						std::string tabs;
						int for_count{ -1 };
						size_t pos = head_tail.first.find("for");
						while (pos != std::string::npos) {
							++for_count;
							pos = head_tail.first.find("for", pos + 1);
						}
						while (for_count > 0) {
							tabs.append("\t");
							--for_count;
						}
						output.append(tabs);
						if (tmp_command.empty()) {
							output.append(command);
						}
						else {
							output.append(command + " = " + tmp_command + ";\n");
						}
						output.append(prefix +tabs+ "\t++" + index_name + ";\n");
						output.append(head_tail.second);
					}
					else if (t.str == "}") {
						tmp_command.insert(0, "{");
						tmp_command.append("}");
						std::string unused_var{ find_unused_name(global_map,local_map) };
						output.append(prefix + "for( auto " + unused_var + ":" + tmp_command + ") {\n");
						output.append(command+ " = " + unused_var + ";\n");
						output.append(prefix + "\t++" + index_name + ";\n");
						output.append(prefix + "}\n");
					}
				}
				if (t.str == ",") {
					t = token_producer.get_next_Token();
					output.append(prefix + "++" + index_name + ";\n");
					had_inner_list_comprehension = false;
				}

			}
			t = token_producer.get_next_Token();
			return output;
		}

		std::string convert_list_comprehension(
			std::string string_to_convert,
			std::string list_name,
			std::map<std::string, std::string>& global_map,
			std::map<std::string, std::string>& local_map,
			std::string prefix,
			std::string outer_expression,
			bool nested)
		{
			std::replace(string_to_convert.begin(), string_to_convert.end(), '[', '{');
			std::replace(string_to_convert.begin(), string_to_convert.end(), ']', '}');
			Tokenizer token_producer{ string_to_convert };
			Token t = token_producer.get_next_Token();//this must be the start of the list; { - can be dropped
			return prefix + "{\n" + convert_list_comprehension(t, token_producer, list_name,
																global_map, local_map, prefix + "\t")
				+ prefix + "}\n";
		}

	}

	std::pair<std::string, bool> convert_brackets(
		Token& t,
		Token_Container& token_producer,
		bool is_list, //is_list => rvalue 
		std::map<std::string,std::string>& global_map,
		std::map<std::string,std::string>& local_map,
		std::string prefix)
	{
		bool list_comprehension{ false };
		std::string output{};
		std::string previous_token_string;
		if ((t.str == "[") || (t.str == "{")) {
			if (is_list) {
				output.append("{");
			}
			else {
				output.append("[");
			}
			previous_token_string = t.str;
			t = token_producer.get_next_Token();
			for (;;) {
				if ((t.str == "[") || (t.str == "{")) {
					if ((previous_token_string == "[") || (previous_token_string == ",")) {
						std::pair<std::string, bool> ret_val =
							convert_brackets(t, token_producer, is_list, global_map, local_map, prefix);
						output.append(ret_val.first);
						if (ret_val.second) {
							list_comprehension = true;
						}
					}
					else {
						std::pair<std::string, bool> ret_val =
							convert_brackets(t, token_producer, false, global_map, local_map, prefix);
						output.append(ret_val.first);
						if (ret_val.second) {
							list_comprehension = true;
						}
					}
					previous_token_string = "]";
				}
				else if ((t.str == "]") || (t.str == "}")) {
					if (is_list) {
						output.append(" }");
					}
					else { 
						output.append(" ]"); 
					}
					previous_token_string = t.str;
					t = token_producer.get_next_Token();
					break;
				}
				else if (t.str == ":") {
					//FOUND LIST COMPREHENSION!!!!!!!!!
					previous_token_string = t.str;
					t = token_producer.get_next_Token();
					output.append(":");
					list_comprehension = true;
				}
				else if (t.str == "\"") {
					output.append(convert_string(t, token_producer));
					previous_token_string = "\"";
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {
					if ((global_map.find(t.str) != global_map.end()) && (global_map[t.str] != "")
						&& (global_map[t.str] != "function"))
					{
						output.append(global_map[t.str]);
					}
					else if ((local_map.find(t.str) != local_map.end()) && (local_map[t.str] != "")
						&& (local_map[t.str] != "function"))
					{
						output.append(local_map[t.str]);
					}
					else {
						output.append(" "+t.str);
					}
					previous_token_string = t.str;
					t = token_producer.get_next_Token();
				}
			}
		}
		return std::make_pair(output, list_comprehension);
	}

	std::string convert_function(
		Token& t, Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::string prefix,
		std::string symbol)
	{
		std::map<std::string, std::string> local_map{};
		if (t.str == "function") {
			std::string output{ prefix + "auto " };
			t = token_producer.get_next_Token();//name
			std::string symbol_name{ t.str };
			output.append(t.str);
			global_map[t.str] = "function";
			t = token_producer.get_next_Token();//(
			output.append("(");
			t = token_producer.get_next_Token();
			while (t.str != ")") {
				if ((t.str == "uint") || (t.str == "int") || (t.str == "String")
					|| (t.str == "bool") || (t.str == "half") || (t.str == "float"))
				{
					output.append(convert_type(t, token_producer, global_map, local_map) + " ");
					output.append(t.str); // must be the parameter name
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {
					output.append(t.str);
				}
				t = token_producer.get_next_Token();
			}
			output.append(")");
			t = token_producer.get_next_Token();
			if (t.str == "-->") {
				output.append(" -> ");
			}
			else {//TODO check this! Is this required?
				std::cerr << "Error parsing function";
			}
			t = token_producer.get_next_Token();//must be the return type
			output.append(convert_type(t, token_producer, global_map, local_map) + " {\n");
			t = token_producer.get_next_Token();
			while ((t.str != "end") && (t.str != "endfunction")) {
				if ((t.str == "var") || (t.str == "begin") || (t.str == "do")
					|| (t.str == ":") || (t.str == ","))
				{
					t = token_producer.get_next_Token();
				}
				else if (t.str == "if") {
					output.append(convert_if(t, token_producer, global_map, local_map, true, prefix + "\t"));
				}
				else if ((t.str == "for") || (t.str == "foreach")) {
					output.append(convert_for(t, token_producer, global_map, local_map, true, prefix + "\t"));
				}
				else if (t.str == "while") {
					output.append(convert_while(t, token_producer, global_map, local_map, true, prefix + "\t"));
				}
				else if ((t.str == "list") || (t.str == "List")) {
					output.append(convert_list(t, token_producer, global_map, local_map, "*", prefix + "\t"));
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {
					output.append(convert_expression(t, token_producer, global_map, local_map, "*", true, prefix + "\t"));
				}
			}
			output.append(prefix + "}\n");
			t = token_producer.get_next_Token();
			if ((symbol_name != symbol) && (symbol != "*")) {
				return "";//jump out of this procedure if the symbol is not required by the caller
			}
			return output;
		}
		else {
			throw Wrong_Token_Exception{ "Expected a function declaration but found:" + t.str };
		}
	}

	std::string convert_procedure(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::string prefix,
		std::string symbol)
	{
		std::map<std::string, std::string> local_map{};
		if (t.str == "procedure") {
			std::string output{ prefix + "void " };
			t = token_producer.get_next_Token();//name
			output.append(t.str);
			std::string symbol_name{ t.str };
			global_map[t.str] = "function";
			t = token_producer.get_next_Token();//(
			output.append("(");
			t = token_producer.get_next_Token();
			while (t.str != ")") {
				if ((t.str == "uint") || (t.str == "int") || (t.str == "String")
					|| (t.str == "bool") || (t.str == "half") || (t.str == "float"))
				{
					output.append(convert_type(t, token_producer, global_map, local_map)+ " ");
					output.append(t.str); //must be the parameter name
					local_map[t.str] = "";
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {
					output.append(t.str);
				}
				t = token_producer.get_next_Token();
			}
			output.append(") {\n");
			t = token_producer.get_next_Token();
			while ((t.str != "end") && (t.str != "endprocedure")) {
				if ((t.str == "var") || (t.str == "begin") || (t.str == "do") || (t.str == ":")) {
					t = token_producer.get_next_Token();
				}
				else if (t.str == "if") {
					output.append(convert_if(t, token_producer, global_map, local_map, false, prefix + "\t"));
				}
				else if ((t.str == "for") || (t.str == "foreach")) {
					output.append(convert_for(t, token_producer, global_map, local_map, false, prefix + "\t"));
				}
				else if (t.str == "while") {
					output.append(convert_while(t, token_producer, global_map, local_map, false, prefix + "\t"));
				}
				else if ((t.str == "list") || (t.str == "List")) {
					output.append(convert_list(t, token_producer, global_map, local_map, "*", prefix + "\t"));
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else { 
					output.append(convert_expression(t, token_producer, global_map, local_map, "*", false, prefix + "\t")); 
				}
			}
			output.append(prefix + "}\n");
			t = token_producer.get_next_Token();
			if ((symbol_name != symbol) && (symbol != "*")) {
				return "";//jump out of this procedure if the symbol is not required by the caller
			}
			return output;
		}
		else {
			throw Wrong_Token_Exception{ "Expected a procedure declaration but found:" + t.str };
		}
	}

	std::string convert_if(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		bool return_statement,
		std::string prefix,
		bool nested)
	{
		std::string output{};
		if (t.str == "if") {
			if (nested) { 
				output.append("if("); 
			}
			else {
				output.append(prefix + "if(");
			}
			t = token_producer.get_next_Token(); // skip if
			while (t.str != "then") {
				if (t.str == "=") {
					output.append(" == ");
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {
					output.append(t.str + " ");
				}
				t = token_producer.get_next_Token();
			}
			output.append("){\n");
			t = token_producer.get_next_Token();
			while ((t.str != "end") && (t.str != "endif")) {
				if (t.str == "else") {
					t = token_producer.get_next_Token();
					if (t.str == "if") {
						output.append(prefix + "} else ");
						output.append(convert_if(t, token_producer, global_map, local_map, return_statement, prefix, true));
					}
					else {
						output.append(prefix + "} else {\n");
					}
				}
				else if (t.str == "if") {
					output.append(convert_if(t, token_producer, global_map, local_map, return_statement, prefix + "\t"));
				}
				else if ((t.str == "for") || (t.str == "foreach")) {
					output.append(convert_for(t, token_producer, global_map, local_map, return_statement, prefix + "\t"));
				}
				else if (t.str == "while") {
					output.append(convert_while(t, token_producer, global_map, local_map, return_statement, prefix + "\t"));
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {
					output.append(convert_expression(t, token_producer, global_map, local_map, "*", return_statement, prefix + "\t"));
				}
			}
			if (!nested) {
				output.append(prefix + "}\n");
			}
			t = token_producer.get_next_Token();
		}
		return output;
	}


	std::string convert_for(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		bool return_statement,
		std::string prefix)
	{
		std::string output{};
		if ((t.str == "for") || (t.str == "foreach")) {
			std::pair<std::string, std::string> head_tail = convert_for_head(t, token_producer, local_map);//t should contain do after this function
			output.append(prefix + head_tail.first);
			t = token_producer.get_next_Token();
			while ((t.str != "end") && (t.str != "endfor") && (t.str != "endforeach")) {
				if (t.str == "if") {
					output.append(convert_if(t, token_producer, global_map, local_map, return_statement, prefix + "\t"));
				}
				else if ((t.str == "for") || (t.str == "foreach")) {
					output.append(convert_for(t, token_producer, global_map, local_map, return_statement, prefix + "\t"));
				}
				else if (t.str == "while") {
					output.append(convert_while(t, token_producer, global_map, local_map, return_statement, prefix + "\t"));
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {
					output.append(convert_expression(t, token_producer, global_map, local_map, "*", return_statement, prefix + "\t"));
				}
			}
			output.append(prefix + head_tail.second);
			t = token_producer.get_next_Token();
		}
		return output;
	}

	std::string convert_while(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		bool return_statement,
		std::string prefix)
	{
		std::string output{};
		if (t.str == "while") {
			t = token_producer.get_next_Token();
			output.append(prefix + "while(");
			while ((t.str != "do") && (t.str != ":")) {
				if (t.str == "=") {
					output.append(" == ");
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {
					output.append(t.str);
				}
				t = token_producer.get_next_Token();
			}
			output.append("){\n");
			t = token_producer.get_next_Token();
			while ((t.str != "end") && (t.str != "endwhile")) {
				if (t.str == "if") {
					output.append(convert_if(t, token_producer, global_map, local_map, return_statement, prefix + "\t"));
				}
				else if ((t.str == "for") || (t.str == "foreach")) {
					output.append(convert_for(t, token_producer, global_map, local_map, return_statement, prefix + "\t"));
				}
				else if (t.str == "while") {
					output.append(convert_while(t, token_producer, global_map, local_map, return_statement, prefix + "\t"));
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {
					output.append(convert_expression(t, token_producer, global_map, local_map, "*", return_statement, prefix + "\t"));
				}
			}
			output.append(prefix + "}\n");
			t = token_producer.get_next_Token();
		}
		return output;
	}
	namespace {
		bool is_const(std::string str, std::map<std::string, std::string>& global_map, std::map<std::string, std::string>& local_map) {
			//all math operators are const
			if ((str == "/") || (str == "-") || (str == "+") || (str == "(") || (str == ")")
				|| (str == "*"))
			{
				return true;
			}
			//all digits are const
			bool digit{ true };
			for (auto it = str.begin(); it != str.end(); ++it) {
				if ((*it == '0') || (*it == '1') || (*it == '2') || (*it == '3')
					|| (*it == '4') || (*it == '5') || (*it == '6') || (*it == '7')
					|| (*it == '8') || (*it == '9'))
				{

				}
				else {
					digit = false;
				}
			}
			if (digit) {
				return true;
			}
			if ((global_map.count(str) > 0) && (global_map[str] != "")
				&& (global_map[str] != "function"))
			{
				return true;
			}
			if ((local_map.count(str) > 0) && (local_map[str] != "")
				&& (local_map[str] != "function"))
			{
				return true;
			}
			return false;
		}

	}


	std::string convert_expression(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::string prefix)
	{
		return convert_expression(t, token_producer, global_map, global_map, "*", false, prefix);
	}

	std::string convert_expression(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::string symbol,
		bool return_statement,
		std::string prefix)
	{
		std::string output{};
		bool type_specified{ false };
		bool println{ false };
		bool print{ false };
		if ((t.str == "uint") || (t.str == "int") || (t.str == "String") || (t.str == "bool")
			|| (t.str == "half") || (t.str == "float"))
		{
			output.append(convert_type(t, token_producer, global_map, local_map) + " ");
			type_specified = true;
		}
		else if (t.str == "List") {
			return convert_list(t, token_producer, global_map, local_map, "*", prefix);//parsing for specific symbol isn't startet here, if specific list has to be found the function is started directly via the entry function of this flow
		}
		else if (return_statement) {
			output.append("return ");
		}
		std::string symbol_name{ t.str };
		local_map[symbol_name] = ""; //insert to check for name collisions
		if (t.str == "println") {
			output.append("std::cout ");
			println = true;
		}
		else if (t.str == "print") {
			output.append("std::cout ");
			print = true;
		}
		else {
			output.append(t.str);
		}
		t = token_producer.get_next_Token();
		while (t.str == "[") {
			output.append(convert_brackets(t, token_producer, false, global_map, local_map, prefix).first);
		}
		if (t.str == ":=") {
			output.insert(0, prefix);
			output.append(" = ");
			t = token_producer.get_next_Token();
			if (t.str == "\"") {
				output.append(convert_string(t, token_producer) + ";");
				t = token_producer.get_next_Token();
			}
			else if (t.str == "[") {
				std::pair<std::string, bool> ret_val =
					convert_brackets(t, token_producer, true, global_map, local_map, prefix);
				if (ret_val.second) {
					//remove = sign
					output.erase(output.find_last_of("="));
					if (!type_specified) {// if there is no type, this is not the declaration, thus the part before the equal sign can be skipped
						while (output.find("\t") != std::string::npos) {
							output.erase(output.find("\t"), 1);
						}
						output = convert_list_comprehension(ret_val.first, output, global_map, local_map, prefix);
					}
					else {
						output.append(";\n");
						output.append(convert_list_comprehension(ret_val.first, symbol_name, global_map, local_map, prefix));
					}
				}
				else {
					if (type_specified) {
						output.append(ret_val.first + ";\n");
					}
					else {
						output = convert_list_comprehension(ret_val.first, symbol_name, global_map, local_map, prefix);
					}
				}
				t = token_producer.get_next_Token();
			}
			else if (t.str == "if") {
				auto tmp = convert_inline_if(t, token_producer);
				if (tmp.second) {
					if (type_specified) {
						//remove = because no immediate initialization is possible in c++
						output.erase(output.find(" = "));
						output.append(";\n");
					}
					else {
						output = "";
					}
					//convert the expression to an if statement
					output.append(convert_inline_if_with_list_assignment(tmp.first, global_map, local_map, prefix, symbol_name));
				}
				else {
					output.append(tmp.first);
					output.append(";\n");
				}
			}
			else {
				std::string value{};
				bool only_const_values{ true };
				while ((t.str != ":") && (t.str != ",") && (t.str != ";") && (t.str != "do")
					&& (t.str != "begin"))
				{
					if (t.str == "=") {//here can be no assignment, so it must be ==
						value.append(" == ");
						t = token_producer.get_next_Token();
					}
					else if (t.str == "if") {
						value.append(convert_inline_if(t, token_producer).first);
						only_const_values = false;
					}
					else if (t.str == "") {
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					else {
						value.append(t.str);
						if (!is_const(t.str, global_map, local_map)) {
							only_const_values = false;
							if ((global_map.count(t.str) > 0) && (global_map[t.str] == "function")) {
								t = token_producer.get_next_Token();
								value.append(convert_function_call_brakets(t, token_producer));
							}
							else {
								t = token_producer.get_next_Token();
							}
						}
						else {
							t = token_producer.get_next_Token();
						}
					}

				}
				if (only_const_values) {
					int result = evaluate_constant_expression(value, global_map, local_map);
					output.append(std::to_string(result) + ";\n");
					local_map[symbol_name] = "";// std::to_string(result); //insert into the map to find it, if it is used to calculate the size of a type!!!
				}
				else {
					output.append(value + ";\n");
					local_map[symbol_name] = ""; //insert into the map to symbolize that it is defined but not const!
				}
			}
		}
		else if (t.str == "=") {
			if (type_specified)output.insert(0, prefix + "const ");
			output.append(" = ");
			t = token_producer.get_next_Token();
			if (t.str == "\"") {
				output.append(convert_string(t, token_producer) + ";");
				t = token_producer.get_next_Token();
			}
			else if (t.str == "[") {
				std::pair<std::string, bool> ret_val = convert_brackets(t, token_producer, true, global_map, local_map, prefix);
				if (ret_val.second) {
					//remove const, because a list cannot be filled by a loop if it is const!
					output.erase(output.find("const "), 6);
					//remove = sign
					output.erase(output.find_last_of("="));
					if (!type_specified) {// if there is no type, this is not the declaration, thus the part before the equal sign can be skipped
						while (output.find("\t") != std::string::npos) {
							output.erase(output.find("\t"), 1);
						}
						output = convert_list_comprehension(ret_val.first, output, global_map, local_map, prefix);
					}
					else {
						output.append(";\n");
						output.append(convert_list_comprehension(ret_val.first, symbol_name, global_map, local_map, prefix));
					}
				}
				else {
					if (type_specified) {
						output.append(ret_val.first + ";\n");
					}
					else {
						output =
							convert_list_comprehension(ret_val.first, symbol_name, global_map, local_map, prefix);
					}
				}
				t = token_producer.get_next_Token();
			}
			else if (t.str == "if") {
				auto tmp = convert_inline_if(t, token_producer);
				if (tmp.second) {
					if (type_specified) {
						//remove const and = because no immediate initialization is possible in c++
						output.erase(output.find("const"), 6);
						output.erase(output.find(" = "));
						output.append(";\n");
					}
					else {
						output = "";
					}
					//convert the expression to an if statement
					output.append(convert_inline_if_with_list_assignment(tmp.first, global_map, local_map, prefix, symbol_name));
				}
				else {
					output.append(tmp.first);
					output.append(";\n");
				}
			}
			else {
				std::string value{};
				bool only_const_values{ true };
				while ((t.str != ":") && (t.str != ",") && (t.str != ";") && (t.str != "do")
					&& (t.str != "begin"))
				{
					if (t.str == "=") {//here can be no assignment, so it must be ==
						value.append(" == ");
						t = token_producer.get_next_Token();
					}
					else if (t.str == "if") {
						value.append(convert_inline_if(t, token_producer).first);
						only_const_values = false;
					}
					else if (t.str == "") {
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					else {
						value.append(t.str);
						if (!is_const(t.str, global_map, local_map)) {
							only_const_values = false;
							if ((global_map.count(t.str) > 0) && (global_map[t.str] == "function")) {
								t = token_producer.get_next_Token();
								value.append(convert_function_call_brakets(t, token_producer));
							}
							else {
								t = token_producer.get_next_Token();
							}
						}
						else {
							t = token_producer.get_next_Token();
						}
					}
					
				}
				if (only_const_values) {
					int result = evaluate_constant_expression(value, global_map, local_map);
					output.append(std::to_string(result) + ";\n");
					local_map[symbol_name] = std::to_string(result); //insert into the map to find it, if it is used to calculate the size of a type!!!
				}
				else {
					output.append(value + ";\n");
					local_map[symbol_name] = ""; //insert into the map to symbolize that it is defined but not const!
				}
			}
		}
		else {//find expressions with an assignment
			output.insert(0, prefix);
			bool output_appended{ false };
			while ((t.str != ":") && (t.str != ";") && (t.str != ",") && (t.str != "end")
				&& (t.str != "endfunction") && (t.str != "endprocedure") && (t.str != "endaction")
				&& (t.str != "else") && (t.str != "do") && (t.str != "begin"))
			{
				if (t.str == "(") {
					output.append(convert_function_call_brakets(t, token_producer,println||print));
					if (println) {
						output.append(" << \"\\n\"");
					}
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {
					if (t.str == "=") {// here is no assignment, thus it must be comparsion
						output.append(" == ");
					}
					else {
						output.append(t.str);
					}
					t = token_producer.get_next_Token();
				}
			}
			output.append(";\n");
		}
		if ((t.str == ";") || (t.str == ",")) {
			t = token_producer.get_next_Token(); //must drop this token, because ; and , can end one expression, but if it is the last expression in this block, there is no line termination
		}
		if ((symbol_name != symbol) && (symbol != "*")) {
			return ""; //stop is symbol is not requested - but must be inserted into the map before
		}
		return output;
	}

	std::string convert_list(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::string prefix)
	{
		return convert_list(t, token_producer, global_map, global_map, "*", prefix);
	}

	std::string convert_list(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::string symbol,
		std::string prefix)
	{
		std::string type{};
		std::string s{ convert_sub_list(t, token_producer, global_map, local_map, symbol, type) };
		//t should now contain the name of the list
		s.insert(s.find_first_of("["), t.str);
		std::string name{ t.str };
		local_map[name] = ""; // insert to check for name collisions
		t = token_producer.get_next_Token();
		if (t.str == ":=") {
			s.insert(0, prefix);
			s.append(" = ");
			t = token_producer.get_next_Token();
			std::pair<std::string, bool> ret_val =
				convert_brackets(t, token_producer, true, global_map, local_map, prefix);
			if (ret_val.second == true) {
				//remove = sign
				s.erase(s.find_last_of("=")-1);
				s.append(";");
				s.append(convert_list_comprehension(ret_val.first, name, global_map, local_map, prefix));
			}
			else {
				s.append(ret_val.first + ";\n");
			}
			t = token_producer.get_next_Token();
		}
		else if (t.str == "=") {
			s.insert(0, prefix + "const ");
			s.append(" = ");
			t = token_producer.get_next_Token();
			std::pair<std::string, bool> ret_val =
				convert_brackets(t, token_producer, true, global_map, local_map, prefix);
			if (ret_val.second == true) {
				//remove = sign
				s.erase(s.find("const "), 6);//remove const
				s.erase(s.find_last_of("=")-1);
				s.append(";");
				s.append(convert_list_comprehension(ret_val.first, name, global_map, local_map, prefix));
			}
			else {
				s.append(ret_val.first + ";\n");
			}
			t = token_producer.get_next_Token();
		}
		else {
			s.append(";\n");
			s.insert(0, prefix);
			t = token_producer.get_next_Token();
		}
		if ((name != symbol) && (symbol != "*")) {//no the requestes symbol -> return empty string
			return "";
		}
		return s;
	}


	std::string convert_type(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map)
	{
		return convert_type(t, token_producer, global_map, global_map);
	}

	std::string convert_type(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map)
	{
		if (t.str == "int") {
			t = token_producer.get_next_Token();
			int value = evaluate_size(t, token_producer, global_map, local_map);
			if (value <= 8) {
				return "char";
			}
			else if (value <= 16) {
				return "short";
			}
			else if (value <= 32) {
				return "int";
			}
			else if (value <= 64) {
				return "long";
			}
			else {
				return "int";
			}
		}
		else if (t.str == "uint") {
			t = token_producer.get_next_Token();
			int value = evaluate_size(t, token_producer, global_map, local_map);
			if (value <= 8) {
				return "unsigned char";
			}
			else if (value <= 16) {
				return "unsigned short";
			}
			else if (value <= 32) {
				return "unsigned int";
			}
			else if (value <= 64) {
				return "unsigned long";
			}
			else {
				return "unsigned int";
			}
		}
		else if (t.str == "bool") {
			t = token_producer.get_next_Token();
			if (t.str == "(") {//bool shouldnt consist of a size, but just in case it does the tokens will be dropped
				while (t.str != ")") {
					if (t.str == "") {
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					t = token_producer.get_next_Token();
				}
				t = token_producer.get_next_Token();
			}
			return "bool";
		}
		else if (t.str == "String") {
			t = token_producer.get_next_Token();
			if (t.str == "(") {//there shouldnt be a size specified, but if it is, drop it
				while (t.str != ")") {
					if (t.str == "") {
						throw Wrong_Token_Exception{ "Unexpected End of File." };
					}
					t = token_producer.get_next_Token();
				}
				t = token_producer.get_next_Token();
			}
			return "const char*";
		}
		else if (t.str == "float") {
			t = token_producer.get_next_Token();
			int value = evaluate_size(t, token_producer, global_map, local_map);
			if (value <= 16) {
				return "half";
			}
			else if (value <= 32) {
				return "float";
			}
			else if (value <= 64) {
				return "double";
			}
			else {
				return "float";
			}
		}
		else if (t.str == "half") {
			t = token_producer.get_next_Token();
			int value = evaluate_size(t, token_producer, global_map, local_map);
			if (value <= 16) {
				return "half";
			}
			else if (value <= 32) {
				return "float";
			}
			else if (value <= 64) {
				return "long";
			}
			else {
				return "half";
			}
		}
		else {
			throw Wrong_Token_Exception{ "Expected a type specifier, but found:" + t.str };
		}
	}

	std::string convert_native_declaration(
		Token& t,
		Token_Container& token_producer,
		std::string symbol,
		Actor_Conversion_Data& actor_conversion_data,
		std::map<std::string, std::string>& global_map)
	{
		std::string declaration;
		bool add{ false };
		bool function{ false };

		if (t.str == "@native") {
			t = token_producer.get_next_Token();
			if ((t.str != "function") && (t.str != "procedure")) {
				throw Wrong_Token_Exception{ "Expected function but found " + t.str + "."};
			}
			if (t.str == "function") {
				function = true;
			}
			t = token_producer.get_next_Token(); //name
			if ((t.str == symbol) || (symbol == "*")) {
				global_map[t.str] = "function";
				actor_conversion_data.add_native_function(t.str);
				add = true;
			}
			declaration.append(t.str);
			t = token_producer.get_next_Token();

			if (t.str != ("(")) {
				throw Wrong_Token_Exception{ "Expected \"(\" but found " + t.str +"." };
			}
			declaration.append(t.str);
			t = token_producer.get_next_Token();

			while (t.str != ")") {
				if ((t.str == "uint") || (t.str == "int") || (t.str == "String")
					|| (t.str == "bool") || (t.str == "half") || (t.str == "float"))
				{
					declaration.append(convert_type(t, token_producer, global_map, global_map) + " ");
					declaration.append(t.str); // must be the parameter name
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {
					//This shouldn't happen
					std::cout << "Native function conversion failed at token " << t.str << std::endl;
					exit(5);
				}
				t = token_producer.get_next_Token();
			}
			declaration.append(")");
			t = token_producer.get_next_Token();
			if (function) {
				if (t.str != "-->") {
					throw Wrong_Token_Exception{ " Expected \"-->\" during native function declaration conversion but found \"" + t.str + "\"." };
				}
				t = token_producer.get_next_Token();
				// must be the type
				std::string return_type = convert_type(t, token_producer, global_map, global_map);
				declaration = "extern \"C\" " + return_type + " " + declaration;
			}
			else {
				declaration = "extern \"C\" void " + declaration;
			}
			if (function) {
				if ((t.str != "end") && (t.str != "endfunction")) {
					throw Wrong_Token_Exception{ "Expected function end but found " + t.str };
				}
			}
			else {
				if ((t.str != "end") && (t.str != "endprocedure")) {
					throw Wrong_Token_Exception{ "Expected procedure end but found " + t.str };
				}
			}
			t = token_producer.get_next_Token();
		}
		else {
			throw Wrong_Token_Exception{ "Expected a native declaration but found:" + t.str };
		}


		if (add) {
			return declaration + ";\n";
		}
		else {
			return std::string();
		}
	}

	std::string convert_native_declaration(
		Token& t,
		Token_Container& token_producer,
		std::string symbol,
		Actor_Conversion_Data& actor_conversion_data)
	{
		std::map<std::string, std::string> tmp_map;
		return convert_native_declaration(t, token_producer, symbol, actor_conversion_data, tmp_map);
	}

	std::string convert_scope(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::string prefix = "")
	{
		std::string output{ prefix + "{\n" };
		std::map<std::string, std::string> scope_local_map{ local_map };
		t = token_producer.get_next_Token(); //skip begin
		while (t.str != "end") {
			if ((t.str == "var") || (t.str == "do")) {
				t = token_producer.get_next_Token();
			}
			else if (t.str == "begin") {
				output.append(convert_scope(t, token_producer, global_map, scope_local_map, prefix + "\t"));
			}
			else if ((t.str == "for") || (t.str == "foreach")) {
				output.append(convert_for(t, token_producer, global_map, scope_local_map, false, prefix + "\t"));
			}
			else if (t.str == "while") {
				output.append(convert_while(t, token_producer, global_map, scope_local_map, false, prefix + "\t"));
			}
			else if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else {
				output.append(convert_expression(t, token_producer, global_map, scope_local_map, "*", false, prefix + "\t"));
			}
		}
		output.append(prefix + "}\n");
		t = token_producer.get_next_Token(); //skip end
		return output;
	}

	std::string convert_actor_parameters(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& name_type_map,
		std::map<std::string, std::string>& default_value_map,
		std::string prefix)
	{
		std::string result;

		if (t.str != "(") {
			throw Wrong_Token_Exception{ "Expected \'(\' but found " + t.str };
		}

		t = token_producer.get_next_Token();
		//t must be a type now and we can start parsing
		while (t.str != ")") {
			std::string type;
			std::string name;
			std::string default_value;

			type = convert_type(t, token_producer, global_map);
			name = t.str;
			t = token_producer.get_next_Token();
			if (t.str == "=") {
				// default value given
				t = token_producer.get_next_Token();
				while ((t.str != ")") && (t.str != ",")) {
					if (global_map.contains(t.str)) {
						default_value.append(global_map[t.str]);
					}
					else {
						default_value.append(t.str);
					}
					t = token_producer.get_next_Token();
				}
			}
			if (t.str == ",") {
				t = token_producer.get_next_Token();
			}
			global_map[name] = default_value;
			if (!default_value.empty()) {
				default_value_map[name] = default_value;
			}
			name_type_map[name] = type;
			result.append(prefix + type + " " + name + ";\n");
		}
		t = token_producer.get_next_Token();
		return result;
	}
}