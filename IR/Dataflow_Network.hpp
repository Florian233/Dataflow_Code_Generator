#pragma once

#include "Actor_Instance.hpp"
#include "Edge.hpp"
#include "Actor.hpp"
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

		/* Tuple of (actor instance, parameter name, value) */
		std::vector< std::tuple<std::string, std::string, std::string> > parameters;

		std::vector<IR::Actor*> actors;

		std::vector<IR::Composit_Actor*> composits;

		std::string name;

	public:

		Dataflow_Network() {}

		void add_actor_instance(Actor_Instance* actor) {
			actor_instances.push_back(actor);
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
	};
}