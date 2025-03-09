#pragma once

#include "IR/Dataflow_Network.hpp"
#include "Exceptions.hpp"
#include <filesystem>

namespace Network_Reader {

	/* Function to read the XDF files and produce a flat network representation. */
	IR::Dataflow_Network* read_network(void);

	/* Function to read the code of the actors and store them as part of their representation
	 * in the flat network representation.
	 */
	void read_actors(IR::Dataflow_Network* dpn);

	IR::Unit* read_unit(IR::Dataflow_Network* dpn, std::filesystem::path path);

	/* Exception that is thrown when during reading something fails, usually file access, e.g.
	 * referenced files don't exist.
	 */
	class Network_Reader_Exception : public Converter_Exception {
	public:
		Network_Reader_Exception(std::string _str) : Converter_Exception{ _str } {};
	};

}