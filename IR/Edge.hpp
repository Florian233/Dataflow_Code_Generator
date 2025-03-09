#pragma once

#include <string>

namespace IR {
	class Actor_Instance;

	/* Information about an edge in the graph. */
	class Edge {
		IR::Actor_Instance* source;
		IR::Actor_Instance* sink;

		std::string dst_id;
		std::string dst_port;
		std::string src_id;
		std::string src_port;
		unsigned int specified_size{};
		bool dst_network_instance_port{ false };
		bool src_network_instance_port{ false };

		bool deleted{ false };

		bool feedback = false;

	public:
		Edge(Actor_Instance* so, Actor_Instance* si) : source(so), sink(si) {

		}

		Edge(
			std::string dst_id,
			std::string dst_port,
			std::string src_id,
			std::string src_port)
			: dst_id(dst_id), dst_port(dst_port), src_id(src_id), src_port(src_port)
		{
			source = nullptr;
			sink = nullptr;
		}

		Edge(
			Actor_Instance* so,
			Actor_Instance* si,
			std::string dst_id,
			std::string dst_port,
			std::string src_id,
			std::string src_port)
			: source(so), sink(si), dst_id(dst_id), dst_port(dst_port), src_id(src_id),
			src_port(src_port)
		{

		}

		IR::Actor_Instance* get_source(void) {
			return source;
		}

		IR::Actor_Instance* get_sink(void) {
			return sink;
		}

		void set_source(IR::Actor_Instance* s) {
			source = s;
		}

		void set_sink(IR::Actor_Instance* s) {
			sink = s;
		}

		std::string get_dst_id(void) {
			return dst_id;
		}

		void set_dst_id(std::string d) {
			dst_id = d;
		}

		std::string get_dst_port(void) {
			return dst_port;
		}

		void set_dst_port(std::string d) {
			dst_port = d;
		}

		std::string get_src_id(void) {
			return src_id;
		}

		void set_src_id(std::string s) {
			src_id = s;
		}

		std::string get_src_port(void) {
			return src_port;
		}

		void set_src_port(std::string s) {
			src_port = s;
		}

		unsigned int get_specified_size(void) {
			return specified_size;
		}

		void set_specified_size(unsigned s) {
			specified_size = s;
		}

		bool get_dst_network_instance_port(void) {
			return dst_network_instance_port;
		}

		void set_dst_network_instance_port(bool b) {
			dst_network_instance_port = b;
		}

		bool get_src_network_instance_port(void) {
			return src_network_instance_port;
		}

		void set_src_network_instance_port(bool b) {
			src_network_instance_port = b;
		}

		std::string get_name(void) {
			return src_id + "_" + src_port + "_" + dst_id +"_" + dst_port;
		}

		void set_deleted(void) {
			deleted = true;
		}

		bool is_deleted(void) {
			return deleted;
		}

		void set_feedback(void) {
			feedback = true;
		}
		bool get_feedback(void) {
			return feedback;
		}
	};
}