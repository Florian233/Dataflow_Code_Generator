#include "ABI_rtos.hpp"
#include "Config/config.h"
#include "Dataflow_Analysis/Scheduling_Lib/Scheduling_Lib.hpp"

void ABI_rtos::init_ABI_support(IR::Dataflow_Network* dpn)
{
	std::set<IR::Actor_Instance_Base*> c;
	// We need a sorted list of actors, then we can take the max for all predecessors + 1
	for (auto it = dpn->get_actor_instances().begin();
		it != dpn->get_actor_instances().end(); ++it)
	{
		IR::Actor_Instance* inst = *it;
		if (inst->is_deleted() || (inst->get_composit_actor() != nullptr)) {
			continue;
		}
		c.insert(inst);
	}
	for (auto it = dpn->get_composit_actors().begin();
		it != dpn->get_composit_actors().end(); ++it)
	{
		IR::Composit_Actor* inst = *it;
		c.insert(inst);
	}

	std::vector<IR::Actor_Instance_Base*> sorted_cluster;
	Scheduling::topology_sort_inst(c, sorted_cluster);

	for (auto inst : sorted_cluster) {
		unsigned prio = 40;
		for (auto it = inst->get_in_edges().begin(); it != inst->get_in_edges().end(); ++it) {
			if ((*it)->is_deleted() || (*it)->get_feedback()) {
				continue;
			}
			IR::Actor_Instance_Base* s = (*it)->get_source();
			if (s->get_sched_prio() >= prio) {
				prio = s->get_sched_prio() + 1;
			}
		}
		// cap prio a little, I'm not sure whether already at 110 is necessary, usually all RTOS have at least a byte-width prio type, but anyhow...
		// data processing applications like this usually are not the most critical ones
		if (prio > 110) {
			prio = 110;
		}
		inst->set_sched_prio(prio);
	}
}

std::string ABI_rtos::atomic_include(void)
{
	return "";
}

std::string ABI_rtos::atomic_var_decl(
	std::string var,
	std::string prefix)
{
	return "";
}

std::string ABI_rtos::atomic_test_set(
	std::string var,
	std::string prefix)
{
	return "";
}

std::string ABI_rtos::atomic_clear(
	std::string var,
	std::string prefix)
{
	return "";
}

std::string ABI_rtos::thread_creation_include(void)
{
	return "";
}

static unsigned thread_count = 0;
std::string ABI_rtos::thread_creation(
	std::string function,
	std::string prefix,
	std::string& identifier_out)
{
	return "";
}

std::string ABI_rtos::thread_start(
	std::string identifier,
	std::string prefix)
{
	// not required for this ABI
	return "";
}

std::string ABI_rtos::thread_join(
	std::string identifier,
	std::string prefix)
{
	return "";
}

std::string ABI_rtos::allocation_include(void)
{
	// not required for this ABI
	return "";
}

std::string ABI_rtos::allocation(
	std::string var,
	std::string size,
	std::string type,
	std::string prefix)
{
	// not required for this ABI.
	return "";
}

std::string ABI_rtos::add_constructor_code(
	std::string actor_class_name)
{
	Config* c = Config::getInstance();
	std::string s = "";

	if (c->get_use_mutex()) {
		s = "\tmutex_init(&sched_mutex, 0);\n";
	}
	else if (c->get_use_atomics()) {
		s = "\tsched_atomic = 0;\n";
	}

	return s;
}