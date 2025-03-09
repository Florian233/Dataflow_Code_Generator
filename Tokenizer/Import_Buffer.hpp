#pragma once

#include <vector>
#include "Buffer.hpp"
#include "Tokenizer.hpp"

/* Buffer for import statements (tokens).*/
class Import_Buffer : public Buffer {

	void buffer_import(Token& t, Tokenizer& token_producer) {
		if ((t.str == "package") || (t.str == "import")) {
			while ((t.str != "actor") && (t.str != "unit")) {
				tokens.push_back(t);
				t = token_producer.get_next_token();
			}
		}
	}

public:
	Import_Buffer(Token& t, Tokenizer& token_producer) {
		buffer_import(t, token_producer);
	}
};