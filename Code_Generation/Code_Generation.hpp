#pragma once

#include "IR/Dataflow_Network.hpp"
#include "Optimization_Phase1/Optimization_Phase1.hpp"
#include "Optimization_Phase2/Optimization_Phase2.hpp"
#include "Mapping/Mapping.hpp"
#include "Exceptions.hpp"
#include <set>

namespace Code_Generation {
	using Header = std::string;
	using Source = std::string;

	/* Entry point of the code generation. */
	void generate_code(
		IR::Dataflow_Network* dpn,
		Optimization::Optimization_Data_Phase1* opt_data1,
		Optimization::Optimization_Data_Phase2* opt_data2,
		Mapping::Mapping_Data* map_data);

	class Code_Generation_Exception : public Converter_Exception {
	public:
		Code_Generation_Exception(std::string _str) : Converter_Exception{ _str } {};
	};
}