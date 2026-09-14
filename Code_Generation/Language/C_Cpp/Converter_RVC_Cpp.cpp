#include "Converter_RVC_Cpp.hpp"
#include "Conversion/Conversion.hpp"
#include "Config/config.h"
#include "ABI/abi.hpp"
#include <iostream>
#include "ABI/RTOS/Types.hpp"

static inline std::string check_replacement(
	std::string identifier,
	std::map<std::string, std::string> replacements)
{
	if (replacements.contains(identifier)) {
		return replacements[identifier];
	}
	else {
		return identifier;
	}
}

namespace Converter_RVC_Cpp {

	std::string convert_function(
		AST::Function* function,
		std::string prefix,
		std::map<std::string, std::string> replacements,
		std::map<std::string, std::string> const_map,
		std::string add_args)
	{
		std::string ret = prefix;
		Config* c = Config::getInstance();
		if (c->get_target_language() == Target_Language::c) {
			ret += "static ";
		}
		ret += convert_type(&function->ret_type, "", const_map);
		ret += " " + function->name.name + "(" + add_args;
		bool first = add_args.empty();
		for (auto p : function->parameters) {
			if (!first) {
				ret += ", ";
			}
			first = false;

			ret += convert_type(&p->type, "", const_map);
			ret += " " + p->name.name;
		}

		ret += ") {\n";
		ret += prefix + "\treturn ";
		ret += convert_expression(function->expression, replacements, const_map);
		ret+= ";\n" + prefix + "}\n";
		return ret;
	}

	std::string convert_procedure(
		AST::Procedure* procedure,
		std::string prefix,
		std::map<std::string, std::string> replacements,
		std::map<std::string, std::string> const_map,
		std::string add_args)
	{
		Config* c = Config::getInstance();

		std::string ret = prefix;
		if (c->get_target_language() == Target_Language::c) {
			ret += "static ";
		}
		ret += "void ";
		ret += " " + procedure->name.name + "(" + add_args;
		bool first = add_args.empty();
		for (auto p : procedure->parameters) {
			if (!first) {
				ret += ", ";
			}
			first = false;

			ret += convert_type(&p->type, "", const_map);
			ret += " " + p->name.name;
		}

		ret += ") {\n";

		for (auto v : procedure->vars) {
			auto x = convert_vardef(v, prefix + "\t", false, replacements, const_map);
			ret += x.first;
			ret += x.second;
		}
		for (auto s : procedure->statements) {
			ret += convert_statement(s, prefix + "\t", replacements, const_map);
		}

		ret += prefix + "}\n";
		return ret;
	}

	std::string convert_nativefunction(
		AST::NativeFunction* native,
		std::string prefix,
		std::map<std::string, std::string> const_map)
	{
		std::string ret = prefix + "extern ";
		Config* c = Config::getInstance();
		if (c->get_target_language() == Target_Language::cpp) {
			ret += "\"C\" ";
		}
		ret += convert_type(&native->ret_type, "", const_map);
		ret += " " + native->name.name + "(";
		bool first = true;
		for (auto p : native->parameters) {
			if (!first) {
				ret += ", ";
			}
			first = false;

			ret += convert_type(&p->type, "", const_map);
			ret += " " + p->name.name;
		}

		ret += ");\n";
		return ret;
	}

	std::string convert_nativeprocedure(
		AST::NativeProcedure* native,
		std::string prefix,
		std::map<std::string, std::string> const_map)
	{
		std::string ret = prefix + "extern ";
		Config* c = Config::getInstance();
		if (c->get_target_language() == Target_Language::cpp) {
			ret += "\"C\" ";
		}
		ret +=" void ";
		ret += " " + native->name.name + "(";
		bool first = true;
		for (auto p : native->parameters) {
			if (!first) {
				ret += ", ";
			}
			first = false;

			ret += convert_type(&p->type, "", const_map);
			ret += " " + p->name.name;
		}

		ret += ");\n";
		return ret;
	}

	static std::string baseexpr_conversion(
		AST::BaseExpression* base,
		std::map<std::string, std::string> replacements,
		std::map<std::string, std::string> const_map)
	{
		std::string result;
		Config* c = Config::getInstance();
		if (dynamic_cast<AST::Literal*>(base) != nullptr) {
			auto l = dynamic_cast<AST::Literal*>(base);
			if (l->negation) {
				result = "-";
			}
			if (replacements.contains(l->literal)) {
				result.append(replacements[l->literal]);
			}
			else {
				result.append(l->literal);
			}
		}
		else if (dynamic_cast<AST::Identifier*>(base) != nullptr) {
			auto i = dynamic_cast<AST::Identifier*>(base);
			if (i->unary_left != nullptr) {
				result = i->unary_left->ops;
			}
			if (replacements.contains(i->identifier)) {
				result += replacements[i->identifier];
			}
			else {
				result += i->identifier;
			}
			if (i->unary_right != nullptr) {
				result += i->unary_right->ops;
			}
			for (auto x : i->indices){
				result += "[" + convert_expression(x->index, replacements, const_map) + "]";
			}
			if (i->call != nullptr) {
				result += "(";
				bool first = true;
				if (c->get_target_ABI() == Target_ABI::stdc && !const_map.contains(i->identifier)) {
					first = false;
					result += "_g";
				}

				for (auto it = i->call->parameters.begin(); it != i->call->parameters.end(); ++it) {
					if (!first) {
						result += ", ";
					}
					else {
						first = false;
					}
					result += convert_expression(*it, replacements, const_map);
				}
				result += ")";
			}
		}
		else if (dynamic_cast<AST::TernaryOperator*>(base) != nullptr) {
			auto t = dynamic_cast<AST::TernaryOperator*>(base);
			result = convert_expression(t->cond, replacements, const_map);
			result += " ? ";
			result += convert_expression(t->ifblock, replacements, const_map);
			result += " : ";
			result += convert_expression(t->elseblock, replacements, const_map);
		}
		else if (dynamic_cast<AST::Operator*>(base) != nullptr) {
			auto o = dynamic_cast<AST::Operator*>(base);
			std::string ops = o->ops;
			if (ops == "=") {
				ops = "==";
			}
			result = baseexpr_conversion(o->left, replacements, const_map) + " " + ops + " " + baseexpr_conversion(o->right, replacements, const_map);
		}
		else if (dynamic_cast<AST::Expression*>(base) != nullptr) {
			auto e = dynamic_cast<AST::Expression*>(base);
			if (e->brakets) {
				result = "(";
			}
			result += baseexpr_conversion(e->child, replacements, const_map);
			if (e->brakets) {
				result += ")";
			}
		}
		else if (dynamic_cast<AST::PortPreview*>(base) != nullptr) {
			auto p = dynamic_cast<AST::PortPreview*>(base);
			ABI_CHANNEL_PREFETCH(c, result, p->port, baseexpr_conversion(p->index->index, replacements, const_map))
		}
		else if (dynamic_cast<AST::PortSize*>(base) != nullptr) {
			auto p = dynamic_cast<AST::PortSize*>(base);
			ABI_CHANNEL_SIZE(c, result, p->port)
		}
		else if (dynamic_cast<AST::PortFree*>(base) != nullptr) {
			auto p = dynamic_cast<AST::PortFree*>(base);
			ABI_CHANNEL_FREE(c, result, p->port)
		}
		else if (dynamic_cast<AST::FSM_Enumeration_Element*>(base) != nullptr) {
			auto e = dynamic_cast<AST::FSM_Enumeration_Element*>(base);
			if (c->get_target_language() == Target_Language::cpp) {
				result = e->enum_name + "::" + e->enum_element;
			}
			else if (c->get_target_ABI() ==Target_ABI::stdc) {
				result = e->enum_name + "_" + e->enum_element;
			}
			else {
				result = e->enum_element;
			}
		}
		return result;
	}

	std::string convert_expression(
		AST::Expression* expression,
		std::map<std::string, std::string> replacements,
		std::map<std::string, std::string> const_map)
	{
		return baseexpr_conversion(expression, replacements, const_map);
	}

	static std::string convert_generator(
		AST::Generator* gen,
		std::map<std::string, std::string> replacements,
		std::string prefix,
		std::map<std::string, std::string> const_map)
	{
		std::string result = prefix + "for (";
		if (gen->type != nullptr) {
			result.append(convert_type(gen->type, gen->identifier.name, const_map));
		}
		else {
			result.append(gen->identifier.name);
		}
		result.append(" = " + convert_expression(gen->start, replacements, const_map) + "; "
			+ gen->identifier.name + " <= " + convert_expression(gen->end, replacements, const_map) + ";");
		result.append(gen->identifier.name + "++) {\n");
		return result;
	}

	std::string convert_statement(
		AST::Statement* stmt,
		std::string prefix,
		std::map<std::string, std::string> replacements,
		std::map<std::string, std::string> const_map)
	{
		std::string result;
		Config* c = Config::getInstance();
		if (dynamic_cast<AST::ForeachStatement*>(stmt) != nullptr) {
			auto f = dynamic_cast<AST::ForeachStatement*>(stmt);
			for (auto g : f->generators) {
				result.append(convert_generator(g, replacements, prefix, const_map));
			}
			for (auto v : f->vars) {
				auto tmp = convert_vardef(v, prefix + "\t", false, replacements, const_map);
				result.append(tmp.first + tmp.second);
			}
			for (auto s : f->statements) {
				result.append(convert_statement(s, prefix + "\t", replacements, const_map));
			}
			for (auto g : f->generators) {
				result.append(prefix + "}\n");
			}
		}
		else if (dynamic_cast<AST::WhileStatement*>(stmt) != nullptr) {
			auto w = dynamic_cast<AST::WhileStatement*>(stmt);
			result.append(prefix + "while (" + convert_expression(w->condition, replacements, const_map) + ") {\n");
			for (auto v : w->vars) {
				auto tmp = convert_vardef(v, prefix + "\t", false, replacements, const_map);
				result.append(tmp.first + tmp.second);
			}
			for (auto s : w->statements) {
				result.append(convert_statement(s, prefix + "\t", replacements, const_map));
			}
			result.append(prefix + "}\n");
		}
		else if (dynamic_cast<AST::AssignmentStatement*>(stmt) != nullptr) {
			auto a = dynamic_cast<AST::AssignmentStatement*>(stmt);
			result = prefix + check_replacement(a->identifier.name, replacements);
			for (auto x : a->indices) {
				result.append("[" + convert_expression(x->index, replacements, const_map) + "]");
			}

			if (dynamic_cast<AST::ListComprehension*>(a->asgnvalue) != nullptr) {
				auto tmp = convert_listcomprehension(dynamic_cast<AST::ListComprehension*>(a->asgnvalue),
					prefix, a->identifier.name, replacements, const_map);
				if (tmp.first.empty()) {
					result.append(";\n" + tmp.second);
				}
				else {
					result.append(" = " + tmp.first + ";\n");
				}
			}
			else {
				result.append(" = " + convert_expression(a->asgnvalue, replacements, const_map) + ";\n");
			}

		}
		else if (dynamic_cast<AST::BlockStatement*>(stmt) != nullptr) {
			auto b = dynamic_cast<AST::BlockStatement*>(stmt);
			result.append(prefix + "{\n");
			for (auto v : b->vars) {
				auto tmp = convert_vardef(v, prefix + "\t", false, replacements, const_map);
				result.append(tmp.first + tmp.second);
			}
			for (auto s : b->statements) {
				result.append(convert_statement(s, prefix + "\t", replacements, const_map));
			}
			result.append(prefix + "}\n");
		}
		else if (dynamic_cast<AST::CallStatement*>(stmt) != nullptr) {
			auto cs = dynamic_cast<AST::CallStatement*>(stmt);
			result = prefix + check_replacement(cs->name.name, replacements) + "(";
			bool first = true;
			if (c->get_target_ABI() == Target_ABI::stdc && !const_map.contains(cs->name.name)) {
				first = false;
				result += "_g";
			}
			for (auto p : cs->parameters) {
				if (!first) {
					result += ", ";
				}
				else {
					first = false;
				}
				result += convert_expression(p, replacements, const_map);
			}
			result += ");\n";
		}
		else if (dynamic_cast<AST::IfStatement*>(stmt) != nullptr) {
			auto i = dynamic_cast<AST::IfStatement*>(stmt);
			result = prefix + "if (" + convert_expression(i->condition, replacements, const_map) + ") {\n";
			for (auto s : i->ifblock) {
				result += convert_statement(s, prefix + "\t", replacements, const_map);
			}
			if (i->elseblock != nullptr) {
				result += prefix + "} else {\n";
				for (auto s : i->elseblock->statements) {
					result += convert_statement(s, prefix + "\t", replacements, const_map);
				}
			}
			result += prefix + "}\n";
		}
		else if (dynamic_cast<AST::OutputChannelWriteStatement*>(stmt) != nullptr) {
			auto o = dynamic_cast<AST::OutputChannelWriteStatement*>(stmt);
			std::string tmp;
			ABI_CHANNEL_WRITE(c, tmp, baseexpr_conversion(o->expr, replacements, const_map), o->port.name);
			result.append(prefix + tmp + ";\n");
		}
		else if (dynamic_cast<AST::InputChannelReadStatement*>(stmt) != nullptr) {
			auto i = dynamic_cast<AST::InputChannelReadStatement*>(stmt);
			std::string tmp;
			ABI_CHANNEL_READ(c, tmp, i->port.name);
			result.append(prefix + i->identifier.name);
			if (i->index != nullptr) {
				result.append("[" + baseexpr_conversion(i->index->index, replacements, const_map) + "]");
			}
			result.append(" = " + tmp + ";\n");
		}
		else if (dynamic_cast<AST::ReturnStatement*>(stmt) != nullptr) {
			result += prefix + "return;\n";
		}
		else if (dynamic_cast<AST::TerminateLoopStatement*>(stmt) != nullptr) {
			result += prefix + "break;\n";
		}
		return result;
	}

	std::string convert_type(
		AST::Type* type,
		std::string name,
		std::map<std::string, std::string> const_map)
	{
		Config* c = Config::getInstance();
		std::string ret;
		if (type->non_standard_type) {
			ret = type->type.name;
			/* This is usually only FSM. */
			if (c->get_target_language() == Target_Language::c) {
				ret.append("_t");
			}
			if (!name.empty()) {
				ret.append(" " + name);
			}
			return ret;
		}

		int value = 5555; /* larger than any check to go to default */
		if (type->size != nullptr) {
			value = Conversion_Helper::evaluate_constant_expression(type->size, const_map);
		}
		if (type->type.name == "list") {
			std::string ltype = convert_type(type->listtype, name, const_map);
			std::string sz = "[" + std::to_string(value) + "]";
			if (ltype.find_first_of("[") == std::string::npos) {
				return ltype + sz;
			}
			else {
				ltype.insert(ltype.find_first_of("["), sz);
				return ltype;
			}
		}
		else if (type->type.name == "uint") {
			if (value <= 8) {
				ret = "unsigned char";
			}
			else if (value <= 16) {
				ret = "unsigned short";
			}
			else if (value <= 32) {
				ret = "unsigned int";
			}
			else if (value <= 64) {
				ret = "unsigned long long";
			}
			else {
				ret = "unsigned int";
			}
		}
		else if (type->type.name == "int") {
			if (value <= 8) {
				ret = "char";
			}
			else if (value <= 16) {
				ret = "short";
			}
			else if (value <= 32) {
				ret = "int";
			}
			else if (value <= 64) {
				ret = "long long";
			}
			else {
				ret = "int";
			}
		}
		else if (type->type.name == "bool") {
			ret = "bool";
		}
		else if (type->type.name == "half") {
			if (value <= 16) {
				ret = "half";
			}
			else if (value <= 32) {
				ret = "float";
			}
			else if (value <= 64) {
				ret = "double";
			}
			else {
				ret = "half";
			}
		}
		else if (type->type.name == "float") {
			if (value <= 16) {
				ret = "half";
			}
			else if (value <= 32) {
				ret = "float";
			}
			else if (value <= 64) {
				ret = "double";
			}
			else {
				ret = "float";
			}
		}
		else if (type->type.name == "String") {
			ret = "const char*";
		}

		if (c->get_target_ABI() == Target_ABI::rtos) {
			ret = convert_rtos_type(ret);
		}

		if (!name.empty()) {
			ret.append(" " + name);
		}

		return ret;
	}

	std::pair<std::string, std::string> convert_vardef(
		AST::VarDefinition* vardef,
		std::string prefix,
		bool noinit,
		std::map<std::string, std::string> replacements,
		std::map<std::string, std::string> const_map,
		bool rtos)
	{
		std::string ret = prefix;
		std::string init;
		Config* c = Config::getInstance();
		/* this is a list comprehension. */
		std::pair<std::string, std::string> listcomp_init;
		if ((vardef->assign != nullptr) && (dynamic_cast<AST::ListComprehension*>(vardef->assign->child) != nullptr)) {
			/* C cannot initialize variables in arrays like needed for the code generated here, hence, place it in init */
			listcomp_init = convert_listcomprehension(dynamic_cast<AST::ListComprehension*>(vardef->assign->child),
				prefix, vardef->name.name, replacements, const_map, vardef->constassign);
		}

		if (rtos) {
			ret += "static ";
		}

		if (vardef->constassign && listcomp_init.second.empty()) {
			/* Noinit should only be true for actor variables in C code generation
			 * but not for variables elsewhere where no static is required
			 */
			if (noinit && !rtos) {
				ret += "static ";
			}
			ret += "const ";
		}
		ret += convert_type(&vardef->type, vardef->name.name, const_map);
		for (auto x: vardef->arrays) {
			ret += "[" + std::to_string(Conversion_Helper::evaluate_constant_expression(x, const_map)) + "]";
		}
		if (vardef->assign != nullptr) {
			if (dynamic_cast<AST::ListComprehension*>(vardef->assign->child) == nullptr) {
				if (noinit && !vardef->constassign) {
					std::string name = vardef->name.name;
					if (c->get_target_ABI() == Target_ABI::stdc) {
						name = "_g->" + name;
					}
					init = prefix + name + " = " + convert_expression(vardef->assign, replacements, const_map) + ";\n";
				}
				else {
					ret += " = " + convert_expression(vardef->assign, replacements, const_map);
				}
			}
			else {
				if (listcomp_init.first.empty()) {
					/* must be init code. */
					init = listcomp_init.second;
				}
				else {
					ret += " = " + listcomp_init.first;
				}
			}
		}
		return std::make_pair(ret + ";\n", init);
	}

	std::pair<std::string, std::string> convert_actorparam(
		AST::ActorParameter* param,
		std::string prefix,
		std::map<std::string, std::string> const_map)
	{
		std::string p = convert_type(&param->type, param->name.name, const_map);
		std::string i;
		if (param->asign != nullptr) {
			i = convert_expression(param->asign, std::map<std::string, std::string>(), const_map);
		}
		return std::make_pair(prefix + p +";\n", i);
	}

	std::pair<std::string, std::string> convert_listcomprehension(
		AST::ListComprehension* outexpr,
		std::string prefix,
		std::string assign_var,
		std::map<std::string, std::string> replacements,
		std::map<std::string, std::string> const_map,
		bool constasgn)
	{
		Config* c = Config::getInstance();
		bool contains_generator = !outexpr->generators.empty();
		bool contains_lstcomp = false;
		for (auto e = outexpr->expressions.begin(); e != outexpr->expressions.end(); ++e) {
			if (dynamic_cast<AST::ListComprehension*>((*e)->child) != nullptr) {
				contains_lstcomp = true;
			}
		}

		if (!contains_lstcomp && !contains_generator && (constasgn || (c->get_target_language() == Target_Language::cpp))) {
			/* This is only a plain list we can initialize directly if not C because struct members cannot be initialized like this */
			std::string result = "{";
			bool first = true;
			for (auto x : outexpr->expressions) {
				if (!first) {
					result += ", ";
				}
				first = false;
				result += convert_expression(x, replacements, const_map);
			}
			result += "}";

			return std::make_pair(result, "");
		}

#if 0
		/* This is only true for constructors but not for other functions, hence, omit it for now. It is only code beautification ... */
		if (c->get_target_language() == Target_Language::cpp) {
			/* construction also has indent for class */
			prefix += "\t";
		}
#endif
		if (c->get_target_ABI() == Target_ABI::stdc && !constasgn) {
			/* Next step will generate init code, hence, the variable will later be stored inside the struct */
			assign_var = "_g->" + assign_var;
		}

		std::string result = prefix + "{\n";
		std::string iterator_var = unused_identifier();
		result += prefix + "\t" + "unsigned " + iterator_var + " = 0;\n";
		std::string generators;
		for (auto g : outexpr->generators) {
			generators.append(convert_generator(g, replacements, prefix + "\t", const_map));
		}

		result += generators;
		if (contains_lstcomp) {
			for (auto e : outexpr->expressions) {
				if (dynamic_cast<AST::ListComprehension*>(e->child) != nullptr) {
					result += convert_listcomprehension(dynamic_cast<AST::ListComprehension*>(e->child), prefix + "\t",
						assign_var + "[" + iterator_var + "]", replacements, const_map, true).second;
					/* The noinit = true makes the first part empty, hence, adding only second is sufficient. */
				}
				else {
					result += prefix + "\t" + assign_var + "[" + iterator_var + "] = " + convert_expression(e, replacements, const_map) + ";\n";
				}
				result.append(prefix + "\t" + "++" + iterator_var + ";\n");
			}
		}
		else {
			for (auto e : outexpr->expressions) {
				result += prefix + "\t" + assign_var + "[" + iterator_var + "++] = " + convert_expression(e, replacements, const_map) + ";\n";
			}

		}
		for (unsigned i = 0; i < outexpr->generators.size(); ++i) {
			result.append(prefix + "\t}\n");
		}
		result += prefix + "}\n";
		return std::make_pair("", result);
	}

	std::string unused_identifier(void)
	{
		static unsigned counter = 1;

		return "_" + std::to_string(counter++);
	}
}