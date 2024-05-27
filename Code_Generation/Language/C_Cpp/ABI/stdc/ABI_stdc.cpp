#include "ABI_stdc.hpp"

void ABI_stdc::init_ABI_support(IR::Dataflow_Network* dpn)
{
	/* Intentially left empty */
}

//Not required by this ABI as it cannot use multithreading anyhow!
std::string ABI_stdc::atomic_include(void)
{
	return "";
}

std::string ABI_stdc::atomic_var_decl(
	std::string var,
	std::string prefix)
{
	return "";
}

std::string ABI_stdc::atomic_test_set(
	std::string var,
	std::string prefix)
{
	return "";
}

std::string ABI_stdc::atomic_clear(
	std::string var,
	std::string prefix)
{
	return "";
}

// No multithreading in standard, would require POSIX or win32 ABI extension!
std::string ABI_stdc::thread_creation_include(void)
{
	return "";
}

std::string ABI_stdc::thread_creation(
	std::string function,
	std::string prefix,
	std::string& identifier_out)
{
	return "";
}

std::string ABI_stdc::thread_start(
	std::string identifier,
	std::string prefix)
{
	return "";
}

std::string ABI_stdc::thread_join(
	std::string identifier,
	std::string prefix)
{
	return "";
}

std::string ABI_stdc::allocation_include(void)
{
	return "#include <stdlib.h>\n";
}

std::string ABI_stdc::allocation(
	std::string var,
	std::string size,
	std::string type,
	std::string prefix)
{
	return prefix + var + " = (" + type + "*)malloc("+size+");\n";
}

