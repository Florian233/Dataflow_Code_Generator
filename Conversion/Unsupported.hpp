#pragma once

#include <string>
#include "Exceptions.hpp"

namespace Conversion_Helper {

	/* Context of the tokens that shall be checked whether the contain unsupported keyword. 
	 * Context is required as keywords can mean different things in different contexts.
	 */
	enum class Context {
		Import,
		Actor_Head,
		Actor_Body,
		Action_Head,
		Action_Body,
		None /* For compatibility reasons - Checks are disabled. */
	};

	/* Function throws an Unsupported Feature Exception if the keyword t in not support in context c,
	 * can be used to check whether the intended supported feature is parsed.
	 * Use None as Context if no checking shall be done.
	 */
	void construct_supported(Context c, std::string& t);
}