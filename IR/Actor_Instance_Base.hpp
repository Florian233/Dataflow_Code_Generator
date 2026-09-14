#pragma once

#include <string>
#include "IR/AST/AST.hpp"

namespace IR {
	class Edge;

	class Actor_Instance_Base {

	public:
		virtual std::string get_name(void) = 0;
		virtual unsigned get_sched_loop_bound(void) = 0;
		virtual unsigned get_mapping(void) = 0;
		virtual unsigned get_sched_prio(void) = 0;
		virtual void set_sched_prio(unsigned prio) = 0;
		virtual std::vector<IR::Edge*>& get_out_edges(void) = 0;
		virtual std::vector<IR::Edge*>& get_in_edges(void) = 0;
		virtual AST::AST_Root* get_ast(void) = 0;
		virtual std::map<std::string, std::string>& get_const_map(void) = 0;
	};

}