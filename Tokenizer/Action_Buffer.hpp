#pragma once

#include "Tokenizer.hpp"
#include "Buffer.hpp"

/* Buffer for action definitions (tokens). */
class Action_Buffer: public Buffer {

	void buffer_action(Token& t, Tokenizer& token_producer) {
		//read action head 
		while ((t.str != "do") && (t.str != "end") && (t.str != "endaction")) {
			if (t.str == "[") {//list comprehension can contain an end, this would break this loop, but it should
				buffer_bracket(t, token_producer);
			}
			else if (t.str == "if") {
				buffer_scope(t, token_producer, "endif");
			}
			else if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File" };
			}
			else {
				tokens.push_back(t);
				t = token_producer.get_next_Token();
			}
		}
		// read rest
		if (t.str == "do") {
			//This is the only case where we have to differentiate this inside a buffer function
			token_producer.set_context(Conversion_Helper::Context::Action_Body);
			buffer_scope(t, token_producer, "endaction");
		}
		else {
			// must be some kind of end token
			tokens.push_back(t);
			t = token_producer.get_next_Token();
		}
	}

public:
	Action_Buffer(Token& t, Tokenizer& token_producer) {
		buffer_action(t, token_producer);
	}
};