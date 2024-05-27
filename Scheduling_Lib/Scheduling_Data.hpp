#pragma once

#include <vector>
#include <string>

namespace Scheduling {
	/* Scheduling data that is created for each channel access of the actions of the actor instance. */
	typedef struct {
		/* Type of this channel. */
		std::string type;
		/* Flag if the generated parameter is a pointer, this is the case when more than one element is consumed/produced. */
		bool is_pointer;
		/* Number of elements that are consumed/produced from/for this channel (size of var_name * repeat_val = elements). */
		unsigned elements;
		/* Flag if the access has a repeat CAL keyword. */
		bool repeat;
		/* Flag whether the action has parameters for this channel. */
		bool parameter_generated;
		std::string channel_name;
		bool in; // if false must be out, I don't add another value for out
		// Variable names required for guard evaluation by the scheduler
		std::vector<std::string> var_names;
		bool unused_channel;
		// parameters for the function call, this is inserted by the scheduler generator itself if used
		std::string parameters;
	} Channel_Schedule_Data;
}