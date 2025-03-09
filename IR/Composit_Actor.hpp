#pragma once

#include <string>
#include "Conversion/Actor_Conversion_Data.hpp"

namespace IR {

	/* Information about a composit actor. This is for future use and has be created during optimization. */
	class Composit_Actor {
		std::string name;
		Actor_Conversion_Data conversion_data;

		unsigned sched_loop_bound;

	public:

		Composit_Actor(std::string n) : name{ n } {};

		std::string get_name(void) {
			return name;
		}

		Actor_Conversion_Data& get_conversion_data(void) {
			return conversion_data;
		}

		Actor_Conversion_Data* get_conversion_data_ptr(void) {
			return &conversion_data;
		}

		void set_sched_loop_bound(unsigned b) {
			sched_loop_bound = b;
		}
		unsigned get_sched_loop_bound(void) {
			return sched_loop_bound;
		}
	};
}