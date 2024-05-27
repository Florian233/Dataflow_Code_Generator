#pragma once

#include "stdc/ABI_stdc.hpp"
#include "stdcpp/ABI_stdcpp.hpp"

#define ABI_OP_ASGN(enum_val, ret, namespace_val, op_val, ...) \
	case Target_ABI::##enum_val: (ret) = namespace_val::##op_val(__VA_ARGS__); break;

#define ABI_ALL_ASGN(conf, ret, op_val, ...) \
	switch((conf)->get_target_ABI()) {\
		ABI_OP_ASGN(stdc, ret, ABI_stdc, op_val, __VA_ARGS__); \
		ABI_OP_ASGN(stdcpp, ret, ABI_stdcpp, op_val, __VA_ARGS__); \
		default: \
			std::cout << "Unknown ABI...exiting." << std::endl; \
			exit(33); \
	}\

#define ABI_OP_RET(enum_val, namespace_val, op_val, ...) \
	case Target_ABI::##enum_val: return namespace_val::##op_val(__VA_ARGS__);

#define ABI_ALL_RET(conf, op_val, ...) \
	switch((conf)->get_target_ABI()) {\
		ABI_OP_RET(stdc, ABI_stdc, op_val, __VA_ARGS__) \
		ABI_OP_RET(stdcpp, ABI_stdcpp, op_val, __VA_ARGS__) \
		default: \
			std::cout << "Unknown ABI...exiting." << std::endl; \
			exit(33); \
	}\

#define ABI_ATOMIC_HEADER(conf, ret) ABI_ALL_ASGN(conf, ret, atomic_include)
#define ABI_ATOMIC_DECL(conf, ret, var, prefix) ABI_ALL_ASGN(conf, ret, atomic_var_decl, var, prefix)
#define ABI_ATOMIC_TEST_SET(conf, ret, var, prefix) ABI_ALL_ASGN(conf, ret, atomic_test_set, var, prefix)
#define ABI_ATOMIC_CLEAR(conf, ret, var, prefix) ABI_ALL_ASGN(conf, ret, atomic_clear, var, prefix)

#define ABI_CHANNEL_GEN(conf, c) ABI_ALL_RET(conf, generate_channel_code, c)
#define ABI_CHANNEL_DECL(conf, ret, name, sz, type, s, prefx) ABI_ALL_ASGN(conf, ret, channel_decl, name, sz, type, s, prefx)
#define ABI_CHANNEL_INIT(conf, ret, name, type, impl_type, sz, prefix) ABI_ALL_ASGN(conf, ret, channel_init, name, type, impl_type, sz, prefix)
#define ABI_CHANNEL_READ(conf, ret, channel) ABI_ALL_ASGN(conf, ret, channel_read, channel)
#define ABI_CHANNEL_WRITE(conf, ret, var, channel) ABI_ALL_ASGN(conf, ret, channel_write, var, channel)
#define ABI_CHANNEL_PREFETCH(conf, ret, channel, offset) ABI_ALL_ASGN(conf, ret, channel_prefetch, channel, offset)
#define ABI_CHANNEL_SIZE(conf, ret, channel) ABI_ALL_ASGN(conf, ret, channel_size, channel)
#define ABI_CHANNEL_FREE(conf, ret, channel) ABI_ALL_ASGN(conf, ret, channel_free, channel)

#define ABI_THREAD_HEADER(conf, ret) ABI_ALL_ASGN(conf, ret, thread_creation_include)
#define ABI_THREAD_CREATE(conf, ret,func, prefix, ident) ABI_ALL_ASGN(conf, ret, thread_creation, func, prefix, ident)
#define ABI_THREAD_START(conf, ret, ident, prefix) ABI_ALL_ASGN(conf, ret, thread_start, ident, prefix)
#define ABI_THREAD_JOIN(conf, ret, ident, prefix) ABI_ALL_ASGN(conf, ret, thread_join, ident, prefix)

#define ABI_INIT(conf, dpn) ABI_ALL_RET(conf, init_ABI_support, dpn)

#define ABI_ALLOC_HEADER(conf, ret) ABI_ALL_ASGN(conf, ret, allocation_include)
#define ABI_ALLOC(conf, ret, var, sz, type, prefix) ABI_ALL_ASGN(conf, ret, allocation, var, sz, type, prefix)