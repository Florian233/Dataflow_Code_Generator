#pragma once
#include <string>

/*
	Parent class to make it possible to use the same functions with a Action_Buffer or a Tokenizer as input.
	The Token constructor proved basic translation from CAL to C of 'mod', 'and', 'or', 'not' and 'abs'.
*/
struct Token {
	std::string str;

	Token(std::string s) : str{ s } {
		if (str == "mod") {
			str = "%";
		}
		else if (str == "and") {
			str = "&&";
		}
		else if (str == "or") {
			str = "||";
		}
		else if (str == "not") {
			str = "!";
		}
		else if (str == "abs") {
			//abs is defined by the c++ compiler and by the most popular ORCC include file
			str = "abs_no_name_collision";
		}
	};
	Token() {};
};

class Token_Container {
public:
	virtual Token get_next_token() = 0;
};