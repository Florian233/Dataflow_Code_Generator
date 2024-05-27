#pragma once

#include <string>
#include <vector>
#include <map>
#include "IR/Action.hpp"
#include "Scheduling_Lib/Scheduling_Data.hpp"

/* Object to store data that is relevant during and for the conversion of an actor and its instances.
 * Under certain conditions actor instances might also be linked to a valid conversion data object!
 */
class Actor_Conversion_Data {
	using value = std::string;
	using symbol = std::string;
	using type = std::string;
private:
	/* Buffer already converted code for the code generation step, conversion might be necessary before
	 * e.g. to determine token rates depending on const values.
	 */
	std::string var_code;
	std::string include_code;
	std::string constructor_code;
	std::string local_scheduler_code;
	std::string scheduling_condition;
	std::string method_code;
	std::string action_code;
	/* Order of the parameters passed to constructor, order preserving of vector is necessary here! */
	std::vector<std::string> parameter_order;
	std::map<symbol, value> actor_symbol_map;
	std::map<symbol, type> symbol_type_map;
	bool is_parallelizable{ false };
	std::string type_code;
	std::vector<std::string> native_functions;
	std::string declarations;

	std::string class_name;

	std::map<std::string, std::string> default_parameters;

	/* Flag whether the actor has an initialization action. */
	bool init{ false };

	std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data>> local_scheduler_data_map;
	std::map<std::string, std::string> channel_name_type_map;

	std::map<std::string, std::string> replacement_map;

	std::vector<std::string> class_variables;

public:
	Actor_Conversion_Data() {
		//declare build in functions in the map
		actor_symbol_map["print"] = "function";
		actor_symbol_map["println"] = "function";
		actor_symbol_map["min"] = "function";
		actor_symbol_map["max"] = "function";
	}

	void add_var_code(std::string s) {
		var_code.append(s);
	}

	void set_is_parallelizable(bool b) {
		is_parallelizable = b;
	}

	bool get_is_parallelizable(void) {
		return is_parallelizable;
	}

	std::map<symbol, value>& get_symbol_map(void) {
		return actor_symbol_map;
	}

	void add_native_function(std::string s) {
		native_functions.push_back(s);
	}

	void add_declarations(std::string s) {
		declarations.append(s);
	}

	std::vector<std::string>& get_parameter_order(void) {
		return parameter_order;
	}

	void add_parameter(std::string p) {
		parameter_order.push_back(p);
	}

	void set_class_name(std::string c) {
		class_name = c;
	}

	std::string get_class_name(void) {
		return class_name;
	}

	void add_default_parameter(std::string p, std::string v) {
		default_parameters[p] = v;
	}

	std::map<std::string, std::string>& get_default_parameter_map(void) {
		return default_parameters;
	}

	std::string get_var_code(void) {
		return var_code;
	}

	std::string get_declarations_code(void) {
		return declarations;
	}

	void set_scheduling_condition(std::string s) {
		scheduling_condition = s;
	}

	std::string get_scheduling_condition(void) {
		return scheduling_condition;
	}

	void set_init(void) {
		init = true;
	}

	bool has_init(void) {
		return init;
	}

	void init_action_scheduler_data(
		std::string name)
	{
		if (!local_scheduler_data_map.contains(name)) {
			local_scheduler_data_map.emplace(name, std::vector<Scheduling::Channel_Schedule_Data>());

		}
	}

	void add_scheduler_data(
		std::string function_name,
		Scheduling::Channel_Schedule_Data data)
	{
		if (!local_scheduler_data_map.contains(function_name)) {
			local_scheduler_data_map.emplace(function_name, std::vector<Scheduling::Channel_Schedule_Data>());

		}
		local_scheduler_data_map[function_name].push_back(data);
	}

	std::map<std::string, std::vector< Scheduling::Channel_Schedule_Data > >& get_scheduling_data(void) {
		return local_scheduler_data_map;
	}

	void add_constructor_code(std::string s) {
		constructor_code.append(s);
	}

	std::string get_constructor_code(void) {
		return constructor_code;
	}

	void add_channel(std::string name, std::string type) {
		channel_name_type_map[name] = type;
	}

	std::map<std::string, std::string>& get_channel_name_type_map(void) {
		return channel_name_type_map;
	}

	std::vector<std::string>& get_class_variables(void) {
		return class_variables;
	}

	void add_class_variable(std::string v) {
		class_variables.push_back(v);
	}

	void add_symbol_type(std::string symbol, std::string type) {
		symbol_type_map[symbol] = type;
	}

	std::string get_symbol_type(std::string symbol) {
		if (symbol_type_map.contains(symbol)) {
			return symbol_type_map[symbol];
		}
		else {
			return "";
		}
	}

	std::map<std::string, std::string>& get_symbol_type_map(void) {
		return symbol_type_map;
	}

	void add_replacement(std::string old_var, std::string new_var) {
		replacement_map[old_var] = new_var;
	}

	std::string get_replacement(std::string var) {
		if (replacement_map.contains(var)) {
			return replacement_map[var];
		}
		return "";
	}

	std::map<std::string, std::string>& get_replacement_map(void) {
		return replacement_map;
	}

};