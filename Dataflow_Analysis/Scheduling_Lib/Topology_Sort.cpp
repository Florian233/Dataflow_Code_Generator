#include "Scheduling_Lib.hpp"
#include <set>
#include <map>
#include <vector>
#include <string>
#include "Optimization_Phase2/Optimization_Phase2.hpp"

struct Cmp_By_Name {
	bool operator()(IR::Actor_Instance_Base* a, IR::Actor_Instance_Base* b) const {
		std::string na = a->get_name();
		std::string nb = b->get_name();
		if (na != nb) {
			return na < nb;
		}
		return a < b;
	}
};

static void topology_sort_base(
	const std::set<IR::Actor_Instance_Base*>& nodes,
	std::vector<IR::Actor_Instance_Base*>& sorted)
{
	std::map<IR::Actor_Instance_Base*, unsigned> in_degree;
	for (IR::Actor_Instance_Base* node : nodes) {
		unsigned degree = 0;
		for (IR::Edge* edge : node->get_in_edges()) {
			if (!edge->get_feedback() && nodes.contains(edge->get_source())) {
				++degree;
			}
		}
		in_degree[node] = degree;
	}

	std::set<IR::Actor_Instance_Base*, Cmp_By_Name> ready;
	std::set<IR::Actor_Instance_Base*> remaining(nodes.begin(), nodes.end());
	for (IR::Actor_Instance_Base* node : nodes) {
		if (in_degree[node] == 0) {
			ready.insert(node);
		}
	}

	while (!remaining.empty()) {
		if (ready.empty()) {
			IR::Actor_Instance_Base* pick = nullptr;
			for (IR::Actor_Instance_Base* node : remaining) {
				if (pick == nullptr ||
					in_degree[node] < in_degree[pick] ||
					(in_degree[node] == in_degree[pick] && Cmp_By_Name()(node, pick))) {
					pick = node;
				}
			}
			ready.insert(pick);
		}

		IR::Actor_Instance_Base* node = *ready.begin();
		ready.erase(ready.begin());
		remaining.erase(node);
		sorted.push_back(node);

		for (IR::Edge* edge : node->get_out_edges()) {
			if (edge->get_feedback()) {
				continue;
			}
			IR::Actor_Instance_Base* sink = edge->get_sink();
			if (!remaining.contains(sink)) {
				continue;
			}
			unsigned& degree = in_degree[sink];
			if (degree > 0 && --degree == 0) {
				ready.insert(sink);
			}
		}
	}
}

static IR::Actor_Instance_Base* find_cycle(
	std::map<IR::Actor_Instance_Base*, IR::Actor_Instance_Base*>& parent,
	IR::Actor_Instance_Base* x)
{
	while (parent[x] != x) {
		parent[x] = parent[parent[x]];
		x = parent[x];
	}
	return x;
}

static void combine_cc(
	std::map<IR::Actor_Instance_Base*, IR::Actor_Instance_Base*>& parent,
	IR::Actor_Instance_Base* a,
	IR::Actor_Instance_Base* b)
{
	parent[find_cycle(parent, a)] = find_cycle(parent, b);
}

static void find_connected_clusters(
	const std::set<IR::Actor_Instance_Base*>& nodes,
	std::vector<std::set<IR::Actor_Instance_Base*>>& clusters)
{
	std::map<IR::Actor_Instance_Base*, IR::Actor_Instance_Base*> parent;
	for (IR::Actor_Instance_Base* n : nodes) {
		parent[n] = n;
	}

	for (IR::Actor_Instance_Base* n : nodes) {
		for (IR::Edge* e : n->get_out_edges()) {
			if (e->is_deleted() || e->get_feedback()) {
				continue;
			}
			if (nodes.contains(e->get_sink())) {
				combine_cc(parent, n, e->get_sink());
			}
		}
	}

	std::map<IR::Actor_Instance_Base*, std::set<IR::Actor_Instance_Base*>> buckets;
	for (IR::Actor_Instance_Base* n : nodes) {
		buckets[find_cycle(parent, n)].insert(n);
	}
	for (auto& entry : buckets) {
		clusters.push_back(entry.second);
	}
}

void Scheduling::determine_connected_clusters(
	const std::set<IR::Actor_Instance_Base*>& nodes,
	std::vector<std::vector<IR::Actor_Instance_Base*>>& components)
{
	std::vector<std::set<IR::Actor_Instance_Base*>> clusters;
	find_connected_clusters(nodes, clusters);

	for (auto& component : clusters) {
		std::vector<IR::Actor_Instance_Base*> sorted;
		topology_sort_base(component, sorted);
		components.push_back(sorted);
	}
}

bool Scheduling::is_connected(
	const std::set<IR::Actor_Instance_Base*>& nodes)
{
	std::vector<std::set<IR::Actor_Instance_Base*>> clusters;
	find_connected_clusters(nodes, clusters);
	return clusters.size() <= 1;
}

void Scheduling::topology_sort(
	std::set<std::string>& actors,
	IR::Dataflow_Network* dpn,
	std::vector<std::string>& sorted_actors)
{
	std::set<IR::Actor_Instance_Base*> nodes;
	for (const auto& name : actors) {
		IR::Actor_Instance* inst = dpn->get_actor_instance(name);
		if (inst != nullptr) {
			nodes.insert(inst);
		}
	}

	std::vector<IR::Actor_Instance_Base*> sorted;
	topology_sort_base(nodes, sorted);

	for (auto* n : sorted) {
		sorted_actors.push_back(n->get_name());
	}
}

void Scheduling::topology_sort_inst(
	std::set<IR::Actor_Instance_Base*>& actors,
	std::vector<IR::Actor_Instance_Base*>& sorted_actors)
{
	topology_sort_base(actors, sorted_actors);
}
