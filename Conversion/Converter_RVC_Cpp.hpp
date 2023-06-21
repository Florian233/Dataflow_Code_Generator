#pragma once
#include "Tokenizer/Tokenizer.hpp"
#include <string>
#include <utility>
#include <map>
#include "Actor_Conversion_Data.hpp"

/*
	Namespace containing all functions needed to convert basic RVC expressions like functions, loops,...

	Convention: The token reference that is used throughout the whole conversion must point at a token that is the next token after the
				RVC construct that is converted in the respective function after the function completed. 
				At the start of a function the token reference must point at a token that is the start of the respective RVC construct.
*/
namespace Converter_RVC_Cpp {

	/*
		Function to convert a RVC function into C++ code.
		Parameters:
			1. Token Reference containing "function" 
			2. the corresponding token producer to the token
			3. global map for definitions and declarations in the actor or global space
			4. prefix for each line of code
			5. symbol, default * means every symbol
		After the function call the token is one past the code of the function.
		The global map hasn't changed. The function returns the converted code if the symbol is a * or equal to the function name. 
	*/
	std::string convert_function(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::string prefix = "",
		std::string symbol = "*"); 

	/*
		Function to convert a RVC procedure into C++ code.
		Parameters:
			1. Token Reference containing "function"
			2. the corresponding token producer reference to the token
			3. global map for definitions and declarations in the actor or global space
			4. prefix for each line of code
			5. symbol, default * means every symbol
		After the function call the token is one past the code of the procedure.
		The global map hasn't changed. The function returns the converted code if the symbol is a * or equal to the procedure name.
	*/
	std::string convert_procedure(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::string prefix = "",
		std::string symbol = "*"); 
	
	/*
		Function to convert an if statement to C++ code.
 		Parameters:
			1. Token reference containg "if"
			2. The corresponding token container reference to the token
			3. A map (symbol,value) for global or actor definitions/declarations
			4. A map (symbol,value) for the local scope
			5. boolean return statement: true: return in the body of the if, false: no return 
			6. prefix for each generated line of code
			7. boolean nested: true: the if is nested in another if, false: no nesting of if clauses
		The function returns the generated code in a string.
	*/
	std::string convert_if(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		bool return_statement = false,
		std::string prefix = "",
		bool nested = false);
	
	/*
		Function to generate a for or a foreach statement and the loop body in C++ based on RVC Code.
		Parameters:
			1. token reference of the start of the for statement
			2. corresponding token container to the token
			3. Map (symbol,value) for the global and actor scope
			4. Map (Symbol, Value) for the local scope
			5. boolean return statement: true: the expression in the loop body should be a return statement
			6. prefix for each generated line of code
		The function converts the loop from RVC to C++ code.
		Both maps are used for type conversions from RVC to C++.
		If the return statement argument is true, the expression in the body of loop gets a "return" as a prefix.
		The generated C++ code is returned.
	*/
	std::string convert_for(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		bool return_statement = false,
		std::string prefix = "");

	/*
		Function to convert a while statement and the loop body from RVC to C++.
		Parameters:
			1. token reference of the start of the for statement
			2. corresponding token container to the token
			3. Map (symbol,value) for the global and actor scope
			4. Map (Symbol, Value) for the local scope
			5. boolean return statement: true: the expression in the loop body should be a return statement
			6. prefix for each generated line of code
		The function converts the loop from RVC to C++ code.
		Both maps are used for type conversions from RVC to C++.
		If the return statement argument is true, the expression in the body of loop gets a "return" as a prefix.
		The generated C++ code is returned.
	*/
	std::string convert_while(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		bool return_statement = false,
		std::string prefix = "");

	/*
		Function to convert a basic expression like operations and assignments or definitions or declarations
		that are not covered by other functions to C++ code.
		Parameters:
			1. Token reference 
			2. the corresponding token container
			3. Map (symbol,value) for the actor and the global scope
			4. Prefix for each generated line of code
		All definitions and declarations are inserted into the global map.
		The generated C++ code is returned in string object.
	*/
	std::string convert_expression(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::string prefix = ""); 

	/*
		Function to convert a basic expression like operations and assignments or definitions or declarations
		that are not covered by other functions to C++ code.
		Parameters:
			1. Token reference
			2. the corresponding token container
			3. Map (symbol,value) for the actor and the global scope
			4. Map (symbol,value) for the local scope
			5. Symbol 
			6. Return statement: true: result of the expression should be returned, false: no return
			7. Prefix for each generated line of code
		All definitions and declarations are inserted into the global map and C++ code is generated for them if the symbol is equal to the 
		name of the variable that is declared or defined or the symbol input is a *.
		If return statement is true, the expression gets "return" as a prefix.
		The generated C++ code is returned in string object.
	*/
	std::string convert_expression(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::string symbol = "*",
		bool return_statement = false,
		std::string prefix = ""); 

	/*
		Function to convert a list definition or declaration to C++ code.
		Parameters:
			1. token reference containing "list"
			2. the corresponding token container to the token
			3. Map (symbol,value) for the global and actor scope
			4. prefix for each generated line of code
		The function converts the RVC code to C++ Code and inserts the name of the list to the global map.
		The list is converted to a c++ array. Assignments are converted to c++ list assignments.
		For the size calculations the symbol value mapping of the map is used.
		List comprehension is converted to loops in a new scope to avoid name collisions by tmp. vars..
		The generated code is returned as a string.
	*/
	std::string convert_list(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::string prefix = "");
	
	/*
		Function to convert a list definition or declaration to C++ code.
		Parameters:
			1. token reference containing "list"
			2. the corresponding token container to the token
			3. Map (symbol,value) for the global and actor scope
			4. Map (symbol,value) for the local scope
			5. symbol name (default *)
			6. prefix for each generated line of code
		The function converts the RVC code to C++ Code and inserts the name of the list to the local map if the name of the list is 
		equal to the symbol or the symbol is equal to *. Otherwise no c++ code is generated and an empty string is returned. 
		The list is converted to a c++ array. Assignments are converted to c++ list assignments.
		For the size calculations the symbol value mapping of both maps is used.
		List comprehension is converted to loops in a new scope to avoid name collisions by tmp. vars..
		The generated code is returned as a string.
	*/
	std::string convert_list(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::string symbol = "*",
		std::string prefix = ""); 

	/*
		Function to convert a RVC type to a C++ type.
		Parameters:
			1. Token reference pointing at the start of the type declaration
			2. the corresponding token container to the token
			3. Map (symbol,value) for the actor/global scope
		The function determines the size of the RVC type and translates it to a corresponding C++ type.
		Constants in the size definition are translated to numbers with the help of the map.
		This type is returned in a string.
	*/
	std::string convert_type(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map);

	/*
		Function to convert a RVC type to a C++ type.
		Parameters:
			1. Token reference pointing at the start of the type declaration
			2. the corresponding token container to the token
			3. Map (symbol,value) for the actor/global scope
			4. Map (symbol,value) for the local scope
		The function determines the size of the RVC type and translates it to a corresponding C++ type.
		Constants in the size definition are translated to numbers with the help of the maps.
		This type is returned in a string.
	*/
	std::string convert_type(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map);
	
	/*
		Function return a variable name that is not present in one of the maps.
		Parameters:
			1. Map (symbol,value) for the global/actor scope
			2. Map (Symbol, value) for the local scope
		Function returns an unused name. This is necessary for temporary variables that are used to convert RVC to C++ Code to avoid
		name collisions. 
	*/
	std::string find_unused_name(
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map);

	/*
		Function to skip a native function or procedure declaration.
		Parameters:
			1. Token containing @native
			2. the corresponding token container to the token
			3. symbol name
			4. boolean reference: native flag
			5. A map (symbol,value) of the actor/global scope (optional)
		The function checks if the name of the native procedure/function is equal to the symbol or if the symbol is a *.
		If yes, the native flag is set to true and the name of the function/procedure is inserted into the global map (if given).
	*/
	std::string convert_native_declaration(
		Token& t,
		Token_Container& token_producer,
		std::string symbol,
		Actor_Conversion_Data& actor_conversion_data);
	std::string convert_native_declaration(
		Token& t,
		Token_Container& token_producer,
		std::string symbol,
		Actor_Conversion_Data& actor_conversion_data,
		std::map<std::string, std::string>& global_map);

	/*
		Function to an expression in a string. Returns an int with the result.
		Parameters:
			1. The expression to evaluate in a string
			2. Map (symbol,value) for the global/actor acope
			3. Map (symbol,value) for the local scope

	*/
	int evaluate_constant_expression(
		std::string expression,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map);

	/*
		Function to get bracket expression. Returns the expression as string and a boolean if it contains a list comprehension as a pair.
		Paramters:
			1. Token reference to the start of the bracket.
			2. the corresponding token producer
			3. is_list is true if it is a r value, false otherwise
			4. Map (symbol,value) for global/actor scope
			5. Map (symbol,value) for local scope
			6. prefix for each line of code

	*/
	std::pair<std::string, bool> convert_brackets(
		Token& t,
		Token_Container& token_producer,
		bool is_list,//is_list => rvalue 
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::string prefix = "");

	/*
		Function to convert a if statement that is inline in an expression.
		Parameters:
			1. Reference to the first token of the if statement
			2. the corresponding token container
		Returns the expression in C++ syntax in a string and a bool if it contains a list assignment.(true if yes)
	*/
	std::pair<std::string, bool> convert_inline_if(
		Token& t,
		Token_Container& token_producer);

	/*
		Function to convert an inline if expression with a list assignment to C++ code.
		Parameters:
			1. String containing the expression
			2. Map (Symbol,Value) for the global/actor scope
			3. Map (Symbol,Value) for the local scope
			4. Prefix for each line 
			5. Symbol the values are assigned to
	*/
	std::string convert_inline_if_with_list_assignment(
		std::string string_to_convert,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& local_map,
		std::string prefix,
		std::string outer_expression);

	/*
	 * Function that converts the parameters to local variables and
	 * adds them to the global symbol map of the actor and the name-type map of the actor. 
	 */
	std::string convert_actor_parameters(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& global_map,
		std::map<std::string, std::string>& name_type_map,
		std::map<std::string, std::string>& default_value_map,
		std::string prefix = "");

}