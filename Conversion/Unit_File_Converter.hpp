#pragma once

#include <string>
#include <map>
#include "Actor_Conversion_Data.hpp"
#include "Exceptions.hpp"
#include <filesystem>

namespace Conversion_Helper {

	/* Result of the conversion of an unit file.
	 * Result is split into declarations and the remaining code.
	 */
	typedef struct {
		std::string declarations;
		std::string code;
	} converted_unit_file;

	/* This exception is thrown if the symbol that shall be imported is not found in the imported file. */
	class Imported_Symbol_Not_Found_Exception : public Converter_Exception {
	public:
		Imported_Symbol_Not_Found_Exception(std::string _str) : Converter_Exception{ _str } {};
	};

	/*
		This function takes the requested symbol to be imported by an actor and the prefix for each generated LoC and returns a string containing c++ code.
		The function generates c++ code for the requested symbol. If the requested symbol is a * it returns all symbols.
		A symbol is either a constant value, a function/procedure or a @native declaration.
		For @native declarations no code is generated. Instead the native_flag is set to true to indicate that the string generated in Initialization shall be included to the output.
	*/
	converted_unit_file convert_unit_file(
		std::map<std::string, std::string>& symbol_map,
		Actor_Conversion_Data& conversion_data,
		const std::string source_dir,
		const std::filesystem::path path,
		const std::string requested_symbol,
		const std::string prefix = "");
}