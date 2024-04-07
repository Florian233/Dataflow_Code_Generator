#include "Code_Generation.hpp"
#include "Config/config.h"
#include <set>

/* Differences of the actor instance from the base actor.
 * Differences might stem from unconnected ports or DCE.
 */
class Actor_Diff_Data {
public:
	std::set<std::string> unused_actions;
	std::set<std::string> unused_in_channels;
	std::set<std::string> unused_out_channels;
	std::string name;
	IR::Actor_Instance* actor;

	Actor_Diff_Data(
		std::set<std::string>& a,
		std::set<std::string>& in,
		std::set<std::string>& out,
		std::string n,
		IR::Actor_Instance *ai) :
		unused_actions{ a }, unused_in_channels{ in }, unused_out_channels{ out },
		name{ n }, actor{ ai }
	{};

};

/* Find all ports of an actor instance that are unconnected,
 * either for input ports (in == true) or output ports (in == false)
 */
static std::set<std::string> get_unconnected_ports(IR::Actor_Instance *instance, bool in) {
	std::set<std::string> result;

	if (in) {
		for (auto it = instance->get_actor()->get_in_buffers().begin();
			it != instance->get_actor()->get_in_buffers().end(); ++it)
		{
			bool found{ false };
			for (auto eit = instance->get_in_edges().begin(); eit != instance->get_in_edges().end(); ++eit) {
				if ((*eit)->get_dst_port() == it->buffer_name) {
					found = true;
					break;
				}
			}
			if (!found) {
				result.insert(it->buffer_name);
			}
		}
	}
	else {
		for (auto it = instance->get_actor()->get_out_buffers().begin();
			it != instance->get_actor()->get_out_buffers().end(); ++it)
		{
			bool found{ false };
			for (auto eit = instance->get_out_edges().begin(); eit != instance->get_out_edges().end(); ++eit) {
				if ((*eit)->get_dst_port() == it->buffer_name) {
					found = true;
					break;
				}
			}
			if (!found) {
				result.insert(it->buffer_name);
			}
		}
	}

	return result;
}

/* Generate a simple base class all other actors inherit from.
 * This allows storing all actors in one list.
 */
static void generate_base_class(void) {
	std::string code =
		"#pragma once\n\n"
		"class Actor {\n"
		"public:\n"
		"\tvirtual void init(void) = 0;\n"
		"\tvirtual void schedule(void) = 0;\n"
		"};";

	Config* c = c->getInstance();
	std::string path{ c->get_target_dir() };

	std::ofstream output_file{ path + "/Actor.hpp" };
	if (output_file.bad()) {
		throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path + "/Actor.hpp" };
	}
	output_file << code;
	output_file.close();
}

void Code_Generation::generate_code(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data)
{
	Config* c = c->getInstance();

	std::string include_code;

	if (c->get_orcc_compat()) {
		generate_ORCC_compatibility_layer(c->get_target_dir());
		include_code.append("#include \"options.h\"\n");
	}

	generate_base_class();

	generate_channel_code(opt_data1, opt_data2, map_data);
	include_code.append("#include \"Channel.hpp\"\n");

	/*
	Actor instances of the same actor might not be equivalent after removing dead code etc.
	Hence, different classes need to be created for them.
	-> List of generated normal actors without any modification
	-> List of generated instances with their modification and check whether such an instance already exists
	   => This leads to a name issue, maybe perform check first and if all instances have the same modificaton
	      name it after the base actor class, like when removing the control/clock channels
	   => How to monitor modifications of instances: map actor + removed in channels + removed out channels to name (also not connected channels)
	   => Is deleted actions also an issue or is it the logical consequence from the removed channels or guards that are the same for all instances?
	   => How to name in other cases?

	   map: actor class name -> list of variants
	*/
	// Not used for composit actors as they are all tailored to the corresponding setup anyhow
	std::map<std::string, std::vector<Actor_Diff_Data>> actor_variants_map;

	for (auto it = dpn->get_actor_instances().begin();
		it != dpn->get_actor_instances().end(); ++it)
	{
		if ((*it)->get_composit_actor() == nullptr) {

			IR::Actor* a = (*it)->get_actor();

			std::set<std::string> unused_actions;
			std::set<std::string> unused_in_channels;
			std::set<std::string> unused_out_channels;

			for (auto edge = (*it)->get_in_edges().begin();
				edge != (*it)->get_in_edges().end(); ++edge)
			{
				if ((*edge)->is_deleted()) {
					unused_in_channels.insert((*edge)->get_dst_port());
				}
			}

			for (auto edge = (*it)->get_out_edges().begin();
				edge != (*it)->get_out_edges().end(); ++edge)
			{
				if ((*edge)->is_deleted()) {
					unused_out_channels.insert((*edge)->get_src_port());
				}
			}

			for (auto action = a->get_actions().begin();
				action != a->get_actions().end(); ++action)
			{
				if ((*action)->is_deleted()) {
					unused_actions.insert((*action)->get_name());
				}
			}

			if (c->get_prune_disconnected()) {
				std::set<std::string> in_uncon = get_unconnected_ports(*it, true);
				std::set<std::string> out_uncon = get_unconnected_ports(*it, false);

#ifdef DEBUG_ACTOR_GENERATION
				if (!in_uncon.empty()) {
					std::cout << "Unconnect in ports: ";
					for (auto in = in_uncon.begin(); in != in_uncon.end(); ++in) {
						if (in != in_uncon.begin()) {
							std::cout << ", ";
						}
						std::cout << *in;
					}
					std::cout << std::endl;
				}
				if (!out_uncon.empty()) {
					std::cout << "Unconnect out ports: ";
					for (auto out = out_uncon.begin(); out != out_uncon.end(); ++out) {
						if (out != out_uncon.begin()) {
							std::cout << ", ";
						}
						std::cout << *out;
					}
					std::cout << std::endl;
				}

				unused_in_channels.insert(in_uncon.begin(), in_uncon.end());
				unused_out_channels.insert(out_uncon.begin(), out_uncon.end());
#endif
			}

			Actor_Conversion_Data& d = a->get_conversion_data();
			std::string name = a->get_class_name();
			if (name.find_last_of('.') != std::string::npos) {
				name = name.substr(name.find_last_of('.') + 1);
			}
			name[0] = std::toupper(name[0]);

			if (actor_variants_map.contains(name)) {
				// Figure out whether we already created this kind of actor variation
				bool found{ false };
				for (auto known_it = actor_variants_map[name].begin();
					known_it != actor_variants_map[name].end(); ++known_it)
				{
					if (known_it->unused_actions == unused_actions
						&& known_it->unused_in_channels == unused_in_channels
						&& known_it->unused_out_channels == unused_out_channels)
					{
						found = true;
					}
				}

				if (!found) {
					auto tmp = Actor_Diff_Data(unused_actions, unused_in_channels, unused_out_channels, name, *it);
					actor_variants_map[name].push_back(tmp);
				}
			}
			else {
				auto tmp = Actor_Diff_Data(unused_actions, unused_in_channels, unused_out_channels, name, *it);
				std::vector<Actor_Diff_Data> v;
				v.push_back(tmp);
				actor_variants_map.insert({name, v});
			}
		}
	}

	// Go through all variants and determine their names as the least changed variant shall keep the
	// original name
	for (auto it = actor_variants_map.begin(); it != actor_variants_map.end(); ++it) {
		size_t change_counter = 999999; //everthing we find should be less than this :D
		for (auto variant = it->second.begin(); variant != it->second.end(); ++variant) {
			size_t this_change = variant->unused_actions.size()
				+ variant->unused_in_channels.size()
				+ variant->unused_out_channels.size();
			if (this_change < change_counter) {
				change_counter = this_change;
			}
		}

		bool default_set{ false }; //track if we used the original actor name already ;-)

		for (auto variant = it->second.begin(); variant != it->second.end(); ++variant) {
			size_t this_change = variant->unused_actions.size()
				+ variant->unused_in_channels.size()
				+ variant->unused_out_channels.size();

			if ((this_change == change_counter) && !default_set) {
				variant->name = it->first;
				default_set = true;
			}
			else {
				variant->name = variant->actor->get_name();
				variant->actor->update_conversion_data();
			}
		}
	}

	//Finally start with the code generation.
	for (auto it = actor_variants_map.begin(); it != actor_variants_map.end(); ++it) {
		for (auto variant = it->second.begin(); variant != it->second.end(); ++variant) {
			variant->actor->get_conversion_data().set_class_name(variant->name);
			std::string i = generate_actor_code(variant->actor, variant->name, variant->unused_actions,
				variant->unused_in_channels, variant->unused_out_channels, opt_data1, opt_data2, map_data);
			include_code.append(i);
		}
	}

	for (auto it = dpn->get_composit_actors().begin();
		it != dpn->get_composit_actors().end(); ++it)
	{
		std::string i = generate_composit_actor_code(*it, opt_data1, opt_data2, map_data);
		include_code.append(i);
	}

	generate_core(dpn, opt_data1, opt_data2, map_data, include_code);

	if (c->get_cmake()) {
		std::string path = c->get_target_dir();
		std::string source_files = "\"main.cpp\"";
		if (c->get_orcc_compat()) {
			source_files.append(" \"orcc_compatibility.cpp\"");
		}
		std::string network_name = dpn->get_name();

		generate_cmake_file(network_name, source_files, path);
	}
}