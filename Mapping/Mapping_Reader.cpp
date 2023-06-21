#include "Mapping/Mapping.hpp"
#include "Exceptions.hpp"
#include <map>
#include <sstream> 
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include "rapidxml-1.13/rapidxml.hpp"
#include <algorithm>
#include <set>
#include "Config/config.h"
using namespace rapidxml;

void Mapping::read_mapping(
	IR::Dataflow_Network* dpn,
	Mapping_Data* data,
	std::string path)
{
	std::map<std::string, unsigned> instance_core_map;
	xml_document<char>* doc = new xml_document<char>;
	Config* c = c->getInstance();

	{
		std::ifstream network_file(path, std::ifstream::in);
		if (network_file.bad()) {
			throw Converter_Exception{ "Cannot open the file " + path };
		}
		std::stringstream Top_network_buffer;
		Top_network_buffer << network_file.rdbuf();
		std::string str_to_parse = Top_network_buffer.str();
		char* buffer = new char[str_to_parse.size() + 1];
		strcpy_s(buffer, str_to_parse.size() + 1, str_to_parse.c_str());
		doc->parse<0>(buffer);
	}

	if (strcmp(doc->first_node()->name(), "Mapping") != 0) {
		// something is wrong here, root node should be mapping ... bail out
		throw Converter_Exception{ "Content of Mapping file erroneous.\n" };
	}

	unsigned core = 0;
	for (const rapidxml::xml_node<>* sub_node = doc->first_node()->first_node();
		sub_node; sub_node = sub_node->next_sibling())
	{
		if (strcmp(sub_node->name(), "Cluster") == 0) {
			for (auto sub_sub_node = sub_node->first_node();
				sub_sub_node; sub_sub_node = sub_sub_node->next_sibling())
			{
				if (strcmp(sub_sub_node->name(), "Node") == 0) {
					for (auto attributes = sub_sub_node->first_attribute();
						attributes; attributes = attributes->next_attribute())
					{
						if (strcmp(attributes->name(), "name") == 0) {
							instance_core_map[attributes->value()] = core;
						}
						else {
							throw Converter_Exception{ "Content of Mapping file erroneous.\n" };
						}
					}
				}
			}
		}
		else {
			throw Converter_Exception{ "Content of Mapping file erroneous.\n" };
		}

		++core;
	}

	if (c->get_cores() < core) {
		std::cout << "Warning: Mapping file uses more cores than specified. Increasing number of cores." << std::endl;
		c->set_cores(core);
	}
	else if (c->get_cores() > core) {
		std::cout << "Warning: Mapping file uses less cores than specified. Decreasing number of cores." << std::endl;
		c->set_cores(core);
	}

	// Assume here that the mapping doesn't map the same instance to multiple cores
	// Maybe another attribute for the mapping node could be introduced to change this
	// but this not needed at the moment.
	data->actor_sharing = false;

	for (auto actor_it = dpn->get_actor_instances().begin();
		actor_it != dpn->get_actor_instances().end(); ++actor_it)
	{
		if (!instance_core_map.contains((*actor_it)->get_name())) {
			std::cout << "Mapping file doesn't contain: " << (*actor_it)->get_name() << " defaulting to core zero." << std::endl;
			(*actor_it)->set_mapping(0);
		}
		else {
			(*actor_it)->set_mapping(instance_core_map[(*actor_it)->get_name()]);
		}
	}
}