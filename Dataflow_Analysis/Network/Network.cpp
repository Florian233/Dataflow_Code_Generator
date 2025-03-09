#include "Dataflow_Analysis/Dataflow_Analysis.hpp"
#include "Config/config.h"
#include "rapidxml-1.13/rapidxml.hpp"
using namespace rapidxml;

static bool is_real_fork(
	IR::Actor_Instance* inst)
{
	std::vector<std::vector<IR::Actor_Instance*>> recognized_successors;

	for (auto a = inst->get_actor()->get_actions().begin();
		a != inst->get_actor()->get_actions().end(); ++a)
	{
		std::vector<IR::Actor_Instance*> s;
		for (auto p = (*a)->get_out_buffers().begin(); p != (*a)->get_out_buffers().end(); ++p) {
			if (p->tokenrate == 0) {
				continue;
			}
			auto suc = find_out_edge(inst, p->buffer_name);
			if (suc != nullptr) {
				s.push_back(suc->get_sink());
			}
		}
		recognized_successors.push_back(s);
	}

	for (unsigned i = 1; i < recognized_successors.size(); ++i) {
		if (!recognized_successors.at(i-1).empty() && !recognized_successors.at(i).empty() &&
			!std::equal(recognized_successors.at(i - 1).begin(), recognized_successors.at(i - 1).end(), recognized_successors.at(i).begin())) {
			return true;
		}
	}

	return false;
}

static bool is_real_join(
	IR::Actor_Instance* inst)
{
	std::vector<std::vector<IR::Actor_Instance*>> recognized_predecessors;

	for (auto a = inst->get_actor()->get_actions().begin();
		a != inst->get_actor()->get_actions().end(); ++a)
	{
		std::vector<IR::Actor_Instance*> s;
		for (auto p = (*a)->get_in_buffers().begin(); p != (*a)->get_in_buffers().end(); ++p) {
			if (p->tokenrate == 0) {
				continue;
			}
			auto pre = find_in_edge(inst, p->buffer_name);
			if (pre != nullptr) {
				s.push_back(pre->get_source());
			}
		}
		recognized_predecessors.push_back(s);
	}

	for (unsigned i = 1; i < recognized_predecessors.size(); ++i) {
		if (!recognized_predecessors.at(i - 1).empty() && !recognized_predecessors.at(i).empty() &&
			!std::equal(recognized_predecessors.at(i - 1).begin(), recognized_predecessors.at(i - 1).end(),
						recognized_predecessors.at(i).begin()))
		{
			return true;
		}
	}

	return false;
}

static void read_input_nodes(
	IR::Dataflow_Network* dpn,
	std::string path)
{
	std::set<std::string> instance_names;
	xml_document<char>* doc = new xml_document<char>;
	{
		std::ifstream network_file(path, std::ifstream::in);
		if (network_file.fail()) {
			throw Converter_Exception{ "Cannot open the file " + path };
		}
		std::stringstream Top_network_buffer;
		Top_network_buffer << network_file.rdbuf();
		std::string str_to_parse = Top_network_buffer.str();
		char* buffer = new char[str_to_parse.size() + 1];
		std::size_t length = str_to_parse.copy(buffer, str_to_parse.size() + 1);
		buffer[length] = '\0';
		doc->parse<0>(buffer);
	}

	if (strcmp(doc->first_node()->name(), "Mapping") != 0) {
		// something is wrong here, root node should be mapping ... bail out
		throw Converter_Exception{ "Content of Input Nodes File erroneous.\n" };
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
							throw Converter_Exception{ "Content of Input Nodes File erroneous.\n" };
						}
					}
				}
			}
		}
		else {
			throw Converter_Exception{ "Content of Input Nodes File erroneous.\n" };
		}
	}

	for (auto actor_it = dpn->get_actor_instances().begin();
		actor_it != dpn->get_actor_instances().end(); ++actor_it)
	{
		if (instance_names.contains((*actor_it)->get_name())) {
			dpn->add_input(*actor_it);
		}
	}
}

static void detect_inputs(IR::Dataflow_Network* dpn) {
	for (auto it = dpn->get_actor_instances().begin(); it != dpn->get_actor_instances().end(); ++it) {
		if ((*it)->get_in_edges().empty()) {
			dpn->add_input(*it);
#ifdef DEBUG_DATAFLOW_ANALYSIS
			std::cout << "Detected " << (*it)->get_name() << " as source." << std::endl;
#endif
		}
	}
}

void read_output_nodes(
	IR::Dataflow_Network* dpn,
	std::string path)
{
	std::set<std::string> instance_names;
	xml_document<char>* doc = new xml_document<char>;
	{
		std::ifstream network_file(path, std::ifstream::in);
		if (network_file.fail()) {
			throw Converter_Exception{ "Cannot open the file " + path };
		}
		std::stringstream Top_network_buffer;
		Top_network_buffer << network_file.rdbuf();
		std::string str_to_parse = Top_network_buffer.str();
		char* buffer = new char[str_to_parse.size() + 1];
		std::size_t length = str_to_parse.copy(buffer, str_to_parse.size() + 1);
		buffer[length] = '\0';
		doc->parse<0>(buffer);
	}

	if (strcmp(doc->first_node()->name(), "Mapping") != 0) {
		// something is wrong here, root node should be mapping ... bail out
		throw Converter_Exception{ "Content of Output Nodes File erroneous.\n" };
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
							throw Converter_Exception{ "Content of Output Node File erroneous.\n" };
						}
					}
				}
			}
		}
		else {
			throw Converter_Exception{ "Content of Output Node File erroneous.\n" };
		}
	}

	for (auto actor_it = dpn->get_actor_instances().begin();
		actor_it != dpn->get_actor_instances().end(); ++actor_it)
	{
		if (instance_names.contains((*actor_it)->get_name())) {
			dpn->add_output(*actor_it);
		}
	}
}

static void detect_outputs(IR::Dataflow_Network* dpn) {
	for (auto it = dpn->get_actor_instances().begin(); it != dpn->get_actor_instances().end(); ++it) {
		if ((*it)->get_out_edges().empty()) {
			dpn->add_output(*it);
#ifdef DEBUG_DATAFLOW_ANALYSIS
			std::cout << "Detected " << (*it)->get_name() << " as sink." << std::endl;
#endif
		}
	}
}

static void detect_feedback_loops(IR::Dataflow_Network* dpn) {
	std::vector<IR::Edge*> process_list;

	for (auto it = dpn->get_inputs().begin(); it != dpn->get_inputs().end(); ++it) {
		for (auto o = (*it)->get_out_edges().begin(); o != (*it)->get_out_edges().end(); ++o) {
			process_list.push_back(*o);
		}
	}

	/* Iterate through the list and determine feedback and predecessors. */
	while (!process_list.empty()) {
		auto x = process_list.begin();
		IR::Edge* cur = *x;
		process_list.erase(x);
		IR::Actor_Instance* src = cur->get_source();
		IR::Actor_Instance* sink = cur->get_sink();

		if (src->is_predecessor(sink)) {
			/* successor is also predecessor, must be feedback */
			cur->set_feedback();
#ifdef DEBUG_DATAFLOW_ANALYSIS
			std::cout << "Detected feedback loop: " << cur->get_name() << std::endl;
#endif
		}
		else {

			for (auto it = src->get_predecessors().begin();
				it != src->get_predecessors().end(); ++it)
			{
				sink->add_predecessor(*it);
			}
			sink->add_predecessor(src);

			for (auto out = sink->get_out_edges().begin(); out != sink->get_out_edges().end(); ++out) {
				process_list.push_back(*out);
			}
		}
	}
}

void Dataflow_Analysis::network_analysis(IR::Dataflow_Network* dpn) {

#ifdef DEBUG_DATAFLOW_ANALYSIS
	std::cout << "Detecting input and output nodes." << std::endl;
#endif
	Config* c = c->getInstance();

	if (c->get_use_inputs_from_file()) {
		read_input_nodes(dpn, c->get_input_nodes_file());
	}
	else {
		detect_inputs(dpn);
	}
	if (c->get_use_outputs_from_file()) {
		read_output_nodes(dpn, c->get_output_nodes_file());
	}
	else {
		detect_outputs(dpn);
	}

#ifdef DEBUG_DATAFLOW_ANALYSIS
	std::cout << "Searching for feedback loops" << std::endl;
#endif

	detect_feedback_loops(dpn);

#ifdef DEBUG_DATAFLOW_ANALYSIS
	std::cout << "Searching for forks and joins" << std::endl;
#endif

	/* Determine fork and join for actor instances. */
	for (auto it = dpn->get_actor_instances().begin(); it != dpn->get_actor_instances().end(); ++it) {
		if (is_real_fork(*it)) {
			(*it)->set_fork();
#ifdef DEBUG_DATAFLOW_ANALYSIS
			std::cout << "Detected fork actor instance: " << (*it)->get_name() << std::endl;
#endif
		}
		if (is_real_join(*it)) {
			(*it)->set_join();
#ifdef DEBUG_DATAFLOW_ANALYSIS
			std::cout << "Detected join actor instance: " << (*it)->get_name() << std::endl;
#endif
		}
	}
}