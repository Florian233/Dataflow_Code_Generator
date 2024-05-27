#pragma once

#include <tuple>
#include <string>
#include "IR/Dataflow_Network.hpp"


namespace ABI_stdc {
	using Header = std::string;
	using Source = std::string;
	using Impl_Type = std::string;

	void init_ABI_support(IR::Dataflow_Network* dpn);

	std::pair<Header, Source> generate_channel_code(bool cntrl_chan);

	std::string atomic_include(void);
	std::string atomic_var_decl(
		std::string var,
		std::string prefix);
	std::string atomic_test_set(
		std::string var,
		std::string prefix);
	std::string atomic_clear(
		std::string var,
		std::string prefix);

	std::string thread_creation_include(void);
	std::string thread_creation(
		std::string function,
		std::string prefix,
		std::string& identifier_out);
	std::string thread_start(
		std::string identifier,
		std::string prefix);
	std::string thread_join(
		std::string identifier,
		std::string prefix);

	std::string allocation_include(void);
	std::string allocation(
		std::string var,
		std::string size,
		std::string type,
		std::string prefix);

	std::pair<std::string, Impl_Type> channel_decl(
		std::string channel_name,
		std::string size,
		std::string type,
		bool static_def,
		std::string prefix);
	std::string channel_init(
		std::string channel_name,
		Impl_Type t,
		std::string type,
		std::string sz,
		std::string prefix);
	std::string channel_read(
		std::string channel);
	std::string channel_write(
		std::string var,
		std::string channel);
	std::string channel_prefetch(
		std::string channel,
		std::string offset);
	std::string channel_size(
		std::string channel);
	std::string channel_free(
		std::string channel);
}