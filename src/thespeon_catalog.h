// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
// The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

#ifndef THESPEON_CATALOG_H
#define THESPEON_CATALOG_H

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <thespeon.h>

// Converts libthespeon's catalog and delete results into Variants, shared by the
// Thespeon node and ThespeonImporter. Keys follow libthespeon's former catalog
// JSON, so scripts written against it keep working.

namespace godot {

String thespeon_version_to_string(const thespeon_version &p_version);

Array thespeon_character_modules_to_array(const thespeon_character_module *p_modules,
		uint64_t p_count);
Array thespeon_language_modules_to_array(const thespeon_language_module *p_modules,
		uint64_t p_count);

// characterModules, languageModules and problems.
Dictionary thespeon_catalog_to_dictionary(const thespeon_catalog &p_catalog);

// The thespeon_delete_result fields plus "status".
Dictionary thespeon_delete_result_to_dictionary(Error p_error,
		const thespeon_delete_result &p_result);

} // namespace godot

#endif // THESPEON_CATALOG_H
