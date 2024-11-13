#include "Code_Generation/Code_Generation.hpp"
#include "ABI_stdcpp.hpp"
#include "Config/config.h"
#include "Config/debug.h"
#include <string>
#include <fstream>
#include <filesystem>

/* Generate the file Channel.hpp containing the Channel base class, the data channel derived class
 * and if required the control channel derived class that is not carring data explicitly.
 */
std::string data_channel =
	"#pragma once\n\n"
    "#include <cstdlib>\n\n"
    "template<typename T>\n"
    "class Channel {\n"
    "protected:\n"
    "    volatile size_t read_index{ 0 };\n"
    "    volatile size_t write_index{ 0 };\n"
    "    size_t max_size{ 0 };\n"
    "    volatile bool full{ false };\n"
    "    T* data{ nullptr };\n"
    "\n"
    "    void increment_write_index(void);\n"
    "\n"
    "    void increment_read_index(void);\n"
    "\n"
    "    Channel(size_t max);\n"
    "\n"
    "public:\n"
    "    T read(void);\n"
    "    void write(T t);\n"
    "    T preview(size_t offset);\n"
    "    size_t size(void);\n"
    "    size_t free(void);\n"
    "\n"
    "    //Following functions are required for OMP, not used otherwise\n"
    "    size_t max(void);\n"
    "\n"
    "    size_t get_read_index(void);\n"
    "\n"
    "    size_t get_write_index(void);\n"
    "\n"
    "    void notify_read(size_t elements);\n"
    "    void notify_write(size_t elements);\n"
    "\n"
    "    T* data_ptr(void);\n"
    "};\n"
    "\n"
    "template<typename T>\n"
    "class Data_Channel : public Channel<T> {\n"
    "public:\n"
    "\n"
    "    Data_Channel(size_t max);\n"
    "\n"
    "    T read(void);\n"
    "\n"
    "    void write(T t);\n"
    "\n"
    "    T preview(size_t offset);\n"
    "\n"
    "    void notify_read(size_t elements);\n"
    "\n"
    "    void notify_write(size_t elements);\n"
    "};\n"
    "\n"
    "template<typename T>\n"
    "void Channel<T>::increment_write_index(void) {\n"
    "    if (write_index == (max_size - 1)) {\n"
    "        write_index = 0;\n"
    "    }\n"
    "    else {\n"
    "        ++write_index;\n"
    "    }\n"
    "    if (read_index == write_index) {\n"
    "        full = true;\n"
    "    }\n"
    "}\n"
    "\n"
    "template<typename T>\n"
    "void Channel<T>::increment_read_index(void) {\n"
    "    if (read_index == (max_size - 1)) {\n"
    "        read_index = 0;\n"
    "    }\n"
    "    else {\n"
    "        ++read_index;\n"
    "    }\n"
    "    if (full && (read_index != write_index)) {\n"
    "        full = false;\n"
    "    }\n"
    "}\n"
    "\n"
    "template<typename T>\n"
    "Channel<T>::Channel(size_t max) : max_size{ max } {};\n"
    "\n"
    "template<typename T>\n"
    "size_t Channel<T>::size(void) {\n"
    "    if (this->full) {\n"
    "        return this->max_size;\n"
    "    }\n"
    "    return (this->max_size + this->write_index - this->read_index) % this->max_size;\n"
    "}\n"
    "\n"
    "template<typename T>\n"
    "size_t Channel<T>::free(void) {\n"
    "    return max_size - size();\n"
    "}\n"
    "\n"
    "template<typename T>\n"
    "size_t Channel<T>::max(void) {\n"
    "    return max_size;\n"
    "}\n"
    "\n"
    "template<typename T>\n"
    "size_t Channel<T>::get_read_index(void) {\n"
    "    return read_index;\n"
    "}\n"
    "\n"
    "template<typename T>\n"
    "size_t Channel<T>::get_write_index(void) {\n"
    "    return write_index;\n"
    "}\n"
    "\n"
    "template<typename T>\n"
    "T* Channel<T>::data_ptr(void) {\n"
    "    return data;\n"
    "}\n"
    "\n"
    "template<typename T>\n"
    "Data_Channel<T>::Data_Channel(size_t max) : Channel<T>(max) {\n"
    "    this->data = (T*)malloc(sizeof(T) * max);\n"
    "}\n"
    "\n"
    "template<typename T>\n"
    "T Data_Channel<T>::read(void) {\n"
    "    T element = this->data[this->read_index];\n"
    "    this->increment_read_index();\n"
    "    return element;\n"
    "}\n"
    "\n"
    "template<typename T>\n"
    "void Data_Channel<T>::write(T t) {\n"
    "    this->data[this->write_index] = t;\n"
    "    this->increment_write_index();\n"
    "}\n"
    "\n"
    "template<typename T>\n"
    "T Data_Channel<T>::preview(size_t offset) {\n"
    "    return this->data[(this->read_index + offset) % this->max_size];\n"
    "}\n"
    "\n"
    "template<typename T>\n"
    "void Data_Channel<T>::notify_read(size_t elements) {\n"
    "    if (this->read_index >= (this->max_size - elements)) {\n"
    "        this->read_index = (this->read_index + elements) - this->max_size;\n"
    "    }\n"
    "    else {\n"
    "        this->read_index += elements;\n"
    "    }\n"
    "    if (this->full && (this->read_index != this->write_index)) {\n"
    "        this->full = false;\n"
    "    }\n"
    "}\n"
    "\n"
    "template<typename T>\n"
    "void Data_Channel<T>::notify_write(size_t elements) {\n"
    "    if (this->write_index >= (this->max_size - elements)) {\n"
    "        this->write_index = (this->write_index + elements) - this->max_size;\n"
    "    }\n"
    "    else {\n"
    "        this->write_index += elements;\n"
    "    }\n"
    "    if (this->read_index == this->write_index) {\n"
    "        this->full = true; \n"
    "    }\n"
    "}";
    
std::string control_channel =
        "template<typename T>\n"
        "class Control_Channel : public Channel<T> {\n"
        "public:\n"
        "\n"
        "    T read(void);\n"
        "\n"
        "    void write(T t);\n"
        "\n"
        "    T preview(size_t offset);\n"
        "\n"
        "    void notify_read(size_t elements);\n"
        "\n"
        "    void notify_write(size_t elements);\n"
        "};\n\n"
        "template<typename T>\n"
        "T Control_Channel<T>::read(void) {\n"
        "    this->increment_read_index();\n"
        "    return static_cast<T>(1);\n"
        "}\n"
        "\n"
        "template<typename T>\n"
        "void Control_Channel<T>::write(T t) {\n"
        "    this->increment_write_index();\n"
        "}\n"
        "\n"
        "template<typename T>\n"
        "T Control_Channel<T>::preview(size_t offset) {\n"
        "    return static_cast<T>(1);\n"
        "}\n"
        "\n"
        "template<typename T>\n"
        "void Control_Channel<T>::notify_read(size_t elements) {\n"
        "    if (this->read_index >= (this->max_size - elements)) {\n"
        "        this->read_index = (this->read_index + elements) - this->max_size;\n"
        "    }\n"
        "    else {\n"
        "        this->read_index += elements;\n"
        "    }\n"
        "    if (this->full && (this->read_index != this->write_index)) {\n"
        "        this->full = false;\n"
        "    }\n"
        "}\n"
        "\n"
        "template<typename T>\n"
        "void Control_Channel<T>::notify_write(size_t elements) {\n"
        "    if (this->write_index >= (this->max_size - elements)) {\n"
        "        this->write_index = (this->write_index + elements) - this->max_size;\n"
        "    }\n"
        "    else {\n"
        "        this->write_index += elements;\n"
        "    }\n"
        "    if (this->read_index == this->write_index) {\n"
        "        this->full = true;\n"
        "    }\n"
        "}";

std::pair<ABI_stdcpp::Header, ABI_stdcpp::Source>
ABI_stdcpp::generate_channel_code(bool cntrl_chan)
{
	Config* c = c->getInstance();

    std::filesystem::path path{ c->get_target_dir() };
    path /= "Channel.hpp";

	std::ofstream output_file{ path };
	if (output_file.fail()) {
		throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path.string() };
	}
	output_file << data_channel;

	// only create control channel class if required
	if (cntrl_chan) {
		output_file << "\n\n" << control_channel;
	}

	output_file.close();

    return std::make_pair("Channel.hpp", "");
}

std::pair<std::string, ABI_stdcpp::Impl_Type> ABI_stdcpp::channel_decl(
    std::string channel_name,
    std::string size,
    std::string type,
    bool static_def,
    std::string prefix)
{
    std::string p2, p1;
    p2 = "Data_Channel<" + type +">";
    p1 = prefix;
    if (static_def) {
        p1.append("Data_Channel<" + type + "> " + channel_name);
        p1.append("{" + size + "};\n");
    }
    else {
        p1.append("Data_Channel<" + type + "> *" + channel_name + "; \n");
    }
    return std::make_pair(p1, p2);
}

std::string ABI_stdcpp::channel_init(
    std::string channel_name,
    Impl_Type t,
    std::string type,
    std::string sz,
    std::string prefix)
{
    std::string r = prefix + channel_name + " = new " + t + "(" + sz + "); \n";
    return r;
}

std::string ABI_stdcpp::channel_read(
    std::string channel)
{
    return channel + "->read()";
}

std::string ABI_stdcpp::channel_write(
    std::string var,
    std::string channel)
{
    return channel + "->write(" + var + ")";
}

std::string ABI_stdcpp::channel_prefetch(
    std::string channel,
    std::string offset)
{
    return channel + "->preview(" + offset + ")";
}

std::string ABI_stdcpp::channel_size(
    std::string channel)
{
    return channel + "->size()";
}

std::string ABI_stdcpp::channel_free(
    std::string channel)
{
    return channel + "->free()";
}