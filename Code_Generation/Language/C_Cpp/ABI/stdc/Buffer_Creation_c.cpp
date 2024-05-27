#include "ABI_stdc.hpp"
#include "Config/config.h"
#include "Config/debug.h"
#include <string>
#include <fstream>
#include <filesystem>
#include "Code_Generation/Code_Generation.hpp"

/* Generate the file Channel.hpp containing the Channel base class, the data channel derived class
 * and if required the control channel derived class that is not carring data explicitly.
 */
std::string c_buffer_impl =
"#ifndef CHANNEL_H\n"
"#define CHANNEL_H\n\n"
"#include <stdbool.h>\n"
"#include <stdlib.h>\n\n"
"typedef struct channel_u8 {\n"
"    volatile size_t read_index;\n"
"    volatile size_t write_index;\n"
"    volatile bool full;\n"
"    size_t max_size;\n"
"    unsigned char* data;\n"
"} channel_u8_t;\n"
"\n"
"static inline void init_u8(channel_u8_t* c, size_t sz) {\n"
"    c->read_index = 0;\n"
"    c->write_index = 0;\n"
"    c->full = false;\n"
"    c->max_size = sz;\n"
"    c->data = (unsigned char*)malloc(sz * sizeof(unsigned char));\n"
"}\n"
"\n"
"static inline size_t size_u8(channel_u8_t* c) {\n"
"    if (c->full) {\n"
"        return c->max_size;\n"
"    }\n"
"    return (c->max_size + c->write_index - c->read_index) % c->max_size;\n"
"}\n"
"\n"
"static inline size_t free_u8(channel_u8_t* c) {\n"
"    return c->max_size - size_u8(c);\n"
"}\n"
"\n"
"static inline unsigned char read_u8(channel_u8_t* c) {\n"
"    unsigned char element = c->data[c->read_index];\n"
"    if (c->read_index == (c->max_size - 1)) {\n"
"        c->read_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->read_index);\n"
"    }\n"
"    if (c->full && (c->read_index != c->write_index)) {\n"
"        c->full = false;\n"
"    }\n"
"    return element;\n"
"}\n"
"\n"
"static inline void write_u8(channel_u8_t* c, unsigned char t) {\n"
"    c->data[c->write_index] = t;\n"
"    if (c->write_index == (c->max_size - 1)) {\n"
"        c->write_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->write_index);\n"
"    }\n"
"    if (c->read_index == c->write_index) {\n"
"        c->full = true;\n"
"    }\n"
"}\n"
"\n"
"static inline unsigned char preview_u8(channel_u8_t* c, size_t offset) {\n"
"    return c->data[(c->read_index + offset) % c->max_size];\n"
"}\n"
"\n"
"typedef struct channel_s8 {\n"
"    volatile size_t read_index;\n"
"    volatile size_t write_index;\n"
"    volatile bool full;\n"
"    size_t max_size;\n"
"    signed char* data;\n"
"} channel_s8_t;\n"
"\n"
"static inline void init_s8(channel_s8_t* c, size_t sz) {\n"
"    c->read_index = 0;\n"
"    c->write_index = 0;\n"
"    c->full = false;\n"
"    c->max_size = sz;\n"
"    c->data = (signed char*)malloc(sz * sizeof(signed char));\n"
"}\n"
"\n"
"static inline size_t size_s8(channel_s8_t* c) {\n"
"    if (c->full) {\n"
"        return c->max_size;\n"
"    }\n"
"    return (c->max_size + c->write_index - c->read_index) % c->max_size;\n"
"}"
"\n"
"static inline size_t free_s8(channel_s8_t* c) {\n"
"    return c->max_size - size_s8(c);\n"
"}\n"
"\n"
"static inline signed char read_s8(channel_s8_t* c) {\n"
"    signed char element = c->data[c->read_index];\n"
"    if (c->read_index == (c->max_size - 1)) {\n"
"        c->read_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->read_index);\n"
"    }\n"
"    if (c->full && (c->read_index != c->write_index)) {\n"
"        c->full = false;\n"
"    }\n"
"    return element;\n"
"}\n"
"\n"
"static inline void write_s8(channel_s8_t* c, signed char t) {\n"
"    c->data[c->write_index] = t;\n"
"    if (c->write_index == (c->max_size - 1)) {\n"
"        c->write_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->write_index);\n"
"    }\n"
"    if (c->read_index == c->write_index) {\n"
"        c->full = true;\n"
"    }\n"
"}\n"
"\n"
"static inline signed char preview_s8(channel_s8_t* c, size_t offset) {\n"
"    return c->data[(c->read_index + offset) % c->max_size];\n"
"}\n"
"\n"
"typedef struct channel_u16 {\n"
"    volatile size_t read_index;\n"
"    volatile size_t write_index;\n"
"    volatile bool full;\n"
"    size_t max_size;\n"
"    unsigned short* data;\n"
"} channel_u16_t;\n"
"\n"
"static inline void init_u16(channel_u16_t* c, size_t sz) {\n"
"    c->read_index = 0;\n"
"    c->write_index = 0;\n"
"    c->full = false;\n"
"    c->max_size = sz;\n"
"    c->data = (unsigned short*)malloc(sz * sizeof(unsigned short));\n"
"}\n"
"\n"
"static inline size_t size_u16(channel_u16_t* c) {\n"
"    if (c->full) {\n"
"        return c->max_size;\n"
"    }\n"
"    return (c->max_size + c->write_index - c->read_index) % c->max_size;\n"
"}\n"
"\n"
"static inline size_t free_u16(channel_u16_t* c) {\n"
"    return c->max_size - size_u16(c);\n"
"}\n"
"\n"
"static inline unsigned short read_u16(channel_u16_t* c) {\n"
"    unsigned short element = c->data[c->read_index];\n"
"    if (c->read_index == (c->max_size - 1)) {\n"
"        c->read_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->read_index);\n"
"    }\n"
"    if (c->full && (c->read_index != c->write_index)) {\n"
"        c->full = false;\n"
"    }\n"
"    return element;\n"
"}\n"
"\n"
"static inline void write_u16(channel_u16_t* c, unsigned short t) {\n"
"    c->data[c->write_index] = t;\n"
"    if (c->write_index == (c->max_size - 1)) {\n"
"        c->write_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->write_index);\n"
"    }\n"
"    if (c->read_index == c->write_index) {\n"
"        c->full = true;\n"
"    }\n"
"}\n"
"\n"
"static inline unsigned short preview_u16(channel_u16_t* c, size_t offset) {\n"
"    return c->data[(c->read_index + offset) % c->max_size];\n"
"}\n"
"\n"
"typedef struct channel_s16 {\n"
"    volatile size_t read_index;\n"
"    volatile size_t write_index;\n"
"    volatile bool full;\n"
"    size_t max_size;\n"
"    signed short* data;\n"
"} channel_s16_t;\n"
"\n"
"static inline void init_s16(channel_s16_t* c, size_t sz) {\n"
"    c->read_index = 0;\n"
"    c->write_index = 0;\n"
"    c->full = false;\n"
"    c->max_size = sz;\n"
"    c->data = (signed short*)malloc(sz * sizeof(signed short));\n"
"}\n"
"\n"
"static inline size_t size_s16(channel_s16_t* c) {\n"
"    if (c->full) {\n"
"        return c->max_size;\n"
"    }\n"
"    return (c->max_size + c->write_index - c->read_index) % c->max_size;\n"
"}\n"
"\n"
"static inline size_t free_s16(channel_s16_t* c) {\n"
"    return c->max_size - size_s16(c);\n"
"}\n"
"\n"
"static inline signed short read_s16(channel_s16_t* c) {\n"
"    signed short element = c->data[c->read_index];\n"
"    if (c->read_index == (c->max_size - 1)) {\n"
"        c->read_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->read_index);\n"
"    }\n"
"    if (c->full && (c->read_index != c->write_index)) {\n"
"        c->full = false;\n"
"    }\n"
"    return element;\n"
"}\n"
"\n"
"static inline void write_s16(channel_s16_t* c, signed short t) {\n"
"    c->data[c->write_index] = t;\n"
"    if (c->write_index == (c->max_size - 1)) {\n"
"        c->write_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->write_index);\n"
"    }\n"
"    if (c->read_index == c->write_index) {\n"
"        c->full = true;\n"
"    }\n"
"}\n"
"\n"
"static inline signed short preview_s16(channel_s16_t* c, size_t offset) {\n"
"    return c->data[(c->read_index + offset) % c->max_size];\n"
"}\n"
"\n"
"typedef struct channel_u32 {\n"
"    volatile size_t read_index;\n"
"    volatile size_t write_index;\n"
"    volatile bool full;\n"
"    size_t max_size;\n"
"    unsigned int* data;\n"
"} channel_u32_t;\n"
"\n"
"static inline void init_u32(channel_u32_t* c, size_t sz) {\n"
"    c->read_index = 0;\n"
"    c->write_index = 0;\n"
"    c->full = false;\n"
"    c->max_size = sz;\n"
"    c->data = (unsigned int*)malloc(sz * sizeof(unsigned int));\n"
"}\n"
"\n"
"static inline size_t size_u32(channel_u32_t* c) {\n"
"    if (c->full) {\n"
"        return c->max_size;\n"
"    }\n"
"    return (c->max_size + c->write_index - c->read_index) % c->max_size;\n"
"}\n"
"\n"
"static inline size_t free_u32(channel_u32_t* c) {\n"
"    return c->max_size - size_u32(c);\n"
"}\n"
"\n"
"static inline unsigned int read_u32(channel_u32_t* c) {\n"
"    unsigned int element = c->data[c->read_index];\n"
"    if (c->read_index == (c->max_size - 1)) {\n"
"        c->read_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->read_index);\n"
"    }\n"
"    if (c->full && (c->read_index != c->write_index)) {\n"
"        c->full = false;\n"
"    }\n"
"    return element;\n"
"}\n"
"\n"
"static inline void write_u32(channel_u32_t* c, unsigned int t) {\n"
"    c->data[c->write_index] = t;\n"
"    if (c->write_index == (c->max_size - 1)) {\n"
"        c->write_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->write_index);\n"
"    }\n"
"    if (c->read_index == c->write_index) {\n"
"        c->full = true;\n"
"    }\n"
"}\n"
"\n"
"static inline unsigned int preview_u32(channel_u32_t* c, size_t offset) {\n"
"    return c->data[(c->read_index + offset) % c->max_size];\n"
"}\n"
"\n"
"typedef struct channel_s32 {\n"
"    volatile size_t read_index;\n"
"    volatile size_t write_index;\n"
"    volatile bool full;\n"
"    size_t max_size;\n"
"    signed int* data;\n"
"} channel_s32_t;\n"
"\n"
"static inline void init_s32(channel_s32_t* c, size_t sz) {\n"
"    c->read_index = 0;\n"
"    c->write_index = 0;\n"
"    c->full = false;\n"
"    c->max_size = sz;\n"
"    c->data = (signed int*)malloc(sz * sizeof(signed int));\n"
"}\n"
"\n"
"static inline size_t size_s32(channel_s32_t* c) {\n"
"    if (c->full) {\n"
"        return c->max_size;\n"
"    }\n"
"    return (c->max_size + c->write_index - c->read_index) % c->max_size;\n"
"}\n"
"\n"
"static inline size_t free_s32(channel_s32_t* c) {\n"
"    return c->max_size - size_s32(c);\n"
"}\n"
"\n"
"static inline signed int read_s32(channel_s32_t* c) {\n"
"    signed int element = c->data[c->read_index];\n"
"    if (c->read_index == (c->max_size - 1)) {\n"
"        c->read_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->read_index);\n"
"    }\n"
"    if (c->full && (c->read_index != c->write_index)) {\n"
"        c->full = false;\n"
"    }\n"
"    return element;\n"
"}\n"
"\n"
"static inline void write_s32(channel_s32_t* c, signed int t) {\n"
"    c->data[c->write_index] = t;\n"
"    if (c->write_index == (c->max_size - 1)) {\n"
"        c->write_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->write_index);\n"
"    }\n"
"    if (c->read_index == c->write_index) {\n"
"        c->full = true;\n"
"    }\n"
"}\n"
"\n"
"static inline signed int preview_s32(channel_s32_t* c, size_t offset) {\n"
"    return c->data[(c->read_index + offset) % c->max_size];\n"
"}\n"
"\n"
"\n"
"typedef struct channel_u64 {\n"
"    volatile size_t read_index;\n"
"    volatile size_t write_index;\n"
"    volatile bool full;\n"
"    size_t max_size;\n"
"    unsigned long long* data;\n"
"} channel_u64_t;\n"
"\n"
"static inline void init_u64(channel_u64_t* c, size_t sz) {\n"
"    c->read_index = 0;\n"
"    c->write_index = 0;\n"
"    c->full = false;\n"
"    c->max_size = sz;\n"
"    c->data = (unsigned long long*)malloc(sz * sizeof(unsigned long long));\n"
"}\n"
"\n"
"static inline size_t size_u64(channel_u64_t* c) {\n"
"    if (c->full) {\n"
"        return c->max_size;\n"
"    }\n"
"    return (c->max_size + c->write_index - c->read_index) % c->max_size;\n"
"}\n"
"\n"
"static inline size_t free_u64(channel_u64_t* c) {\n"
"    return c->max_size - size_u64(c);\n"
"}\n"
"\n"
"static inline unsigned long long read_u64(channel_u64_t* c) {\n"
"    unsigned long long element = c->data[c->read_index];\n"
"    if (c->read_index == (c->max_size - 1)) {\n"
"        c->read_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->read_index);\n"
"    }\n"
"    if (c->full && (c->read_index != c->write_index)) {\n"
"        c->full = false;\n"
"    }\n"
"    return element;\n"
"}\n"
"\n"
"static inline void write_u64(channel_u64_t* c, unsigned long long t) {\n"
"    c->data[c->write_index] = t;\n"
"    if (c->write_index == (c->max_size - 1)) {\n"
"        c->write_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->write_index);\n"
"    }\n"
"    if (c->read_index == c->write_index) {\n"
"        c->full = true;\n"
"    }\n"
"}\n"
"\n"
"static inline unsigned long long preview_u64(channel_u64_t* c, size_t offset) {\n"
"    return c->data[(c->read_index + offset) % c->max_size];\n"
"}\n"
"\n"
"typedef struct channel_s64 {\n"
"    volatile size_t read_index;\n"
"    volatile size_t write_index;\n"
"    volatile bool full;\n"
"    size_t max_size;\n"
"    signed long long* data;\n"
"} channel_s64_t;\n"
"\n"
"static inline void init_s64(channel_s64_t* c, size_t sz) {\n"
"    c->read_index = 0;\n"
"    c->write_index = 0;\n"
"    c->full = false;\n"
"    c->max_size = sz;\n"
"    c->data = (signed long long*)malloc(sz * sizeof(signed long long));\n"
"}\n"
"\n"
"static inline size_t size_s64(channel_s64_t* c) {\n"
"    if (c->full) {\n"
"        return c->max_size;\n"
"    }\n"
"    return (c->max_size + c->write_index - c->read_index) % c->max_size;\n"
"}\n"
"\n"
"static inline size_t free_s64(channel_s64_t* c) {\n"
"    return c->max_size - size_s64(c);\n"
"}\n"
"\n"
"static inline signed long long read_s64(channel_s64_t* c) {\n"
"    signed long long element = c->data[c->read_index];\n"
"    if (c->read_index == (c->max_size - 1)) {\n"
"        c->read_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->read_index);\n"
"    }\n"
"    if (c->full && (c->read_index != c->write_index)) {\n"
"        c->full = false;\n"
"    }\n"
"    return element;\n"
"}\n"
"\n"
"static inline void write_s64(channel_s64_t* c, signed long long t) {\n"
"    c->data[c->write_index] = t;\n"
"    if (c->write_index == (c->max_size - 1)) {\n"
"        c->write_index = 0;\n"
"    }\n"
"    else {\n"
"        ++(c->write_index);\n"
"    }\n"
"    if (c->read_index == c->write_index) {\n"
"        c->full = true;\n"
"    }\n"
"}\n"
"\n"
"static inline signed long long preview_s64(channel_s64_t* c, size_t offset) {\n"
"    return c->data[(c->read_index + offset) % c->max_size];\n"
"}\n"
"#endif";

std::string control_chan_impl;

std::pair<ABI_stdc::Header, ABI_stdc::Source>
ABI_stdc::generate_channel_code(bool cntrl_chan)
{
	Config* c = c->getInstance();

    std::filesystem::path path{ c->get_target_dir() };
    path /= "channel.h";

	std::ofstream output_file{ path };
	if (output_file.bad()) {
		throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path.string() };
	}
	output_file << c_buffer_impl;

	// only create control channel class if required
	if (cntrl_chan) {
		output_file << "\n\n" << control_chan_impl;
	}

	output_file.close();

    return std::make_pair("channel.h", "");
}

// not covering float, bool and string types....
static std::string translate_channel(std::string type) {
	if (type == "char") {
		return "s8";
	}
	else if (type == "unsigned char") {
		return "u8";
	}
	else if (type == "short") {
		return "s16";
	}
	else if (type == "unsigned short") {
		return "u16";
	}
	else if (type == "int") {
		return "s32";
	}
	else if (type == "unsigned int") {
		return "u32";
	}
	else if (type == "long") {
		return "s64";
	}
	else if (type == "unsigned long") {
		return "u64";
	}
	else {
		return "s32";
	}
}

static std::map<std::string, std::string> name_type_map;
static bool is_static = false;

std::pair<std::string, ABI_stdc::Impl_Type> ABI_stdc::channel_decl(
	std::string channel_name,
	std::string size,
	std::string type,
	bool static_def,
	std::string prefix)
{
	std::string impl_type = translate_channel(type);
	name_type_map[channel_name] = impl_type;
	static_def = is_static;

	std::string decl = prefix + "channel_" + impl_type + "_t ";
	if (!static_def) {
		decl.append("*");
	}
	decl.append(channel_name);
	if (static_def) {
		decl.append("{.read_index = 0, .write_index = 0, .full = false, .max_size = " + size +" }");
	}
	decl.append(";\n");
	return std::make_pair(decl, impl_type);
}

std::string ABI_stdc::channel_init(
	std::string channel_name,
	ABI_stdc::Impl_Type t,
	std::string type,
	std::string sz,
	std::string prefix)
{
	std::string ret = prefix;
	if (is_static) {
		ret.append(channel_name + ".data = ");
		ret.append("(" + type + "*)malloc(sizeof(" + type + ") * " + sz + ");\n");
	}
	else {
		ret.append(channel_name + " = (channel_" + name_type_map[channel_name] + "_t*)malloc(sizeof(channel_" + name_type_map[channel_name] + "_t));\n");
		ret.append(prefix + "init_"+ name_type_map[channel_name]+"("+channel_name+", "+sz+");\n");
	}
	return ret;
}

std::string ABI_stdc::channel_read(
	std::string channel)
{
	return "read_" + name_type_map[channel] +"(_g->" + channel+")";
}

std::string ABI_stdc::channel_write(
	std::string var,
	std::string channel)
{
	return "write_" + name_type_map[channel] + "(_g->" + channel + ", " + var + ")";
}

std::string ABI_stdc::channel_prefetch(
	std::string channel,
	std::string offset)
{
	return "prefetch_" + name_type_map[channel] + "(_g->" + channel + ", " + offset + ")";
}

std::string ABI_stdc::channel_size(
	std::string channel)
{
	return "size_" + name_type_map[channel] + "(_g->" + channel + ")";
}

std::string ABI_stdc::channel_free(
	std::string channel)
{
	return "free_" + name_type_map[channel] + "(_g->" + channel + ")";
}