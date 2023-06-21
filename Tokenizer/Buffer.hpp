#pragma once

#include "Token_Container.hpp"
#include <vector>

/* Base class for the specific buffer classes. It provides basic functionality to buffer scopes,
   iterate through the buffered tokens, print them and reset the reading.
 */
class Buffer : public Token_Container {
	using BufferIterator = Token*;
protected:
	Buffer() {};

	std::vector<Token> tokens{};
	unsigned index{ 0 };

	void buffer_bracket(Token& t, Tokenizer& token_producer) {
		if (t.str == "[") {
			tokens.push_back(t);
			t = token_producer.get_next_Token();
			while (t.str != "]") {
				if ((t.str == "[") || (t.str == "(")) {
					buffer_bracket(t, token_producer);
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File" };
				}
				else {
					tokens.push_back(t);
					t = token_producer.get_next_Token();
				}
			}
			tokens.push_back(t);
			t = token_producer.get_next_Token();
		}
		else if (t.str == "(") {
			tokens.push_back(t);
			t = token_producer.get_next_Token();
			while (t.str != ")") {
				if ((t.str == "[") || (t.str == "(")) {
					buffer_bracket(t, token_producer);
				}
				else if (t.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File" };
				}
				else {
					tokens.push_back(t);
					t = token_producer.get_next_Token();
				}
			}
			tokens.push_back(t);
			t = token_producer.get_next_Token();
		}
	}

	/* Function stops when either a closing end or a closing "end_token" is found.
	 * This is done to support the use of end + "construct name".
	 */
	void buffer_scope(Token& t, Tokenizer& token_producer, std::string end_token) {
		while ((t.str != "end") && (t.str != end_token)) {
			tokens.push_back(t);
			t = token_producer.get_next_Token();

			if (t.str == "if") {
				buffer_scope(t, token_producer, "endif");
			}
			else if (t.str == "while") {
				buffer_scope(t, token_producer, "endwhile");
			}
			else if (t.str == "foreach") {
				buffer_scope(t, token_producer, "endforeach");
			}
			else if (t.str == "begin") {
				//can only end with "end" not endbegin
				buffer_scope(t, token_producer, "end");
			}
			else if ((t.str == "(") || (t.str == "[")) {
				buffer_bracket(t, token_producer);
			}
			else if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File" };
			}
		}

		// push the end token
		tokens.push_back(t);
		t = token_producer.get_next_Token();
	}

public:

	Token get_next_Token() {
		if (index >= tokens.size()) {
			return Token{ "" };
		}
		Token t = tokens[index];
		index++;
		return t;
	}

	void reset_buffer() { index = 0; }

	void decrement_index(unsigned dec_val) {
		if (index >= dec_val) {
			index -= dec_val;
		}
		else {
			index = 0;
		}
	}

	void print_buffer(void) {
		for (auto it = tokens.begin(); it != tokens.end(); ++it) {
			std::cout << it->str << " ";
		}
		std::cout << std::endl;
	}

	BufferIterator begin(void) {
		return &tokens[0];
	}

	BufferIterator end(void) {
		Token* p = &tokens[tokens.size() - 1];
		return ++p;
	}
};