#pragma once
#include <string>
#include "IR/AST/AST.hpp"
#include <map>

/*
	Namespace containing all functions needed to convert basic RVC expressions like functions, loops,...

	Convention: The token reference that is used throughout the whole conversion must point at a token that is the next token after the
				RVC construct that is converted in the respective function after the function completed. 
				At the start of a function the token reference must point at a token that is the start of the respective RVC construct.
*/
namespace Converter_RVC_Cpp {


	std::string convert_function(
		AST::Function* function,
		std::string prefix,
		std::map<std::string, std::string> replacements,
		std::map<std::string, std::string> const_map,
		std::string add_args = "");

	std::string convert_procedure(
		AST::Procedure* procedure,
		std::string prefix,
		std::map<std::string, std::string> replacements,
		std::map<std::string, std::string> const_map,
		std::string add_args = "");

	std::string convert_nativefunction(
		AST::NativeFunction* native,
		std::string prefix,
		std::map<std::string, std::string> const_map);

	std::string convert_nativeprocedure(
		AST::NativeProcedure* native,
		std::string prefix,
		std::map<std::string, std::string> const_map);

	std::string convert_expression(
		AST::Expression* expression,
		std::map<std::string, std::string> replacements,
		std::map<std::string, std::string> const_map);

	std::string convert_statement(
		AST::Statement* statement,
		std::string prefix,
		std::map<std::string, std::string> replacements,
		std::map<std::string, std::string> const_map);

	std::string convert_type(
		AST::Type* type,
		std::string name,
		std::map<std::string, std::string> const_map);

	std::pair<std::string, std::string> convert_vardef(
		AST::VarDefinition* vardef,
		std::string prefix,
		/* Don't initialize non-constant values directly, instead generate constructor code */
		bool noinit,
		std::map<std::string, std::string> replacements,
		std::map<std::string, std::string> const_map,
		bool rtos = false);

	std::pair<std::string, std::string> convert_actorparam(
		AST::ActorParameter* param,
		std::string prefix,
		std::map<std::string, std::string> const_map);

	/* first: initialization lists, second: code to initialize the variable, mutually exclusive */
	std::pair<std::string,std::string> convert_listcomprehension(
		AST::ListComprehension* outexpr,
		std::string prefix,
		std::string assign_var,
		std::map<std::string, std::string> replacements,
		std::map<std::string, std::string> const_map,
		bool constasgn = false);

	std::string unused_identifier(void);
}