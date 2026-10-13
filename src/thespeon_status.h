// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
// The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

#ifndef THESPEON_STATUS_H
#define THESPEON_STATUS_H

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/string.hpp>

#include <thespeon.h>

namespace godot {

inline Error thespeon_status_to_error(thespeon_status p_status) {
	switch (p_status) {
		case THESPEON_STATUS_OK:
			return OK;
		case THESPEON_STATUS_INVALID_ARGUMENT:
			return ERR_INVALID_PARAMETER;
		case THESPEON_STATUS_CANCELLED:
			return ERR_SKIP;
		default:
			return FAILED;
	}
}

// Must run straight after the failing call: the error text is thread-local and valid
// only until the next operation on this thread.
inline void thespeon_print_error(const char *p_call, thespeon_status p_status) {
	const char *message = thespeon_get_last_error();
	ERR_PRINT(vformat("%s failed (status %d): %s", p_call, p_status,
			message ? String::utf8(message) : String("no error text")));
}

} // namespace godot

#endif // THESPEON_STATUS_H
