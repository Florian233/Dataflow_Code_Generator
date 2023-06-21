#pragma once

#include <vector>
#include "Buffer.hpp"
#include "Tokenizer.hpp"

/* Class to buffer the parameters (tokens) of an actor. */
class Parameter_Buffer : public Buffer {

	void buffer_parameters(Token& t, Tokenizer& token_producer) {
		if (t.str == "(") {
			buffer_bracket(t, token_producer);
		}
	}

public:
	Parameter_Buffer(Token& t, Tokenizer& token_producer) {
		buffer_parameters(t, token_producer);
	}
};