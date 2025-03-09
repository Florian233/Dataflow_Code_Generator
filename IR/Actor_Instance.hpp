#pragma once

#include "Actor.hpp"
#include "Composit_Actor.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
#include "Conversion/Actor_Conversion_Data.hpp"

namespace IR {
	class Edge;

	/* Information about an actor instance */
	class Actor_Instance {

		IR::Actor* actor;
		std::string name;
		std::map<std::string, std::string> parameters;

		bool deleted{ false };

		/* Mapping of this actor instance of the core that shall execute this instance. */
		unsigned mapping{ 111 };

		/* Scheduling order of this node for the given mapping, this might be the result of the mapping ops */
		/* -1 indicated that no scheduling order is set, sched_order numbers can be sorted but need not be unique nor contiguous. */
		int sched_order = -1;

		/* Might be used during compositactor creation to cluster the instances. */
		unsigned cluster_id{ 0 };
		/* Pointer valid if is is part of a composit actor, otherwise nullptr. */
		IR::Composit_Actor *composit;

		std::vector<Edge*> in_edges;
		std::vector<Edge*> out_edges;

		// actions of actor that shall not be part of the code for this instance
		std::set<std::string> deleted_actions; 

		/* If only this instance has been modified, e.g. by removing an connected channel, it requires a 
		 * different conversion_data object.
		 * Contains information relevant during and for conversion but not for the IR.
		 */
		bool use_instance_data{ false };
		Actor_Conversion_Data conversion_data;

		bool critical_path = false;
		unsigned parallel_path_id = 0;

		std::set<IR::Actor_Instance*> predecessors;

		bool fork{ false };
		bool join{ false };

		unsigned id;

		unsigned sched_loop_bound = 0;

	public:

		Actor_Instance(std::string name, unsigned _id) : name(name), id(_id) {
			actor = nullptr;
			composit = nullptr;
		}

		Actor_Instance(std::string name, IR::Actor* a, unsigned _id) : name(name), actor(a), id(_id) {
			composit = nullptr;
		}

		unsigned get_id(void) {
			return id;
		}

		void set_actor(IR::Actor* a) {
			actor = a;
		}

		void add_parameter(std::string k, std::string v) {
			parameters[k] = v;
		}

		std::string get_name(void) {
			return name;
		}

		std::map<std::string, std::string>& get_parameters(void) {
			return parameters;
		}

		IR::Actor* get_actor(void) {
			return actor;
		}

		void set_deleted(void) {
			deleted = true;
		}

		bool is_deleted(void) {
			return deleted;
		}

		void set_mapping(unsigned c) {
			mapping = c;
		}

		unsigned get_mapping(void) {
			return mapping;
		}

		void set_cluster_id(unsigned id) {
			cluster_id = id;
		}

		unsigned get_cluster_id(void) {
			return cluster_id;
		}

		void add_in_edge(IR::Edge* e) {
			in_edges.push_back(e);
		}

		void add_out_edge(IR::Edge* e) {
			out_edges.push_back(e);
		}

		std::vector<IR::Edge*>& get_out_edges(void) {
			return out_edges;
		}

		std::vector<IR::Edge*>& get_in_edges(void) {
			return in_edges;
		}

		IR::Composit_Actor* get_composit_actor(void) {
			return composit;
		}

		void set_composit_actor(IR::Composit_Actor* a) {
			composit = a;
		}

		void add_deleted_action(std::string a) {
			deleted_actions.insert(a);
		}

		std::set<std::string>& get_deleted_actions(void) {
			return deleted_actions;
		}

		bool is_port(std::string name) {
			for (auto it = actor->get_in_buffers().begin();
				it != actor->get_in_buffers().end(); ++it)
			{
				if (it->buffer_name == name) {
					return true;
				}
			}

			for (auto it = actor->get_out_buffers().begin();
				it != actor->get_out_buffers().end(); ++it)
			{
				if (it->buffer_name == name) {
					return true;
				}
			}

			return false;
		}

		Actor_Conversion_Data& get_conversion_data(void) {
			if (use_instance_data) {
				return conversion_data;
			}
			else {
				return actor->get_conversion_data();
			}
		}

		Actor_Conversion_Data* get_conversion_data_ptr(void) {
			if (use_instance_data) {
				return &conversion_data;
			}
			else {
				return actor->get_conversion_data_ptr();
			}
		}

		void set_critical_path(bool b) {
			critical_path = b;
		}
		bool get_critical_path(void) {
			return critical_path;
		}

		void set_parallel_path_id(unsigned i) {
			parallel_path_id = i;
		}
		unsigned get_parallel_path_id(void) {
			return parallel_path_id;
		}

		void add_predecessor(IR::Actor_Instance* inst) {
			predecessors.insert(inst);
		}

		bool is_predecessor(IR::Actor_Instance* inst) {
			return predecessors.contains(inst);
		}

		std::set<IR::Actor_Instance*>& get_predecessors(void) {
			return predecessors;
		}

		bool is_fork(void) {
			return fork;
		}

		bool is_join(void) {
			return join ;
		}

		void set_fork(void) {
			fork = true;
		}

		void set_join(void) {
			join = true;
		}

		void update_conversion_data(void) {
			conversion_data = actor->get_conversion_data();
			use_instance_data = true;
		}

		int get_sched_order(void) {
			return sched_order;
		}

		void set_sched_order(int s) {
			sched_order = s;
		}

		void set_sched_loop_bound(unsigned b) {
			sched_loop_bound = b;
		}
		unsigned get_sched_loop_bound(void) {
			return sched_loop_bound;
		}
	};
};