#include "Dataflow_Analysis/Dataflow_Analysis.hpp"
#include "Config/config.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "rapidxml-1.13/rapidxml.hpp"
#include "common/include/String_Helper.h"
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
				s.push_back(dynamic_cast<IR::Actor_Instance*>(suc->get_sink()));
			}
		}
		recognized_successors.push_back(s);
	}

	for (unsigned i = 1; i < recognized_successors.size(); ++i) {
		if (!recognized_successors.at(i-1).empty() && !recognized_successors.at(i).empty() &&
			!std::equal(recognized_successors.at(i - 1).begin(), recognized_successors.at(i - 1).end(),
						recognized_successors.at(i).begin(), recognized_successors.at(i).end())) {
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
				s.push_back(dynamic_cast<IR::Actor_Instance*>(pre->get_source()));
			}
		}
		recognized_predecessors.push_back(s);
	}

	for (unsigned i = 1; i < recognized_predecessors.size(); ++i) {
		if (!recognized_predecessors.at(i - 1).empty() && !recognized_predecessors.at(i).empty() &&
			!std::equal(recognized_predecessors.at(i - 1).begin(), recognized_predecessors.at(i - 1).end(),
						recognized_predecessors.at(i).begin(), recognized_predecessors.at(i).end()))
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
			(*actor_it)->set_source();
			dpn->add_input(*actor_it);
		}
	}
}

static void detect_inputs(IR::Dataflow_Network* dpn) {
	for (auto it = dpn->get_actor_instances().begin(); it != dpn->get_actor_instances().end(); ++it) {
		if ((*it)->get_in_edges().empty()) {
			(*it)->set_source();
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
			(*actor_it)->set_sink();
			dpn->add_output(*actor_it);
		}
	}
}

static void detect_outputs(IR::Dataflow_Network* dpn) {
	for (auto it = dpn->get_actor_instances().begin(); it != dpn->get_actor_instances().end(); ++it) {
		if ((*it)->get_out_edges().empty()) {
			(*it)->set_sink();
			dpn->add_output(*it);
#ifdef DEBUG_DATAFLOW_ANALYSIS
			std::cout << "Detected " << (*it)->get_name() << " as sink." << std::endl;
#endif
		}
	}
}

void read_feedback_edges(
	IR::Dataflow_Network* dpn,
	std::string path)
{
	std::set<std::string> edge_names;
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
		throw Converter_Exception{ "Content of Feedback Edge File erroneous.\n" };
	}

	for (const rapidxml::xml_node<>* sub_node = doc->first_node()->first_node();
		sub_node; sub_node = sub_node->next_sibling())
	{
		if (strcmp(sub_node->name(), "Feedback") == 0) {
			for (auto sub_sub_node = sub_node->first_node();
				sub_sub_node; sub_sub_node = sub_sub_node->next_sibling())
			{
				if (strcmp(sub_sub_node->name(), "Edge") == 0) {
					for (auto attributes = sub_sub_node->first_attribute();
						attributes; attributes = attributes->next_attribute())
					{
						if (strcmp(attributes->name(), "name") == 0) {
							edge_names.insert(attributes->value());
						}
						else {
							throw Converter_Exception{ "Content of Feedback Edge File erroneous.\n" };
						}
					}
				}
			}
		}
		else {
			throw Converter_Exception{ "Content of Feedback Edge File erroneous.\n" };
		}
	}

	for (auto e : edge_names) {
		replace_all_substrings(e, ".", "_");
		auto edge = dpn->get_edge(e);
		if (edge != nullptr) {
			edge->set_feedback();
		}
		else {
			std::cout << "WARNING: Edge " << e << " specified as feedback edge but not found in the network definition." << std::endl;
		}
	}
}

static void detect_feedback_loops(IR::Dataflow_Network* dpn) {
	std::vector<IR::Edge*> process_list;

	std::map<IR::Actor_Instance_Base*, unsigned> level;

	for (auto it = dpn->get_outputs().begin(); it != dpn->get_outputs().end(); ++it) {
		for (auto o = (*it)->get_in_edges().begin(); o != (*it)->get_in_edges().end(); ++o) {
			process_list.push_back(*o);
		}
		level[*it] = 0;
	}

	while (!process_list.empty()) {
		auto x = process_list.begin();
		IR::Edge* cur = *x;
		process_list.erase(x);

		IR::Actor_Instance* src = dynamic_cast<IR::Actor_Instance*>(cur->get_source());
		IR::Actor_Instance* sink = dynamic_cast<IR::Actor_Instance*>(cur->get_sink());

		if (level.contains(src) && (level[src] > (level[sink] + 1))) {
			level[src] = level[sink] + 1;

			for (auto in = src->get_in_edges().begin(); in != src->get_in_edges().end(); ++in) {
				process_list.push_back(*in);
			}
		}
	}

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

		IR::Actor_Instance* src = dynamic_cast<IR::Actor_Instance*>(cur->get_source());
		IR::Actor_Instance* sink = dynamic_cast<IR::Actor_Instance*>(cur->get_sink());

		if (src->is_predecessor(sink)) {
			/* successor is also predecessor, must be feedback */
			cur->set_feedback();
#ifdef DEBUG_DATAFLOW_ANALYSIS
			std::cout << "Detected feedback loop: " << cur->get_name() << std::endl;
#endif
		}
		else {
			bool addition = false;
			for (auto it = src->get_predecessors().begin();
				it != src->get_predecessors().end(); ++it)
			{
				addition |= sink->add_predecessor(*it);
			}
			addition |= sink->add_predecessor(src);

			if (addition) {
				for (auto out = sink->get_out_edges().begin(); out != sink->get_out_edges().end(); ++out) {
					process_list.push_back(*out);
				}
			}
		}
	}
}


static void read_scheduling_loop_bounds(
	IR::Dataflow_Network* dpn)
{
	Config* c = Config::getInstance();

	for (auto a = dpn->get_actor_instances().begin(); a != dpn->get_actor_instances().end(); ++a) {
		(*a)->set_sched_loop_bound(c->get_local_sched_loop_num());
	}

	if (c->get_bound_sched_loops_file().empty()) {
		return;
	}

	rapidxml::xml_document<char>* doc = new rapidxml::xml_document<char>;

	std::ifstream network_file(c->get_bound_sched_loops_file(), std::ifstream::in);
	if (network_file.fail()) {
		throw Converter_Exception{ "Cannot open the file " + c->get_bound_sched_loops_file() };
	}
	std::stringstream sched_loop_buffer;
	sched_loop_buffer << network_file.rdbuf();
	std::string str_to_parse = sched_loop_buffer.str();
	char* buffer = new char[str_to_parse.size() + 1];
	std::size_t length = str_to_parse.copy(buffer, str_to_parse.size() + 1);
	buffer[length] = '\0';
	doc->parse<0>(buffer);

	if (strcmp(doc->first_node()->name(), "Loopbound") != 0) {
		// something is wrong here, root node should be mapping ... bail out
		throw Converter_Exception{ "Content of Loop Bound file erroneous." };
	}

	for (const rapidxml::xml_node<>* sub_node = doc->first_node()->first_node();
		sub_node; sub_node = sub_node->next_sibling())
	{
		if (strcmp(sub_node->name(), "Bound") == 0) {
			std::string inst;
			std::string bound;
			for (auto attributes = sub_node->first_attribute();
				attributes; attributes = attributes->next_attribute())
			{
				if (strcmp(attributes->name(), "name") == 0) {
					inst = attributes->value();
				}
				else if (strcmp(attributes->name(), "value") == 0) {
					bound = attributes->value();
				}
				else {
					throw Converter_Exception{ "Content of Loopbound file erroneous." };
				}
			}
			IR::Actor_Instance* i = dpn->get_actor_instance(inst);
			if (i == nullptr) {
				throw Converter_Exception{ "Content of Loopbound file erroneous, actor instance " + inst + " doesn't exit." };
			}
			try {
				i->set_sched_loop_bound(std::stoul(bound));
			}
			catch (const std::exception&) {
				throw Converter_Exception{ "Loop bound value \"" + bound + "\" for instance " + inst + " is not a valid unsigned integer." };
			}
		}
		else {
			throw Converter_Exception{ "Content of Loopbound file erroneous." };
		}
	}
}

void Dataflow_Analysis::network_analysis(IR::Dataflow_Network* dpn) {

#ifdef DEBUG_DATAFLOW_ANALYSIS
	std::cout << "Detecting input and output nodes." << std::endl;
#endif
	Config* c = Config::getInstance();

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
	std::cout << "Reading scheduling loop bounds" << std::endl;
#endif

	if (c->get_bound_local_sched_loops()) {
		read_scheduling_loop_bounds(dpn);
	}

#ifdef DEBUG_DATAFLOW_ANALYSIS
	std::cout << "Searching for feedback loops" << std::endl;
#endif

	if (c->get_feedback_edges_file().empty()) {
		detect_feedback_loops(dpn);
	} else {
		read_feedback_edges(dpn, c->get_feedback_edges_file());
	}

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

	/* check if ports are used in the network definition that don't exist */
	for (auto it = dpn->get_actor_instances().begin(); it != dpn->get_actor_instances().end(); ++it) {
		for (auto in = (*it)->get_in_edges().begin(); in != (*it)->get_in_edges().end(); ++in) {
			std::string portname = (*in)->get_dst_port();
			bool found = false;
			for (auto port : (*it)->get_ast()->actor->inports) {
				if (port->name.name == portname) {
					found = true;
				}
			}
			if (!found) {
				throw Converter_Exception{ "Inport " + portname + " of instance " + (*it)->get_name() + " does not exist."};
			}
		}
		for (auto out = (*it)->get_out_edges().begin(); out != (*it)->get_out_edges().end(); ++out) {
			std::string portname = (*out)->get_src_port();
			bool found = false;
			for (auto port : (*it)->get_ast()->actor->outports) {
				if (port->name.name == portname) {
					found = true;
				}
			}
			if (!found) {
				throw Converter_Exception{ "Outport " + portname + " of instance " + (*it)->get_name() + " does not exist." };
			}
		}
	}
}