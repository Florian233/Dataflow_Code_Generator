#pragma once

#include <string>
#include <exception>

namespace Lexer {

	enum Token_Class
	{
		Identifier,
		Operator,
		Delimiter,
		Delimiter2,
		Keyword,
		String,
		NumericLiteral,
		X
	};

	struct Token {
		std::string str;
		unsigned line = 0;
		unsigned character = 0;
		Token_Class type = X;
	};

	const std::string END = "";

	class Lexer {
	private:
		std::string str;
		unsigned line{ 0 };
		unsigned character{ 0 };
		unsigned index{ 0 };
		unsigned max_index;

		/* 	Function to skip comments, white spaces, tabs and newlines. */
		bool skip_comment(void);
		void find_next_char(void);
		Token scan_string(void);
		Token read_number(char first_character, unsigned token_line, unsigned token_start);
		Token read_operator(char first_character, unsigned token_line, unsigned token_start);
		Token read_delimiter(char first_character, unsigned token_line, unsigned token_start);
		Token read_delimiter2(char first_character, unsigned token_line, unsigned token_start);
	public:

		Lexer(std::string _s) : str{ _s } { max_index = str.empty() ? 0 : static_cast<unsigned>(str.size() - 1); }

		Token next(void);

	};

	class Lexer_Exception : public std::exception {
	public:
		virtual const char* what() const throw() = 0;
	};

	class Lexer_Exception_Unexpected_EOF : public Lexer_Exception {
	public:
		Lexer_Exception_Unexpected_EOF(unsigned _l, unsigned _c, std::string _msg) : Lexer_Exception{ }, line{ _l }, character{ _c }, msg{_msg} {
			local = "Unexpected End of File at line " + std::to_string(line) + " character " + std::to_string(character) + "\n";
			local.append(msg);
		};

		virtual const char* what() const throw()
		{
			return local.c_str();
		}

	private:
		std::string local;
		std::string msg;
		unsigned line;
		unsigned character;
	};

	class Lexer_Exception_Unknown_Character : public Lexer_Exception {
	public:
		Lexer_Exception_Unknown_Character(char _ue, unsigned _l, unsigned _c, std::string _msg) : Lexer_Exception{ }, line{ _l }, character{ _c }, msg{ _msg } {
			local = "Unknown character '" + std::to_string(_ue) + "' at line " + std::to_string(line) + " character " + std::to_string(character) + "\n";
			local.append(msg);
		};

		virtual const char* what() const throw()
		{
			return local.c_str();
		}

	private:
		std::string local;
		std::string msg;
		unsigned line;
		unsigned character;
	};


	class Lexer_Exception_Unexpected_Character : public Lexer_Exception {
	public:
		Lexer_Exception_Unexpected_Character(char _ue, unsigned _l, unsigned _c, std::string _msg) : Lexer_Exception{ }, line{ _l }, character{ _c }, msg{ _msg } {
			local = "Unexpected character '" + std::to_string(_ue) + "' at line " + std::to_string(line) + " character " + std::to_string(character) + "\n";
			local.append(msg);
		};

		virtual const char* what() const throw()
		{
			return local.c_str();
		}

	private:
		std::string local;
		std::string msg;
		unsigned line;
		unsigned character;
	};

}