#pragma once

#include <vector>
#include "Buffer.hpp"
#include "Tokenizer.hpp"

/* Buffer for function and procedure definitions (tokens). */
class Method_Buffer : public Buffer {

	void buffer_method(Token& t, Tokenizer& token_producer) {

		if (t.str == "procedure") {
			// procedure bodies can start with do or begin, but begin is also a keyword to start a new block that has to be ended with end
			while ((t.str != "end") && (t.str != "do") && (t.str != "endprocedure")
				&& (t.str != "begin"))
			{
				if (t.str == "[") {//list comprehension can contain an end, this would break this loop, but it should
					buffer_bracket(t, token_producer);
				}
				else if (t.str == "if") {
					buffer_scope(t, token_producer, "endif");
				}
				else {
					tokens.push_back(t);
					t = token_producer.get_next_Token();
				}
			}
			// read rest
			if ((t.str == "begin") || (t.str == "do")) {
				buffer_scope(t, token_producer, "endprocedure");
			}
			else {
				// push end token
				tokens.push_back(t);
				t = token_producer.get_next_Token();
			}
		}
		else if (t.str == "function") {
			// function bodies are started with do, not begin
			buffer_scope(t, token_producer, "endfunction");
		}
	}

public:
	Method_Buffer(Token& t, Tokenizer& token_producer) {
		buffer_method(t, token_producer);
	}
};