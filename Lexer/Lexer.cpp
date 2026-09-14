#include "Lexer.hpp"
#include <cctype>
#include <iostream>
#include "common/include/String_Helper.h"

static bool is_digit(unsigned char c) {
	return (std::isdigit(c) != 0);
}

static bool is_hex(unsigned char c) {
	if (is_digit(c) || (c == 'a') || (c == 'A') || (c == 'b') || (c == 'B') || (c == 'c')
		|| (c == 'C') || (c == 'd') || (c == 'D') || (c == 'e') || (c == 'E') || (c == 'f') || (c == 'F'))
	{
		return true;
	}
	return false;
}

static bool is_letter(unsigned char c) {
	return (std::isalpha(c) != 0);
}

static bool is_whitespace(unsigned char c) {
	return (std::isspace(static_cast<unsigned char>(c)) != 0);
}

static bool is_literal(unsigned char c) {
	return ((std::isalnum(c) != 0) || (c == '_'));
}

static bool is_linebreak(unsigned char c) {
	return (c == '\n');
}

static bool is_operator(unsigned char c) {
	switch (c) {
	case '!':
	case '@':
	case '#':
	case '$':
	case '%':
	case '^':
	case '&':
	case '*':
	case '/':
	case '+':
	case '-':
	case '=':
	case '<':
	case '>':
	case '?':
	case '~':
	case '|':
		return true;
	default:
		return false;
	}
}

static bool is_delimiter1(unsigned char c) {
	switch (c) {
	case ':':
	case '(':
	case ')':
	case '{':
	case '}':
	case '[':
	case ']':
		return true;
	default:
		return false;
	}
}

static bool is_delimiter2(unsigned char c) {
	switch (c) {
	case ',':
	case '.':
	case ';':
		return true;
	default:
		return false;
	}
}

static bool is_keyword(std::string s) {
	//to_lower_case(s);
	if ((s == "action") ||
		(s == "actor") ||
		(s == "all") ||
		(s == "and") ||
		(s == "any") ||
		(s == "at") ||
		(s == "begin") ||
		(s == "choose") ||
		(s == "const") ||
//		(s == "delay") ||
		(s == "div") ||
		(s == "do") ||
		(s == "dom") ||
		(s == "else") ||
		(s == "end") ||
		(s == "endaction") ||
		(s == "endactor") ||
		(s == "endchoose") ||
		(s == "endforeach") ||
		(s == "endfunction") ||
		(s == "endif") ||
		(s == "endinitialize") ||
		(s == "endlambda") ||
		(s == "endlet") ||
		(s == "endpriority") ||
		(s == "endproc") ||
		(s == "endprocedure") ||
		(s == "endschedule") ||
		(s == "endwhile") ||
		(s == "false") ||
		(s == "for") ||
		(s == "foreach") ||
		(s == "fsm") ||
		(s == "function") ||
		(s == "guard") ||
		(s == "if") ||
		(s == "import") ||
		(s == "in") ||
		(s == "initialize") ||
		(s == "lambda") ||
		(s == "let") ||
		(s == "map") ||
		(s == "mod") ||
		(s == "multi") ||
		(s == "mutable") ||
		(s == "not") ||
		(s == "null") ||
		(s == "old") ||
		(s == "or") ||
		(s == "priority") ||
		(s == "proc") ||
		(s == "procedure") ||
		(s == "regexp") ||
		(s == "repeat") ||
		(s == "rng") ||
		(s == "schedule") ||
		(s == "then") ||
		(s == "time") ||
		(s == "true") ||
		(s == "var") ||
		(s == "while") ||
		(s == "assign") ||
		(s == "case") ||
		(s == "default") ||
		(s == "endinvariant") ||
		(s == "endtask") ||
		(s == "endtype") ||
		(s == "ensure") ||
		(s == "invariant") ||
		(s == "now") ||
		//(s == "out") ||
		(s == "protocol") ||
		(s == "package") ||
		(s == "require") ||
		(s == "task") ||
		(s == "type") ||
		(s == "native") ||
		(s == "half") ||
		(s == "float") ||
		(s == "list") ||
		(s == "List") ||
		(s == "uint") ||
		(s == "int") ||
		(s == "type") ||
		(s == "size") ||
		(s == "unit") ||
		(s == "String") ||
		(s == "bool"))
	{
		return true;
	}
	return false;
}

bool Lexer::Lexer::skip_comment(void)
{
	/* lookahead by one */
	size_t tmp_index = index + 1;
	if (tmp_index > max_index) {
		//out of bounds, 
		index = max_index + 1;
		return false;
	}
	if (str[tmp_index] == '/') {
		++index;
		++character;
		while ((index <= max_index) && (str[index] != '\n')) {
			++index;
			++character;
		}
		return false;
	}
	else if (str[tmp_index] == '*') {
		index += 2;
		character += 2;

		if (index > max_index) {
			index = max_index + 1;
			return false;
		}

		bool prev_star = false;

		while (index <= max_index) {
			if (str[index] == '/' && prev_star) {
				++index;
				return false;
			}
			if (str[index] == '\n') {
				++line;
				character = 0;
			}
			else {
				++character;
			}
			prev_star = (str[index] == '*');

			++index;
		}
		return false;
	}
	else {
		return true;
	}
}

void Lexer::Lexer::find_next_char(void)
{
	for (; index <= max_index;) {
		char& c = str[index];
		switch (c) {
		case ' ':
			++character;
			++index;
			break;
		case '\n':
			++line;
			++index;
			character = 0;
			break;
		case '\t':
			++index;
			++character;
			break;
		case '/':
		{
			bool ret = skip_comment();
			if (ret) {
				return;
			}
		}
			break;
		default:
			return;
		}
	}
}

Lexer::Token Lexer::Lexer::scan_string(void)
{
	std::string t{"\""};
	bool escape = false;
	bool r = false;
	unsigned token_start = character;
	unsigned token_line = line;
	++index;
	++character;

	while ((index <= max_index)) {
		char& c = str[index];
		if (c == '\\') {
			escape = !escape;
		}
		else if (c == '\"') {
			if (!escape) {
				r = true;
			}
			escape = false;
		}
		else if (is_linebreak(c)) {
			break;
		}
		else {
			escape = false;
		}
		t += c;
		++index;
		++character;

		if (r) {
			return Token{ .str = t,  .line = token_line, .character = token_start, .type = Token_Class::String };
		}
	}

	throw Lexer_Exception_Unexpected_EOF{line, character, "String not closed."};
}

Lexer::Token Lexer::Lexer::read_number(
	char first_character,
	unsigned token_line,
	unsigned token_start)
{
	std::string current_token = "";
	bool real = false;
	bool hex = false;
	bool leading_zero = false;

	if (first_character == '0') {
		leading_zero = true;

		current_token += first_character;
		++index;
		++character;
		if (index <= max_index) {
			first_character = str[index];
			if ((first_character == 'x') || (first_character == 'X')) {
				hex = true;
				current_token += first_character;
				++index;
				++character;

				if (index > max_index) {
					throw Lexer_Exception_Unexpected_EOF(token_line, character, "Hex Number incomplete.");
				}

				first_character = str[index];
			}
		}
	}

	while (index <= max_index) {
		if (hex) {
			if (is_hex(first_character)) {
				current_token += first_character;
				++index;
				++character;
			}
			else {
				if ((index <= max_index) && is_literal(first_character)) {
					throw Lexer_Exception_Unexpected_Character(first_character, token_line, character, "Hexadecimal number of terminated properly.");
				}
				else {
					/* bail out here, hex cannot have an exponent */
					return Token{ .str = current_token,  .line = token_line, .character = token_start, .type = Token_Class::NumericLiteral };
				}
			}
		}
		else {

			if (first_character == '.') {
				if (real) {
					break;
				}
				real = true;

				current_token += first_character;
				++index;
				++character;
			}
			else if (is_digit(first_character)) {
				current_token += first_character;
				++index;
				++character;
			}
			else {
				break;
			}
		}
		if (index <= max_index) {
			first_character = str[index];
		}
	}

	if (index > max_index) {
		return Token{ .str = current_token,  .line = token_line, .character = token_start, .type = Token_Class::NumericLiteral };
	}

	/* Let's have a look at the exponent */
	if (first_character == 'e' || first_character == 'E') {
		current_token += first_character;
		++index;
		++character;
		if (index > max_index) {
			throw Lexer_Exception_Unexpected_EOF(token_line, character, "Exponent incomplete.");
		}
		first_character = str[index];

		if (first_character == '+' || first_character == '-') {
			current_token += first_character;
			++index;
			++character;
			if (index > max_index) {
				throw Lexer_Exception_Unexpected_EOF(token_line, character, "Exponent incomplete.");
			}
			first_character = str[index];
		}

		while (index <= max_index) {
			if (is_digit(first_character)) {
				current_token += first_character;
				++index;
				++character;

				if (index <= max_index) {
					first_character = str[index];
				}
			}
			else {
				if (is_literal(first_character)) {
					throw Lexer_Exception_Unexpected_Character(first_character, token_line, character, "Eponent not terminated properly.");
				}
				else {
					break;
				}
			}
		}
	}
	else {
		if ((index <= max_index) && is_literal(first_character)) {
			throw Lexer_Exception_Unexpected_Character(first_character, token_line, character, "Number not terminated properly.");
		}
	}

	return Token{ .str = current_token,  .line = token_line, .character = token_start, .type = Token_Class::NumericLiteral };
}

Lexer::Token Lexer::Lexer::read_operator(
	char first_character,
	unsigned token_line,
	unsigned token_start)
{
	std::string current_token{};
	while (is_operator(first_character)) {
		current_token += first_character;
		++index;
		++character;
		first_character = str[index];
	}
	return Token{ .str = current_token,  .line = token_line, .character = token_start, .type = Token_Class::Operator };
}

Lexer::Token Lexer::Lexer::read_delimiter(
	char first_character,
	unsigned token_line,
	unsigned token_start)
{
	std::string current_token{};
#if 0
	while (is_delimiter1(first_character)) {
		current_token += first_character;
		++index;
		++character;
		first_character = str[index];
	}
#else
	current_token += first_character;
	++index;
	++character;
#endif
	return Token{ .str = current_token,  .line = token_line, .character = token_start, .type = Token_Class::Delimiter };
}

Lexer::Token Lexer::Lexer::read_delimiter2(
	char first_character,
	unsigned token_line,
	unsigned token_start)
{
	std::string current_token{};
#if 0
	while (is_delimiter2(first_character)) {
		current_token += first_character;
		++index;
		++character;
		first_character = str[index];
	}
#else
	current_token += first_character;
	++index;
	++character;
#endif
	return Token{ .str = current_token,  .line = token_line, .character = token_start, .type = Token_Class::Delimiter2 };
}

Lexer::Token Lexer::Lexer::next(void)
{
	if (index >= str.size()) {
		return Token{ .str = END };
	}

	find_next_char();

	//no tokens to read anymore
	if (index > max_index) {
		return Token{ .str = END };
	}

	std::string current_token{};
	unsigned token_start = character;
	unsigned token_line = line;
	//check for special characters
	char& first_character = str[index];

	/* All the index out of bounds warnings in the following code can be ignored.
	   [] on string will return a null char, this will lead to a token with the
	   content up to the end of the string and an update of index.
	   Index will then point at the end of the string, hence, no further token
	   is produced.
	 */
	switch (first_character) {
	case ':':
	case '(':
	case ')':
	case '{':
	case '}':
	case '[':
	case ']':
		return read_delimiter(first_character, token_line, token_start);
	case '!':
	case '@':
	case '#':
	case '$':
	case '%':
	case '^':
	case '&':
	case '*':
	case '/':
	case '+':
	case '-':
	case '=':
	case '<':
	case '>':
	case '?':
	case '~':
	case '|':
		return read_operator(first_character, token_line, token_start);
	case ',':
	case '.':
	case ';':
		return read_delimiter2(first_character, token_line, token_start);
	case '\"':
		return scan_string();
		break;
	default:
		if (is_digit(first_character)) {
			return read_number(first_character, token_line, token_start);
		}
		if (is_literal(first_character)) {
			while ((index <= max_index) && is_literal(first_character)) {
				current_token += first_character;
				++index;
				++character;
				if (index <= max_index) {
					first_character = str[index];
				}
			}
			bool keyword = is_keyword(current_token);
			if (keyword) {
				to_lower_case(current_token);
			}
			/* some translation to make the parser a bit simpler */
			if (current_token == "and") {
				return Token{ .str = "&&", .line = token_line, .character = token_start,
							  .type = Token_Class::Operator };
			}
			else if (current_token == "or") {
				return Token{ .str = "||", .line = token_line, .character = token_start,
							  .type = Token_Class::Operator };
			}
			else if (current_token == "not") {
				return Token{ .str = "!", .line = token_line, .character = token_start,
						      .type = Token_Class::Operator };
			}
			else if (current_token == "mod") {
				return Token{ .str = "%", .line = token_line, .character = token_start,
			  .type = Token_Class::Operator };
			}
			else {

				return Token{ .str = current_token,  .line = token_line, .character = token_start,
							  .type = keyword ? Token_Class::Keyword : Token_Class::Identifier };
			}
		}
		else {
			throw Lexer_Exception_Unknown_Character{ first_character, line, character, "."};
		}
	}

	return Token{ .str = END };
}