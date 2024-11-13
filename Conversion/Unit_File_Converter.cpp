#include "Reader/Reader.hpp"
#include "Unit_File_Converter.hpp"
#include "Code_Generation/Language/C_Cpp/Converter_RVC_Cpp.hpp"
#include "Config/debug.h"
#include <filesystem>
using namespace Converter_RVC_Cpp;
using namespace Conversion_Helper;

/*
	Class to generate C++ code for a given unit file.
*/
class Unit_Generator {

	Tokenizer token_producer;

	Actor_Conversion_Data& actor_conversion_data;

	std::string path;//path to source dir

	// Path to file, only used for error case output
	std::filesystem::path file_path;

	//map where all constants are inserted. This map is used to find constants if they are used in the calculation of the size of a type.
	//This map holds all constants in global or actor namespace
	std::map<std::string, std::string>& const_map;

	//functionality is equal to const_map, but for local namespaces, like function bodies
	std::map<std::string, std::string> local_map;

	converted_unit_file convert_import(Token& t) {
		if (t.str == "import") {
			std::filesystem::path path_to_import_file{ path };
			std::string symbol{};
			path_to_import_file.append(token_producer.get_next_Token().str); //must be part of the path
			Token previous_token = token_producer.get_next_Token();
			Token next_token = token_producer.get_next_Token();

			if (previous_token.str == "all") {
				symbol = "*";
				previous_token = next_token;
				next_token = token_producer.get_next_Token();
			}

			for (;;) {
				if (next_token.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				} else if (next_token.str != ";") {
					if (previous_token.str != ".") {
						path_to_import_file /= previous_token.str;
					}
					previous_token = next_token;
					next_token = token_producer.get_next_Token();
				}
				else if (next_token.str == ";") {
					if (symbol.empty()) {
						symbol = previous_token.str;
					}
					else {
						if (previous_token.str != ".") {
							path_to_import_file /= previous_token.str;
						}
						else {
							throw Wrong_Token_Exception{ "Found unexpected Token." };
						}
					}
					path_to_import_file += ".cal";
					break;
				}
			}
			Unit_Generator gen(path_to_import_file, const_map, actor_conversion_data, path);
			auto output = gen.convert(symbol, "\t");
			return output;
		}
		else {
			throw Wrong_Token_Exception{ "Cannot happend!" };
		}
	}

public:
	Unit_Generator(
		std::filesystem::path _path,
		std::map<std::string, std::string>& map,
		Actor_Conversion_Data& data,
		std::string source_dir)
		: token_producer{""}, path{source_dir}, file_path{_path}, const_map{map},
		actor_conversion_data{data}
	{
		std::ifstream file(_path, std::ifstream::in);
		if (file.fail()) {
			throw Network_Reader::Network_Reader_Exception{ "Cannot open the file " + _path.string()};
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		token_producer = Tokenizer{ buffer.str() };
	}

	/*
		This function takes the requested symbol to imported by an actor and the prefix for each generated LoC and returns a string containing c++ code.
		The function generates c++ code for the requested symbol. If the requested symbol is a * it returns all symbols.
		A symbol is either a constant value, a function/procedure or a @native declaration.
		For @native declarations no code is generated. Instead the native_flag is set to true to indicate that the string generated in Initialization shall be included to the output.
	*/
	converted_unit_file convert(
		const std::string requested_symbol,
		const std::string prefix = "")
	{
		std::string code{};
		std::string declarations;
		bool stop{ false };
		bool found_symbol{ false };
		if (requested_symbol == "*") {
			// looking for nothing specific...
			found_symbol = true;
		}
		else if (actor_conversion_data.get_symbol_map().contains(requested_symbol)) {
			//symbol is already defined, what to do now?
			//FIXME
		}
		Token t = token_producer.get_next_Token();
		//skip everything before unit
		while (t.str != "unit") {
			if (t.str == "import") {
				auto r = convert_import(t);
				code += r.code;
				declarations += r.declarations;
			}
			else if (t.str == "") {
				//nothing in this file to be imported, leave
				stop = true;
				break;
			}
			t = token_producer.get_next_Token();
		}

		if (!stop) {
			Token name = token_producer.get_next_Token();
			Token doppelpunkt = token_producer.get_next_Token();
			Token next_token = token_producer.get_next_Token();
			while (next_token.str != "end") {
				if (next_token.str == "@native") {
					declarations += convert_native_declaration(next_token, token_producer, requested_symbol, actor_conversion_data);
				}
				else if ((next_token.str == "uint") || (next_token.str == "int")
					|| (next_token.str == "String") || (next_token.str == "bool")
					|| (next_token.str == "half") || (next_token.str == "float"))
				{
					code += convert_expression(next_token, token_producer, const_map, const_map, actor_conversion_data.get_symbol_type_map(), requested_symbol, false, prefix);
				}
				else if (next_token.str == "function") {
					std::string tmp{ convert_function(next_token, token_producer, const_map, actor_conversion_data.get_symbol_type_map(), prefix, requested_symbol) };
					code += tmp;
					//find declaration and insert it at the beginning of the source string, to avoid linker errors
					//std::string dekl = tmp.substr(0, tmp.find("{")) + ";\n";
					//declarations.insert(0, dekl);
				}
				else if (next_token.str == "procedure") {
					std::string tmp{ convert_procedure(next_token, token_producer, const_map, actor_conversion_data.get_symbol_type_map(), prefix, requested_symbol) };
					code += tmp;
					//find declaration and insert it at the beginning of the source string, to avoid linker errors
					//std::string dekl = tmp.substr(0, tmp.find("{")) + ";\n";
					//declarations.insert(0, dekl);
				}
				else if (next_token.str == "List") {
					code += convert_list(next_token, token_producer, const_map, const_map, actor_conversion_data.get_symbol_type_map(), requested_symbol, prefix);
				}
				else if (next_token.str == "") {
					throw Wrong_Token_Exception{ "Unexpected End of File." };
				}
				else {
					throw Wrong_Token_Exception{ "Unexpected token during processing of " + file_path.string()};
				}
			}
		}
		if (actor_conversion_data.get_symbol_map().contains(requested_symbol)) {
			found_symbol = true;
		}
		if (!found_symbol) {
			throw Imported_Symbol_Not_Found_Exception{ "Didn't find symbol " + requested_symbol + " in " + file_path.string() + "."};
		}
		converted_unit_file c;
		c.code = code;
		c.declarations = declarations;
		return c;
	}
};

converted_unit_file Conversion_Helper::convert_unit_file(
	std::map<std::string, std::string>& symbol_map,
	Actor_Conversion_Data& conversion_data,
	const std::string source_dir,
	const std::filesystem::path path,
	const std::string requested_symbol,
	const std::string prefix)
{
#ifdef DEBUG_NETWORK_READ
	std::cout << "Reading imported file " << path << "." << std::endl;
#endif
	Unit_Generator gen(path, symbol_map, conversion_data, source_dir);
	auto output = gen.convert(requested_symbol, prefix);
	return output;
}
