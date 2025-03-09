#pragma once

#include <vector>
#include "Buffer.hpp"
#include "Tokenizer.hpp"

/* Buffer for native function declarations (tokens).*/
class Native_Buffer : public Buffer {

	void buffer_native(Token& t, Tokenizer& token_producer) {
		if (t.str == "@native") {
			//read action head - FIXME: Do we have to consider endprocedure or endfunction here?
			while (t.str != "end") {
				tokens.push_back(t);
				t = token_producer.get_next_token();
			}
			tokens.push_back(t);
			t = token_producer.get_next_token();
		}
	}

public:
	Native_Buffer(Token& t, Tokenizer& token_producer) {
		buffer_native(t, token_producer);
	}
};