#pragma once

#include <string>
#include "Conversion/Actor_Conversion_Data.hpp"

namespace IR {

	/* Information about a composit actor. This is for future use and has be created during optimization. */
	class Composit_Actor {
		std::string name;
		Actor_Conversion_Data conversion_data;

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
	};
}