#pragma once

#include "IR/Actor_Instance.hpp"
#include "IR/Edge.hpp"
#include "IR/Actor.hpp"
#include "IR/Unit.hpp"
#include <vector>
#include <map>

namespace IR {

	/* Information read during the first conversion phase, the network reading.
	 * Later on only newly created composit actors might be added,
	 * other than that this object shouldn't change!
	 */
	class Dataflow_Network {
		std::vector<Actor_Instance*> actor_instances;
		std::vector<Edge> edges;

		/* Map actor to it's class path (path in the source directory) */
		std::map<std::string, std::string> actors_class_path;
		/* Map and actor instance to it's class path. */
		std::map<std::string, std::string> id_class_map;

		/* Map include paths to Unit objects to avoid loading them several times */
		std::map<std::string, IR::Unit*> path_unit_map;

		/* Tuple of (actor instance, parameter name, value) */
		std::vector< std::tuple<std::string, std::string, std::string> > parameters;

		std::vector<IR::Actor*> actors;

		std::vector<IR::Composit_Actor*> composits;

		std::string name;

		std::map<std::string, Actor_Instance*> name_instance_map;
		std::map<unsigned, Actor_Instance*> id_instance_map;

		std::vector<Actor_Instance*> inputs;
		std::vector<Actor_Instance*> outputs;

	public:

		Dataflow_Network() {}

		void add_actor_instance(Actor_Instance* actor) {
			actor_instances.push_back(actor);
			name_instance_map[actor->get_name()] = actor;
			id_instance_map[actor->get_id()] = actor;
		}

		Actor_Instance* get_actor_instance(std::string name) {
			return name_instance_map[name];
		}

		Actor_Instance* get_actor_instance_by_id(unsigned id) {
			return id_instance_map[id];
		}

		void add_edge(Edge& e) {
			edges.push_back(e);
		}

		void set_edges(std::vector<Edge> e) {
			edges = e;
		}

		std::vector<Actor_Instance*>& get_actor_instances(void) {
			return actor_instances;
		}

		std::vector<Edge>& get_edges(void) {
			return edges;
		}

		void add_actors_class_path(std::string actor, std::string path) {
			actors_class_path[actor] = path;
		}

		std::map<std::string, std::string>& get_actors_class_path_map(void) {
			return actors_class_path;
		}

		void add_id_class(std::string id, std::string c) {
			id_class_map[id] = c;
		}

		std::map<std::string, std::string>& get_id_class_map(void) {
			return id_class_map;
		}

		void add_parameter(std::string name, std::string param, std::string value) {
			parameters.push_back(std::make_tuple(name, param, value));
		}

		void get_params_for_instance(
			std::string instance_name,
			std::map<std::string, std::string>& map_out)
		{
			for (auto it = parameters.begin(); it != parameters.end(); ++it) {
				if (std::get<0>(*it) == instance_name) {
					map_out[std::get<1>(*it)] = std::get<2>(*it);
				}
			}
		}

		void add_actor(IR::Actor* a) {
			actors.push_back(a);
		}

		std::vector<IR::Actor*>* get_actors(void) {
			return &actors;
		}

		void add_composit_actor(IR::Composit_Actor* a) {
			composits.push_back(a);
		}

		std::vector<IR::Composit_Actor*>& get_composit_actors(void) {
			return composits;
		}

		std::string get_name(void) {
			return name;
		}

		void set_name(std::string n) {
			name = n;
		}

		void add_input(Actor_Instance* a) {
			inputs.push_back(a);
		}
		std::vector<Actor_Instance*>& get_inputs(void) {
			return inputs;
		}

		void add_output(Actor_Instance* a) {
			outputs.push_back(a);
		}
		std::vector<Actor_Instance*>& get_outputs(void) {
			return outputs;
		}

		void add_unit(std::string path, Unit* u) {
			path_unit_map[path] = u;
		}
		Unit* get_unit(std::string path) {
			if (path_unit_map.contains(path)) {
				return path_unit_map[path];
			}
			else {
				return nullptr;
			}
		}
	};
}

static inline IR::Edge* find_in_edge(IR::Actor_Instance *inst, std::string port) {
	for (IR::Edge* e : inst->get_in_edges()) {
		if (e->get_dst_port() == port) {
			return e;
		}
	}
	return nullptr;
}

static inline IR::Edge* find_out_edge(IR::Actor_Instance *inst, std::string port) {
	for (IR::Edge* e : inst->get_out_edges()) {
		if (e->get_src_port() == port) {
			return e;
		}
	}
	return nullptr;
}