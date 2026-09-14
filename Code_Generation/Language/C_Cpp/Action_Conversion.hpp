#pragma once

#include <map>
#include <string>
#include "IR/Action.hpp"
#include <set>
#include "Dataflow_Analysis/Scheduling_Lib/Scheduling_Data.hpp"

std::string convert_action(
	IR::Action* action,
	/* Set to true if the input channels shall not be read directly,
	   instead parameters are added to the generated function.
	 */
	bool input_channel_parameters,
	std::set<std::string> unused_in_channels,
	std::set<std::string> unused_out_channels,
	std::string prefix,
	std::map<std::string, std::string> replacements,
	std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data>>& scheduledata,
	std::string class_name,
	std::map<std::string, std::string> port_type_map,
	std::map<std::string, std::string> const_map,
	std::string additional_code = "");
