#include "Reader/Reader.hpp"
#include "Config/config.h"

IR::Unit* Network_Reader::read_unit(
	IR::Dataflow_Network* dpn,
	std::filesystem::path path)
{
	Config* c = c->getInstance();
	std::filesystem::path full_path{ c->get_source_dir() };
	full_path /= path;

	if (dpn->get_unit(full_path.string()) != nullptr) {
		return dpn->get_unit(full_path.string());
	}
	std::ifstream file(full_path, std::ifstream::in);
	if (file.fail()) {
		throw Network_Reader::Network_Reader_Exception{ "Cannot open the file " + full_path.string() };
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string code = buffer.str();

	IR::Unit* u = new IR::Unit{ code };
	return u;
}
