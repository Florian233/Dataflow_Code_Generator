#include "Unsupported.hpp"

/* The corresponding endX are not checked here as there must be the X before that should trigger the exception. */
void Converter_RVC_Cpp::construct_supported(Context context, std::string& t) {
	if (context == Context::None) {
		return;
	}

	if (context == Context::Import) {
		if (t == "=") {
			throw Unsupported_Feature_Exception{ "Importing an entity with assigning a different identifier is not supported." };
		}
	}
	else if (context == Context::Actor_Head) {
		if (t == "[") {
			throw Unsupported_Feature_Exception{ "Type interference for actors is not supported." };
		}
		else if (t == "multi") {
			throw Unsupported_Feature_Exception{ "Multichannels are not supported." };
		}
	}
	else if (context == Context::Actor_Body) {
		if ((t == "choose") || (t == "lambda") || (t == "map")
			|| (t == "set") || (t == "regexp") || (t == "time")
			|| (t == "old") || (t == "mutable") || (t == "let"))
		{
			throw Unsupported_Feature_Exception{ "Unsupported Feature detected: " + t };
		}
	}
	else if (context == Context::Action_Head) {
		if ((t == "at") || (t == "all") || (t == "time") || (t == "map")
			|| (t == "set") || (t == "old") || (t == "mutable") || (t == "let"))
		{
			throw Unsupported_Feature_Exception{ "Unsupported Feature detected: " + t };
		}
	}
	else if (context == Context::Action_Body) {
		if ((t == "choose") || (t == "old") || (t == "mutable")) {
			throw Unsupported_Feature_Exception{ "Unsupported Feature detected: " + t };
		}
	}
}