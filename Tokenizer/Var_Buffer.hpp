#pragma once

#include <vector>
#include "Buffer.hpp"
#include "Tokenizer.hpp"

/* Buffer to buffer variables (tokens) including assignments */
class Var_Buffer : public Buffer {

	bool is_const_assignment{ false };

	void buffer_var(Token& t, Tokenizer& token_producer) {
		//read action head 
		while (t.str != ";") {
			// avoid interpreting ='s for types or list sizes as assignment
			if ((t.str == "(") || (t.str == "[")) {
				buffer_bracket(t, token_producer);
			}
			else {
				if (t.str == "=") {
					is_const_assignment = true;
				}
				tokens.push_back(t);
				t = token_producer.get_next_Token();
			}
		}
		tokens.push_back(t);
		t = token_producer.get_next_Token();
	}

public:
	Var_Buffer(Token& t, Tokenizer& token_producer) {
		buffer_var(t, token_producer);
	}

	bool is_const(void) {
		return is_const_assignment;
	}
};