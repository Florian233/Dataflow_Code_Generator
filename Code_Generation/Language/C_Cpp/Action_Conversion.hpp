#pragma once

#include "Tokenizer/Tokenizer.hpp"
#include <map>
#include <string>
#include "Conversion/Actor_Conversion_Data.hpp"
#include "Tokenizer/Action_Buffer.hpp"
#include "IR/Action.hpp"
#include <set>

std::string convert_action(
	IR::Action* action,
	Action_Buffer* token_producer,
	Actor_Conversion_Data& actor_data,
	/* Set to true if the input channels shall not be read directly,
	   instead parameters are added to the generated function.
	 */
	bool input_channel_parameters,
	/* Set to true if the output channels shall not be written directly,
       instead parameters are added to the generated function.
     */
	bool output_channel_parameters,
	std::set<std::string> unused_in_channels,
	std::set<std::string> unused_out_channels,
	std::string prefix = "");
