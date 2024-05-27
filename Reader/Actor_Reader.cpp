#include "Reader.hpp"
#include <map>
#include <vector>
#include "Config/debug.h"
#include <iostream>
#include <fstream>
#include <sstream>

// Map the name of an instance to it's instance object to speed up linking between edges and actor instances.
static std::map<std::string, IR::Actor_Instance*> instance_map{};
// Map the name of an actor to it's actor object to speed up linking between instances and actors
static std::map<std::string, IR::Actor*> actor_map{};

/* Create objects for all actors (not instances!) and add them to the Dataflow_Network object. */
static void create_actor_objects(IR::Dataflow_Network* dpn) {
	for (auto it = dpn->get_actors_class_path_map().begin();
		it != dpn->get_actors_class_path_map().end(); ++it)
	{
#ifdef DEBUG_READER_ACTORS
		std::cout << "Creating actor " << it->first << " with path " << it->second << "." << std::endl;
#endif
		IR::Actor* a = new IR::Actor(it->first, it->second);
		actor_map[it->first] = a;
		dpn->add_actor(a);
	}
}

/* Check whether already an object for an actor instance has been created,
 * if not, create one and add it to the dataflow network object.
 * The function is also responsible to add the parameter values to the actor instance object!
 * The function returns a pointer to the corresponding actor instance regardless whether it has
 * been created during this executed or has already been created before.
 */
static IR::Actor_Instance* check_instance_and_create(
	IR::Dataflow_Network* dpn,
	std::string id,
	std::string cal_class)
{
	if (instance_map.count(id) == 0) {
		// Instance has not been created yet, create and assign.
		IR::Actor_Instance* ret = new IR::Actor_Instance(id, actor_map[cal_class]);
#ifdef DEBUG_READER_ACTORS
		std::cout << "Creating actor instance " << id <<", referring to actor " << cal_class << std::endl;
#endif
		instance_map[id] = ret;
		dpn->add_actor_instance(ret);
		//Add paramteres to this instance
		dpn->get_params_for_instance(id, ret->get_parameters());
		return ret;
	}
	else {
		return instance_map[id];
	}
}

/* Create all actor (instances) in the network and connect them to the edges. */
void Network_Reader::read_actors(IR::Dataflow_Network* dpn){

	create_actor_objects(dpn);

	for (auto it = dpn->get_edges().begin(); it != dpn->get_edges().end(); ++it) {

		std::string src_id = it->get_src_id();
		std::string dst_id = it->get_dst_id();

		std::string src_class = dpn->get_id_class_map()[src_id];
		std::string dst_class = dpn->get_id_class_map()[dst_id];

		IR::Actor_Instance* src = check_instance_and_create(dpn, src_id, src_class);
		IR::Actor_Instance* dst = check_instance_and_create(dpn, dst_id, dst_class);

		if (it->get_source() == nullptr) {
			it->set_source(src);
			src->add_out_edge(&(*it));
		}
		else {
			if (it->get_source() != src) {
				throw Network_Reader_Exception("Found corrupted edge definition.");
			}
		}
		if (it->get_sink() == nullptr) {
			it->set_sink(dst);
			dst->add_in_edge(&(*it));
		}
		else {
			/* Only a sanity check, shouldn't happen if the network is correct. */
			if (it->get_sink() != dst) {
				throw Network_Reader_Exception("Found corrupted edge definition.");
			}
		}
	}

	// Get the source code of all actors
	for (auto it = actor_map.begin(); it != actor_map.end(); ++it) {
		it->second->read_actor();
	}
}

/* Read the source code of an actor. */
void IR::Actor::read_actor(void) {
	std::ifstream file(path, std::ifstream::in);
	if (file.bad()) {
		throw Network_Reader::Network_Reader_Exception{ "Cannot open the file " + path };
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	code = buffer.str();

#ifdef DEBUG_READER_ACTORS
	std::cout << "Read file of actor " << class_name << std::endl;
#endif
}