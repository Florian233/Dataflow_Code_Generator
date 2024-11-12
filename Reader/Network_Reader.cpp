#include "Reader.hpp"
#include <sstream> 
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include "rapidxml-1.13/rapidxml.hpp"
#include <algorithm>
#include <set>
#include "Config/config.h"
#include <filesystem>
using namespace rapidxml;

//set for all network instance ids
static std::set<std::string> network_instance_ids;

/*
	Function tests if a file specified by the input parameter exists.
	If it exists it returns true. Otherwise it returns false.
*/
static inline bool file_exists(std::filesystem::path f)
{
	std::ifstream stream(f);
	return stream.good();
}

/*
	The function tests if the file path_string.xdf exists.
	If yes it return true, otherwise it returns false.
*/
static inline bool network_file(std::filesystem::path path_string) {
	path_string += ".xdf";
	if (file_exists(path_string)) {
		return true;
	}
	return false;
}

//Deklaration due to a circular dependency between start_paring and parse_network
static void start_parsing(
	std::string& start,
	const char* path,
	IR::Dataflow_Network* dpn,
	std::string prefix = "",
	bool top = true);

/*
	Function that parses a complete network. The starting node is the network_node input parameter.
	The input parameter prefix is used as a prefix for all actor ids to distinguish between the same actors used in different network instances.
	For every connection a connection object is created and stored in the given dpn.
	For each instance the function checks if it is a instance of another network (.xdf) or a actor (.cal). The actors are parsed recursively and the actors are stored in the dpn object.
	Additionally, the input parameters for the actors are stored in the dpn and inserts all network instance ids to the network_instance_ids set.
*/
static void parse_network(
	const rapidxml::xml_node<>* network_node,
	const char* path,
	IR::Dataflow_Network* dpn,
	std::string prefix,
	bool top)
{
	std::map<std::string, std::string> decl_parameter_map;
	std::set<std::string> network_instances;
	Config* c = c->getInstance();

	//Read the name of the network first
	if (top) {
		for (auto attributes = network_node->first_attribute();
			attributes; attributes = attributes->next_attribute())
		{
			if (strcmp(attributes->name(), "name") == 0) {
				dpn->set_name(attributes->value());
				std::cout << "Network name is: " << attributes->value() << std::endl;
			}
		}
	}

	for (const rapidxml::xml_node<>* sub_node = network_node->first_node();
		sub_node; sub_node = sub_node->next_sibling())
	{
		if (strcmp(sub_node->name(), "Decl") == 0) {
			//find name
			std::string decl_name;
			for (auto attributes = sub_node->first_attribute();
				attributes; attributes = attributes->next_attribute())
			{
				if (strcmp(attributes->name(), "name") == 0) {
					decl_name = attributes->value();
				}
			}
			//find value
			std::string value;
			for (auto sub_node_decl = sub_node->first_node();
				sub_node_decl; sub_node_decl = sub_node_decl->next_sibling())
			{
				if (strcmp(sub_node_decl->name(), "Expr") == 0) {
					bool string{ false };
					for (auto attributes = sub_node_decl->first_attribute();
						attributes; attributes = attributes->next_attribute())
					{
						if (strcmp(attributes->name(), "value") == 0) {
							if (string) {
								value.append("\"");
								value.append(attributes->value());
								value.append("\"");
							}
							else {
								value = attributes->value();
							}
						}
						else if ((strcmp(attributes->name(), "literal-kind") == 0)
							&& (strcmp(attributes->value(), "String") == 0))
						{
							string = true;
						}
					}
				}
			}
			decl_parameter_map[decl_name] = value;
		}

		if (strcmp(sub_node->name(), "Connection") == 0) {
			std::string src_port, dst_port, dst_id, src_id;
			bool dst_is_instance = false;
			bool src_is_instance = false;
			unsigned size = c->get_FIFO_size();

			for (const rapidxml::xml_attribute<>* connection_attribute =sub_node->first_attribute();
				connection_attribute; connection_attribute = connection_attribute->next_attribute())
			{
				if (strcmp(connection_attribute->name(), "dst") == 0) {
					std::string str{ connection_attribute->value() };
					if (str.size() == 0) {
						dst_id = prefix.substr(0, prefix.size() - 1) + str;
						dst_is_instance = true;
					}
					else {
						dst_id = prefix + str;
						if (network_instances.find(str) != network_instances.end()) {
							dst_is_instance = true;
						}
					}
				}
				if (strcmp(connection_attribute->name(), "dst-port") == 0) {
					dst_port = connection_attribute->value();
				}
				if (strcmp(connection_attribute->name(), "src") == 0) {
					std::string str{ connection_attribute->value() };
					if (str.size() == 0) {
						src_id = prefix.substr(0, prefix.size() - 1) + str;
						src_is_instance = true;
					}
					else {
						src_id = prefix + connection_attribute->value();
						if (network_instances.find(str) != network_instances.end()) {
							src_is_instance = true;
						}
					}
				}
				if (strcmp(connection_attribute->name(), "src-port") == 0) {
					src_port = connection_attribute->value();
				}
			}
			for (auto x = sub_node->first_node(); x; x = x->next_sibling()) {
				if (strcmp(x->name(), "Attribute") == 0) {
					for (auto s = x->first_node(); s; s = s->next_sibling()) {
						if (strcmp(s->name(), "Expr") == 0) {
							for (auto a = s->first_attribute(); a; a = a->next_attribute()) {
								if (strcmp(a->name(), "value") == 0) {
									size = static_cast<unsigned int>(std::atoi(a->value()));
								}
							}
						}
					}
				}
			}
			IR::Edge e(dst_id, dst_port, src_id, src_port);
			e.set_dst_network_instance_port(dst_is_instance);
			e.set_src_network_instance_port(src_is_instance);
			e.set_specified_size(size);
			dpn->add_edge(e);
		}
		if (strcmp(sub_node->name(), "Instance") == 0) {
			const rapidxml::xml_attribute<>* id_attribute = sub_node->first_attribute();
			for (const rapidxml::xml_node<>* instance_node = sub_node->first_node();
				instance_node; instance_node = instance_node->next_sibling())
			{
				if (strcmp(instance_node->name(), "Class") == 0) {
					const rapidxml::xml_attribute<>* a = instance_node->first_attribute();
					std::filesystem::path path_string{ path };
					std::string str{ a->value() };
					std::replace(str.begin(), str.end(), '.', static_cast<char>(std::filesystem::path::preferred_separator));
					path_string /= str;
					if (network_file(path_string)) {
						network_instances.insert(id_attribute->value());
						path_string += ".xdf";
						network_instance_ids.insert(prefix + id_attribute->value());
						std::string tmp = path_string.string();
						start_parsing(tmp, path, dpn, std::string{prefix + id_attribute->value()} + "_", false);
					}
					else {
						path_string += ".cal";
						dpn->add_actors_class_path(a->value(), path_string.string());
						dpn->add_id_class(prefix + id_attribute->value(), a->value());
					}

				}
				else if (strcmp(instance_node->name(), "Parameter") == 0) {
					const rapidxml::xml_attribute<>* parameter_name = instance_node->first_attribute();
					const rapidxml::xml_node<>* expr_node = instance_node->first_node();
					const rapidxml::xml_attribute<>* expr_attribute;
					bool literal_type_is_string{ false };
					std::string value;
					bool variable_decl_lookup{ true }; //is set to false if a literal-kind is found, true indicates that the parameter is a variable decl
					for (expr_attribute = expr_node->first_attribute(); expr_attribute;
						expr_attribute = expr_attribute->next_attribute())
					{
						if (strcmp(expr_attribute->name(), "value") == 0) {
							if (literal_type_is_string) {
								value.append("\"");
								value.append(expr_attribute->value());
								value.append("\"");
							}
							else {
								value = expr_attribute->value();
							}
						}
						else if (strcmp(expr_attribute->name(), "literal-kind") == 0) {
							variable_decl_lookup = false;
							if (strcmp(expr_attribute->value(), "String") == 0) {
								literal_type_is_string = true;
							}
						}
						else if ((strcmp(expr_attribute->name(), "kind") == 0)
							&& (strcmp(expr_attribute->value(), "UnaryOp") == 0))
						{
							for (auto param_sub_nodes = expr_node->first_node();
								param_sub_nodes; param_sub_nodes = param_sub_nodes->next_sibling())
							{
								if (strcmp(param_sub_nodes->name(), "Op") == 0) {//find operator and put it at the beginning of the value string
									for (auto op_attribute = param_sub_nodes->first_attribute();
										op_attribute; op_attribute = op_attribute->next_attribute())
									{
										if (strcmp(op_attribute->name(), "name") == 0) {
											value.insert(0, op_attribute->value());
										}
									}
								}
								else if (strcmp(param_sub_nodes->name(), "Expr") == 0) {//find value
									for (auto op_attribute = param_sub_nodes->first_attribute();
										op_attribute; op_attribute = op_attribute->next_attribute())
									{
										if (strcmp(op_attribute->name(), "value") == 0) {
											if (literal_type_is_string) {
												value.append("\"");
												value.append(op_attribute->value());
												value.append("\"");
											}
											else {
												value = op_attribute->value();
											}

										}
										else if (strcmp(op_attribute->name(), "literal-kind") == 0) {
											variable_decl_lookup = false;
											if (strcmp(op_attribute->value(), "String") == 0) {
												literal_type_is_string = true;
											}
										}
									}
								}
							}
						}
					}
					if (variable_decl_lookup) {
						value = decl_parameter_map[value];
					}
					dpn->add_parameter(prefix + id_attribute->value(), parameter_name->value(), value);
				}
			}
		}
	}
}
/*
	Open the file that is specified by the path given in start. The input parameter *path contains the path to the sources of the RVC network.
	Prefix contains the name of the instance, if this network is instanziated in another network.
	This function reads the content of the file and creates a xml parser object and calls parse_network to parse the network.

	This function throws an exception if it cannot open the file.
*/
static void start_parsing(
	std::string& start,
	const char* path,
	IR::Dataflow_Network* dpn,
	std::string prefix,
	bool top)
{
	std::ifstream network_file(start, std::ifstream::in);
	if (network_file.fail()) {
		throw Network_Reader::Network_Reader_Exception{ "Cannot open the file " + start };
	}
	std::stringstream Top_network_buffer;
	Top_network_buffer << network_file.rdbuf();
	std::string str_to_parse = Top_network_buffer.str();
	char* buffer = new char[str_to_parse.size() + 1];
	std::size_t length = str_to_parse.copy(buffer, str_to_parse.size() + 1);
	buffer[length] = '\0';
	xml_document<char> *doc = new xml_document<char>;
	doc->parse<0>(buffer);
	parse_network(doc->first_node(), path, dpn, prefix, top);
}
/*
	This function checks whether there are still ports of network instances left in the list of the connections.
*/
static bool are_network_instance_ports_present(std::vector<IR::Edge>& connections) {
	for (auto connection = connections.begin(); connection != connections.end(); ++connection) {
		if ((connection->get_dst_network_instance_port()) ||
			(connection->get_src_network_instance_port()))
		{
			return true;
		}
	}
	return false;
}

/*
	This function creates a vector with all connections that are between two actors or starting at an actor and going to a network port.
*/
static std::vector<IR::Edge> find_start_connections(IR::Dataflow_Network* dpn) {
	std::vector<IR::Edge> return_value;
	for (auto it = dpn->get_edges().begin(); it != dpn->get_edges().end(); ++it) {
		if (!it->get_dst_network_instance_port() && !it->get_src_network_instance_port()) {
			return_value.push_back(*it);
		}
		else if (it->get_dst_network_instance_port() && !it->get_src_network_instance_port()) {
			return_value.push_back(*it);
		}
	}
	return return_value;
}

/*
	This function connects connections that are going to a network port with another connection that are starting with the network port and combines these two to one connection.
	This connection is added to the output along with the connections between two actors.
*/
static std::vector<IR::Edge> connection_network_ports(
	IR::Dataflow_Network* dpn,
	std::vector<IR::Edge>& new_connections)
{
	std::vector<IR::Edge> return_value;
	for (auto it = new_connections.begin(); it != new_connections.end(); ++it) {
		if (it->get_dst_network_instance_port()) {
			for (auto connec_it = dpn->get_edges().begin();
				connec_it != dpn->get_edges().end(); ++connec_it)
			{
				if ((it->get_dst_id() == connec_it->get_src_id())
					&& (it->get_dst_port() == connec_it->get_src_port()))
				{
					std::string dst_id = connec_it->get_dst_id();
					std::string dst_port = connec_it->get_dst_port();
					bool dst_network_instance_port = connec_it->get_dst_network_instance_port();
					std::string src_id = it->get_src_id();
					std::string src_port = it->get_src_port();
					bool src_network_instance_port = it->get_src_network_instance_port();
					unsigned specified_size = std::max(it->get_specified_size(), connec_it->get_specified_size());
					IR::Edge e(dst_id, dst_port, src_id, src_port);
					e.set_dst_network_instance_port(dst_network_instance_port);
					e.set_src_network_instance_port(src_network_instance_port);
					e.set_specified_size(specified_size);
					return_value.push_back(e);
				}
			}
		}
		else {
			return_value.push_back(*it);
		}
	}
	return return_value;
}


/*
	This function parses the complete network and removes all network instances and replaces them by
	the containing actors and connections directly between two actors without network ports between them.
	Every remaining connection, actor id and the corresponding path the source file is inserted into a Dataflow_Network object.
*/
IR::Dataflow_Network* Network_Reader::read_network(void) {

	IR::Dataflow_Network* dpn = new IR::Dataflow_Network;
	Config* c = c->getInstance();
	std::string str{ c->get_network_file()};
	start_parsing(str, c->get_source_dir(), dpn);
	std::vector<IR::Edge> starting_point{ find_start_connections(dpn) };
	while (are_network_instance_ports_present(starting_point)) {
		starting_point = connection_network_ports(dpn, starting_point);

	}
	dpn->set_edges(starting_point);

#ifdef DEBUG_READER_PRINT_EDGES
	for (auto it = dpn->get_edges().begin(); it != dpn->get_edges().end(); ++it) {
		std::cout << "Edge: " << it->get_src_id() << "." << it->get_src_port() << " to "
			<< it->get_dst_id() << "." << it->get_dst_port() << std::endl;
	}
#endif

	return dpn;
};

