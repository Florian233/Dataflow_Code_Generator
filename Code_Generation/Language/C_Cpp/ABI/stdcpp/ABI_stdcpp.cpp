#include "ABI_stdcpp.hpp"

void ABI_stdcpp::init_ABI_support(IR::Dataflow_Network* dpn)
{
	/* Intentially left empty */
}

std::string ABI_stdcpp::atomic_include(void)
{
	return "#include <atomic>\n";
}

std::string ABI_stdcpp::atomic_var_decl(
	std::string var,
	std::string prefix)
{
	return prefix + "std::atomic_flag " + var + "_lock = ATOMIC_FLAG_INIT;\n";
}

std::string ABI_stdcpp::atomic_test_set(
	std::string var,
	std::string prefix)
{
	return prefix + var + "_lock.test_and_set()";
}

std::string ABI_stdcpp::atomic_clear(
	std::string var,
	std::string prefix)
{
	return prefix + var + "_lock.clear();\n";
}

std::string ABI_stdcpp::thread_creation_include(void)
{
	return "#include <thread>\n";
}

static unsigned thread_count = 0;
std::string ABI_stdcpp::thread_creation(
	std::string function,
	std::string prefix,
	std::string& identifier_out)
{
	std::string result = prefix + "std::thread t" + std::to_string(thread_count) + "(" + function + ");\n";
	identifier_out = "t" + std::to_string(thread_count);
	++thread_count;

	return result;
}

std::string ABI_stdcpp::thread_start(
	std::string identifier,
	std::string prefix)
{
	// not required for this ABI
	return "";
}

std::string ABI_stdcpp::thread_join(
	std::string identifier,
	std::string prefix)
{
	return prefix + identifier + ".join();\n";
}

std::string ABI_stdcpp::allocation_include(void)
{
	// not required for this ABI
	return "";
}

std::string ABI_stdcpp::allocation(
	std::string var,
	std::string size,
	std::string type,
	std::string prefix)
{
	// not required for this ABI.
	return "";
}