#include "Mapping_Lib.hpp"
#include "rapidxml-1.13/rapidxml.hpp"
#include <string>
#include <cstring>
using namespace rapidxml;

static unsigned weight_for_instance(
	IR::Actor_Instance* instance)
{
	std::map<std::string, unsigned> in;
	std::map<std::string, unsigned> out;
	unsigned weight = 0;

	IR::Actor* actor = instance->get_actor();

	for (auto action_it = actor->get_actions().begin();
		action_it != actor->get_actions().end(); ++action_it)
	{
		for (auto in_it = (*action_it)->get_in_buffers().begin();
			in_it != (*action_it)->get_in_buffers().end(); ++in_it)
		{
			if (in.contains(in_it->buffer_name)) {
				if (in[in_it->buffer_name] < in_it->tokenrate) {
					in[in_it->buffer_name] = in_it->tokenrate;
				}
			}
			else {
				in[in_it->buffer_name] = in_it->tokenrate;
			}
		}

		for (auto out_it = (*action_it)->get_out_buffers().begin();
			out_it != (*action_it)->get_out_buffers().end(); ++out_it)
		{
			if (out.contains(out_it->buffer_name)) {
				if (out[out_it->buffer_name] < out_it->tokenrate) {
					out[out_it->buffer_name] = out_it->tokenrate;
				}
			}
			else {
				out[out_it->buffer_name] = out_it->tokenrate;
			}
		}
	}

	for (auto it = in.begin(); it != in.end(); ++it) {
		weight += it->second;
	}
	for (auto it = out.begin(); it != out.end(); ++it) {
		weight += it->second;
	}

	return weight;
}

static void find_predecessors(
	IR::Dataflow_Network* dpn,
	IR::Actor_Instance* inst,
	std::set<IR::Actor_Instance*>& predecessors)
{
	for (auto it = inst->get_in_edges().begin();
		it != inst->get_in_edges().end(); ++it)
	{
		predecessors.insert((*it)->get_source());
	}
}

static void find_sucessors(
	IR::Dataflow_Network* dpn,
	IR::Actor_Instance* inst,
	std::set<IR::Actor_Instance*>& sucessors)
{
	for (auto it = inst->get_out_edges().begin();
		it != inst->get_out_edges().end(); ++it)
	{
		sucessors.insert((*it)->get_sink());
	}
}

unsigned Mapping::compute_actor_weights(
	IR::Dataflow_Network* dpn,
	bool use_weights,
	std::map<IR::Actor_Instance*, unsigned>& actor_weight_map)
{
	unsigned overall = 0;
	for (auto inst_it = dpn->get_actor_instances().begin();
		inst_it != dpn->get_actor_instances().end(); ++inst_it)
	{
		if ((*inst_it)->is_deleted()) {
			continue;
		}
		unsigned w = 1;
		if (use_weights) {
			w = weight_for_instance((*inst_it));
		}
		actor_weight_map[*inst_it] = w;
		overall += w;

	}
	return overall;
}

unsigned Mapping::read_actor_weights(
	IR::Dataflow_Network* dpn,
	std::string path,
	std::map<IR::Actor_Instance*, unsigned>& actor_weight_map)
{
	std::map<std::string, unsigned> name_weight_map;
	xml_document<char>* doc = new xml_document<char>;
	{
		std::ifstream network_file(path, std::ifstream::in);
		if (network_file.bad()) {
			throw Converter_Exception{ "Cannot open the file " + path };
		}
		std::stringstream Top_network_buffer;
		Top_network_buffer << network_file.rdbuf();
		std::string str_to_parse = Top_network_buffer.str();
		char* buffer = new char[str_to_parse.size() + 1];
        std::size_t length = str_to_parse.copy(buffer,str_to_parse.size() + 1);
        buffer[length]='\0';
		doc->parse<0>(buffer);
	}

	if (strcmp(doc->first_node()->name(), "Mapping") != 0) {
		// something is wrong here, root node should be mapping ... bail out
		throw Converter_Exception{ "Content of Mapping file erroneous.\n" };
	}

	for (const rapidxml::xml_node<>* sub_node = doc->first_node()->first_node();
		sub_node; sub_node = sub_node->next_sibling())
	{
		if (strcmp(sub_node->name(), "Weights") == 0) {
			for (auto sub_sub_node = sub_node->first_node();
				sub_sub_node; sub_sub_node = sub_sub_node->next_sibling())
			{
				if (strcmp(sub_sub_node->name(), "Weight") == 0) {
					std::string name;
					unsigned weight;
					bool name_found = false;
					bool weight_found = false;
					for (auto attributes = sub_sub_node->first_attribute();
						attributes; attributes = attributes->next_attribute())
					{
						if (strcmp(attributes->name(), "name") == 0) {
							name = attributes->value();
							name_found = true;
						}
						else if (strcmp(attributes->name(), "Value") == 0) {
							weight = std::stoul(attributes->value(), nullptr, 10);
							weight_found = true;
						}
						else {
							throw Converter_Exception{ "Content of Mapping file erroneous.\n" };
						}
					}
					if (!weight_found || !name_found) {
						throw Converter_Exception{ "Content of Mapping file erroneous.\n" };
					}
					name_weight_map[name] = weight;
				}
			}
		}
		else {
			throw Converter_Exception{ "Content of Mapping file erroneous.\n" };
		}
	}

	unsigned overall = 0;

	for (auto actor_it = dpn->get_actor_instances().begin();
		actor_it != dpn->get_actor_instances().end(); ++actor_it)
	{
		unsigned w = 0;
		if (!name_weight_map.contains((*actor_it)->get_name())) {
			std::cout << "Mapping Weights file doesn't contain: " << (*actor_it)->get_name() << " defaulting to autodetection." << std::endl;
			w = weight_for_instance(*actor_it);
			actor_weight_map[*actor_it] = w;
		}
		else {
			w = name_weight_map[(*actor_it)->get_name()];
			actor_weight_map[*actor_it] = w;
		}
		overall += w;
	}

	return overall;
}

static unsigned get_actor_instance_weight(
	IR::Actor_Instance* inst,
	bool use_weights,
	std::map<IR::Actor_Instance*, unsigned>& actor_weight_map)
{
	if (use_weights) {
		return actor_weight_map[inst];
	}
	else {
		return 1;
	}
}

unsigned Mapping::compute_levels_alap(
	IR::Dataflow_Network* dpn,
	bool use_weights,
	std::map<IR::Actor_Instance*, unsigned>& actor_weight_map,
	std::set<IR::Actor_Instance*>& output_nodes,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map)
{
	unsigned level = 0;
	std::set<IR::Actor_Instance*> working_set;

	for (auto it = output_nodes.begin(); it != output_nodes.end(); ++it) {
		actor_level_map[*it] = 0;
		working_set.insert(*it);
	}

	while (!working_set.empty()) {
		IR::Actor_Instance* inst = *working_set.begin();
		working_set.erase(working_set.begin());

		std::set<IR::Actor_Instance*> predecessors;
		find_predecessors(dpn, inst, predecessors);

		unsigned inst_weight = get_actor_instance_weight(inst, use_weights, actor_weight_map);

		for (auto pre = predecessors.begin(); pre != predecessors.end(); ++pre) {
			unsigned max = inst_weight + actor_level_map[inst];
			if (!actor_level_map.contains(*pre)) {
				actor_level_map[*pre] = max;
				working_set.insert(*pre);
				if (max > level) {
					level = max;
				}
			}
			else {
				if (actor_level_map[*pre] < max) {
					actor_level_map[*pre] = max;
					working_set.insert(*pre);
					if (max > level) {
						level = max;
					}
				}
			}
		}
	}
	return level;
}

void Mapping::detect_output_nodes(
	IR::Dataflow_Network* dpn,
	std::set<IR::Actor_Instance*>& output_nodes)
{
	for (auto inst_it = dpn->get_actor_instances().begin();
		inst_it != dpn->get_actor_instances().end(); ++inst_it)
	{
		if ((*inst_it)->is_deleted()) {
			continue;
		}
		if ((*inst_it)->get_actor()->get_out_buffers().empty()) {
			output_nodes.insert(*inst_it);
		}
	}
}

void Mapping::read_output_nodes(
	IR::Dataflow_Network* dpn,
	std::string path,
	std::set<IR::Actor_Instance*>& output_nodes)
{
	std::set<std::string> instance_names;
	xml_document<char>* doc = new xml_document<char>;
	{
		std::ifstream network_file(path, std::ifstream::in);
		if (network_file.bad()) {
			throw Converter_Exception{ "Cannot open the file " + path };
		}
		std::stringstream Top_network_buffer;
		Top_network_buffer << network_file.rdbuf();
		std::string str_to_parse = Top_network_buffer.str();
		char* buffer = new char[str_to_parse.size() + 1];
        std::size_t length = str_to_parse.copy(buffer,str_to_parse.size() + 1);
        buffer[length]='\0';
		doc->parse<0>(buffer);
	}

	if (strcmp(doc->first_node()->name(), "Mapping") != 0) {
		// something is wrong here, root node should be mapping ... bail out
		throw Converter_Exception{ "Content of Mapping file erroneous.\n" };
	}

	for (const rapidxml::xml_node<>* sub_node = doc->first_node()->first_node();
		sub_node; sub_node = sub_node->next_sibling())
	{
		if (strcmp(sub_node->name(), "Output") == 0) {
			for (auto sub_sub_node = sub_node->first_node();
				sub_sub_node; sub_sub_node = sub_sub_node->next_sibling())
			{
				if (strcmp(sub_sub_node->name(), "Node") == 0) {
					for (auto attributes = sub_sub_node->first_attribute();
						attributes; attributes = attributes->next_attribute())
					{
						if (strcmp(attributes->name(), "name") == 0) {
							instance_names.insert(attributes->value());
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
	}

	for (auto actor_it = dpn->get_actor_instances().begin();
		actor_it != dpn->get_actor_instances().end(); ++actor_it)
	{
		if (instance_names.contains((*actor_it)->get_name())) {
			output_nodes.insert(*actor_it);
		}
	}
}

unsigned Mapping::compute_levels_asap(
	IR::Dataflow_Network* dpn,
	bool use_weights,
	std::map<IR::Actor_Instance*, unsigned>& actor_weight_map,
	std::set<IR::Actor_Instance*>& input_nodes,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map)
{
	unsigned level = 0;
	std::set<IR::Actor_Instance*> working_set;

	for (auto it = input_nodes.begin(); it != input_nodes.end(); ++it) {
		actor_level_map[*it] = 0;
		working_set.insert(*it);
	}

	while (!working_set.empty()) {
		IR::Actor_Instance* inst = *working_set.begin();
		working_set.erase(working_set.begin());

		std::set<IR::Actor_Instance*> sucessors;
		find_sucessors(dpn, inst, sucessors);

		unsigned inst_weight = get_actor_instance_weight(inst, use_weights, actor_weight_map);

		for (auto pre = sucessors.begin(); pre != sucessors.end(); ++pre) {
			unsigned min = inst_weight + actor_level_map[inst];
			if (!actor_level_map.contains(*pre)) {
				actor_level_map[*pre] = min;
				working_set.insert(*pre);
				if (min > level) {
					level = min;
				}
			}
			else {
				if (actor_level_map[*pre] > min) {
					actor_level_map[*pre] = min;
					working_set.insert(*pre);
				}
			}
		}
	}
	return level;
}

void Mapping::detect_input_nodes(
	IR::Dataflow_Network* dpn,
	std::set<IR::Actor_Instance*>& input_nodes)
{
	for (auto inst_it = dpn->get_actor_instances().begin();
		inst_it != dpn->get_actor_instances().end(); ++inst_it)
	{
		if ((*inst_it)->is_deleted()) {
			continue;
		}
		if ((*inst_it)->get_actor()->get_in_buffers().empty()) {
			input_nodes.insert(*inst_it);
		}
	}
}

void Mapping::read_input_nodes(
	IR::Dataflow_Network* dpn,
	std::string path,
	std::set<IR::Actor_Instance*>& input_nodes)
{
	std::set<std::string> instance_names;
	xml_document<char>* doc = new xml_document<char>;
	{
		std::ifstream network_file(path, std::ifstream::in);
		if (network_file.bad()) {
			throw Converter_Exception{ "Cannot open the file " + path };
		}
		std::stringstream Top_network_buffer;
		Top_network_buffer << network_file.rdbuf();
		std::string str_to_parse = Top_network_buffer.str();
		char* buffer = new char[str_to_parse.size() + 1];
        std::size_t length = str_to_parse.copy(buffer,str_to_parse.size() + 1);
        buffer[length]='\0';
		doc->parse<0>(buffer);
	}

	if (strcmp(doc->first_node()->name(), "Mapping") != 0) {
		// something is wrong here, root node should be mapping ... bail out
		throw Converter_Exception{ "Content of Mapping file erroneous.\n" };
	}

	for (const rapidxml::xml_node<>* sub_node = doc->first_node()->first_node();
		sub_node; sub_node = sub_node->next_sibling())
	{
		if (strcmp(sub_node->name(), "Input") == 0) {
			for (auto sub_sub_node = sub_node->first_node();
				sub_sub_node; sub_sub_node = sub_sub_node->next_sibling())
			{
				if (strcmp(sub_sub_node->name(), "Node") == 0) {
					for (auto attributes = sub_sub_node->first_attribute();
						attributes; attributes = attributes->next_attribute())
					{
						if (strcmp(attributes->name(), "name") == 0) {
							instance_names.insert(attributes->value());
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
	}

	for (auto actor_it = dpn->get_actor_instances().begin();
		actor_it != dpn->get_actor_instances().end(); ++actor_it)
	{
		if (instance_names.contains((*actor_it)->get_name())) {
			input_nodes.insert(*actor_it);
		}
	}
}

struct Path {
	std::vector<IR::Actor_Instance*> current_path;
	std::set<IR::Actor_Instance*> next;
	unsigned weight;
};

static bool node_in_path(
	std::vector<IR::Actor_Instance*>& path,
	IR::Actor_Instance* inst)
{
	for (auto it = path.begin(); it != path.end(); ++it) {
		if (*it == inst) {
			return true;
		}
	}

	return false;
}

/* Maybe we should remove backward edges first or ignore them. Currently it simply takes the longest path. 
 * DP might be more efficient, but this also works for now.
 */
unsigned Mapping::compute_critical_path(
	IR::Dataflow_Network* dpn,
	bool use_weights,
	std::map<IR::Actor_Instance*, unsigned>& actor_weight_map,
	std::set<IR::Actor_Instance*>& output_nodes,
	std::vector<IR::Actor_Instance*>& critical_path)
{
	std::set<Path*> paths;
	unsigned num_active_paths = 0;

	for (auto it = output_nodes.begin(); it != output_nodes.end(); ++it) {
		Path* p = (Path*)malloc(sizeof(Path));
		if (p == NULL) {
			throw Converter_Exception{ "Allocation failed." };
		}
		p->current_path.push_back(*it);
		p->weight = get_actor_instance_weight(*it, use_weights, actor_weight_map);
		find_predecessors(dpn, *it, p->next);
		++num_active_paths;
	}

	while (num_active_paths > 0) {
		Path* p = *paths.begin();
		paths.erase(paths.begin());

		--num_active_paths;

		for (auto it = p->next.begin(); it != p->next.end(); ++it) {
			if (node_in_path(p->current_path, *it)) {
				continue;
			}

			++num_active_paths;
			Path* n = (Path*)malloc(sizeof(Path));
			if (n == NULL) {
				throw Converter_Exception{ "Allocation failed." };
			}
			n->weight = p->weight + get_actor_instance_weight(*it, use_weights, actor_weight_map);
			n->current_path.assign(p->current_path.begin(), p->current_path.end());
			n->current_path.push_back(*it);
			for (auto next = (*it)->get_in_edges().begin();
				next != (*it)->get_in_edges().end(); ++next)
			{
				n->next.insert((*next)->get_source());
			}
		}
		free(p);
	}

	unsigned longest = 0;
	Path* path = NULL;

	for (auto it = paths.begin(); it != paths.end(); ++it) {
		if ((*it)->weight > longest) {
			longest = (*it)->weight;
			path = *it;
		}
	}

	if (path == NULL) {
		return 0;
	}

	critical_path.assign(path->current_path.begin(), path->current_path.end());

	return longest;
}

void Mapping::compute_slack(
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map_lft,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map_est,
	unsigned max_level_est,
	std::map<IR::Actor_Instance*, unsigned>& actor_weight_map,
	std::map<IR::Actor_Instance*, unsigned>& slack_map)
{
	for (auto it = actor_level_map_lft.begin(); it != actor_level_map_lft.end(); ++it) {
		unsigned slack = 0;

		/* Add actor weight because EST is earliest start and LFT is latested finish 
		 * and use max_level_est because EST starts with 0 at input and LFT starts with zero as output 
		 */
		slack = max_level_est - actor_level_map_est[it->first] + actor_weight_map[it->first] - actor_level_map_lft[it->first];

		slack_map[it->first] = slack;
	}
}