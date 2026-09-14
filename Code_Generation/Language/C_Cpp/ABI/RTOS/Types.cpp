#include "Types.hpp"

std::string convert_rtos_type(std::string t)
{
	/* Actually we could convert more types to create clean code, but for the experiments this is sufficient. */
	if (t == "bool") {
		return "bool_t";
	}
	return t;
}