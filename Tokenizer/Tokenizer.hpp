#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include "Token_Container.hpp"
#include "Exceptions.hpp"
#include "Conversion/Unsupported.hpp"

/*
	Class that produces tokens from an string. It directly takes the string as an argument.
	Before return a token, it is checked whether the token contains a keyword of an unsupported
	CAL feature. In this case an exception is raised. For this purpose the context has to be set
	properly, if no checking shall be applied set the context to none.
	When reading more tokens than available an empty token will be returned.
*/
class Tokenizer : public Token_Container {
	//no reference because i want to be able to destroy the string that is used as a parameter
	std::string str_to_tokenize;
	size_t index{ 0 };
	Conversion_Helper::Context context = Conversion_Helper::Context::None;

	/*
		Function to skip comments, white spaces, tabs and newlines.
	*/
	void find_next_valid_character() {
		for (; index < str_to_tokenize.size(); index++) {
		char& character = str_to_tokenize[index];
		switch (character) {
		case ' ':
			break;
		case '\n':
			break;
		case '\t':
			break;
		case '/':
		{
			size_t tmp_index = index + 1;
			if (tmp_index >= str_to_tokenize.size()) {
				//out of bounds, 
				index = str_to_tokenize.size();
				return;
			}
			if (str_to_tokenize[tmp_index] == '/') {
				size_t found = str_to_tokenize.find('\n', index);
				if (found == std::string::npos) {
					index = str_to_tokenize.size();
					return;
				}
				else {
					index = found;
				}
			}
			else if (str_to_tokenize[tmp_index] == '*') {
				size_t found = str_to_tokenize.find("*/", index);
				if (found == std::string::npos) {
					index = str_to_tokenize.size();
					return;
				}
				else {
					index = found + 1;
				}
			}
			else {
				return;
			}
		}
			break;
		default:
			return;
		}
		}
	}

public:
	/*
		Constructors as described above.
	*/
	Tokenizer(std::string text) : str_to_tokenize{ text } {};

	/*
		Function returns the next token.
		If all tokens have been read this function just returns a token with an empty string. This is the only case when an empty string can be returned.
	*/
	virtual Token get_next_token() {
		//first set the index to the next relevant character
		find_next_valid_character();

		//no tokens to read anymore
		if (index == str_to_tokenize.size()) {
			//throw Converter_RVC_Cpp::Tokenizer_Exception{ "Tokenizer Index out of Bounds" };
			return Token{ "" };
		}
		
		std::string current_token{};

		//check for special characters
		char& first_character = str_to_tokenize[index];

		/* All the index out of bounds warnings in the following code can be ignored.
		   [] on string will return a null char, this will lead to a token with the
		   content up to the end of the string and an update of index.
		   Index will then point at the end of the string, hence, no further token
		   is produced.
		 */
		switch (first_character) {
		case '*':
			if (str_to_tokenize[index + 1] == '*') {
				index += 2;
				current_token = "**";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else {
				++index;
				current_token = "*";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			break;
		case '/':
			index++;
			current_token = "/";
			Conversion_Helper::construct_supported(context, current_token);
			return Token{ current_token };
			break;
		case ':':
			if (str_to_tokenize[index + 1] == '=') {
				index += 2;
				current_token = ":=";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else {
				++index;
				current_token = ":";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			break;
		case ';':
			++index;
			current_token = ";";
			Conversion_Helper::construct_supported(context, current_token);
			return Token{ current_token };
			break;
		case '=':
			if ((str_to_tokenize[index + 1] == '=') && (str_to_tokenize[index + 2] == '>')) {
				index += 3;
				current_token = "==>";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else if (str_to_tokenize[index + 1] == '=') {
				index += 2;
				current_token = "==";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else {
				++index;
				current_token = "=";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			break;
		case ',':
			++index;
			current_token = ",";
			Conversion_Helper::construct_supported(context, current_token);
			return Token{ current_token };
			break;
		case '(':
			++index;
			current_token = "(";
			Conversion_Helper::construct_supported(context, current_token);
			return Token{ current_token };
			break;
		case ')':
			++index;
			current_token = ")";
			Conversion_Helper::construct_supported(context, current_token);
			return Token{ current_token };
			break;
		case '[':
			++index;
			current_token = "[";
			Conversion_Helper::construct_supported(context, current_token);
			return Token{ current_token };
			break;
		case ']':
			++index;
			current_token = "]";
			Conversion_Helper::construct_supported(context, current_token);
			return Token{ current_token };
			break;
		case '&':
			if ((str_to_tokenize[index + 1] == '&') && (str_to_tokenize[index + 2] == '&')) {
				index += 3;
				current_token = "&&&";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else if (str_to_tokenize[index + 1] == '&' ) {
				index += 2;
				current_token = "&&";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else {
				++index;
				current_token = "&";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			break;
		case '<':
			if ((str_to_tokenize[index + 1] == '<') && (str_to_tokenize[index + 2] == '<')) {
				index += 3;
				current_token = "<<<";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else if (str_to_tokenize[index + 1] == '<') {
				index += 2;
				current_token = "<<";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else if (str_to_tokenize[index + 1] == '=') {
				index += 2;
				current_token = "<=";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else { 
				++index;
				current_token = "<";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			break;
		case '>':
			if ((str_to_tokenize[index + 1] == '>') && (str_to_tokenize[index + 2] == '>')) {
				index += 3;
				current_token = ">>>";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else if (str_to_tokenize[index + 1] == '>') {
				index += 2;
				current_token = ">>";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else if (str_to_tokenize[index + 1] == '=') { 
				index += 2;
				current_token = ">=";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else { 
				++index;
				current_token = ">";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			break;
		case '|':
			if ((str_to_tokenize[index + 1] == '|') && (str_to_tokenize[index + 2] == '|')) {
				index += 3;
				current_token = "|||";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else if (str_to_tokenize[index + 1] == '|') { 
				index += 2;
				current_token = "||";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else { 
				++index;
				current_token = "|";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			break;
		case '\"':
			++index;
			current_token = "\"";
			Conversion_Helper::construct_supported(context, current_token);
			return Token{ current_token };
			break;
		case '{':
			++index;
			current_token = "{";
			Conversion_Helper::construct_supported(context, current_token);
			return Token{ current_token };
			break;
		case '}':
			++index;
			current_token = "}";
			Conversion_Helper::construct_supported(context, current_token);
			return Token{ current_token };
			break;
		case '.':
			if (str_to_tokenize[index + 1] == '.') { 
				index += 2;
				current_token = "..";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else { 
				++index;
				current_token = ".";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			break;
		case '!':
			if (str_to_tokenize[index + 1] == '=') { 
				index += 2;
				current_token = "!=";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else { 
				++index;
				current_token = "!";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
		case '-':
			if ((str_to_tokenize[index + 1] == '-') && (str_to_tokenize[index + 2] == '>')) { 
				index += 3;
				current_token = "-->";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else if (str_to_tokenize[index + 1] == '-'){
				index += 2;
				current_token = "--";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else if (str_to_tokenize[index + 1] == '=') { 
				index += 2;
				current_token = "-=";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else { 
				++index;
				current_token = "-";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			break;
		case '+':
			if (str_to_tokenize[index + 1] == '=') { 
				index += 2;
				current_token = "+=";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			if (str_to_tokenize[index + 1] == '+') { 
				index += 2;
				current_token = "++";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			else { 
				++index;
				current_token = "+";
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			break;
		case '\\':
			++index;
			current_token = "\\";
			Conversion_Helper::construct_supported(context, current_token);
			return Token{ current_token };
			break;
		case '?':
			++index;
			current_token = "?";
			Conversion_Helper::construct_supported(context, current_token);
			return Token{ current_token };
			break;
		default:
			current_token += first_character; 
			++index;
		}

		//run until a special character is found. If one of those is found don't add it to the token and return all characters that have been added to this token yet.
		for (;;) {
			if (index == str_to_tokenize.size()) {
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
			}
			char& character = str_to_tokenize[index];
			switch (character) {
			case ' ':
			case '\n':
			case '\t':++index;
			case '*':
			case '/':
			case ':':
			case ';':
			case '=':
			case '(':
			case ')':
			case '[':
			case ']':
			case '&':
			case '<':
			case '>':
			case '|':
			case '-':
			case '+':
			case '\"':
			case '.':
			case '!':
			case '{':
			case '}':
			case ',':
			case '\\':
			case '?':
				Conversion_Helper::construct_supported(context, current_token);
				return Token{ current_token };
				break;
			default:current_token += character; ++index;
			}
			
		}
		throw Tokenizer_Exception{ "Tokenizer Index out of Bounds" };
	}

	void set_context(Conversion_Helper::Context c) {
		context = c;
	}

	Conversion_Helper::Context get_context(void) {
		return context;
	}
};
