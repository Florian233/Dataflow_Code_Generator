#pragma once
#include <string>
#include "Tokenizer/Token_Container.hpp"
#include "common/include/Exceptions.hpp"
#include "Tokenizer/Method_Buffer.hpp"
#include "Tokenizer/Tokenizer.hpp"
#include "Tokenizer/Var_Buffer.hpp"
#include "Tokenizer/Native_Buffer.hpp"
#include <vector>

namespace IR {

	class Dataflow_Network;

	class Unit {
	private:

		std::string code;
		bool initialized{ false };

		std::vector<Var_Buffer> var_buffers{};
		std::vector<Method_Buffer> method_buffers{};
		std::vector<Native_Buffer> native_buffers{};

		std::vector<std::pair<std::string, Unit*>> sub_units;

		void convert_imports(Token& t, Tokenizer& b, IR::Dataflow_Network* dpn);

	public:

		Unit(std::string c) : code{ c } {

		}

		void add_var_buffer(Var_Buffer& b) {
			var_buffers.push_back(b);
		}
		std::vector<Var_Buffer>& get_var_buffers(void) {
			return var_buffers;
		}

		void add_method_buffer(Method_Buffer& b) {
			method_buffers.push_back(b);
		}
		std::vector<Method_Buffer>& get_method_buffers(void) {
			return method_buffers;
		}

		void add_native_buffer(Native_Buffer& b) {
			native_buffers.push_back(b);
		}
		std::vector<Native_Buffer>& get_native_buffers(void) {
			return native_buffers;
		}

		void initialize(IR::Dataflow_Network* dpn);
	};
}