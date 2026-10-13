// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
// The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

#include "thespeon_catalog.h"

#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/variant.hpp>

using namespace godot;

namespace {

// Catalog strings are never null; unset fields are "" and are left out.
void set_if_present(Dictionary &r_dictionary, const char *p_key, const char *p_value) {
	if (p_value && p_value[0] != '\0') {
		r_dictionary[p_key] = String::utf8(p_value);
	}
}

Array languages_to_array(const thespeon_module_language *p_languages, uint64_t p_count) {
	Array languages;
	for (uint64_t i = 0; i < p_count; ++i) {
		const thespeon_module_language &language = p_languages[i];
		Dictionary dictionary;
		set_if_present(dictionary, "iso639_2", language.iso639_2);
		set_if_present(dictionary, "iso639_3", language.iso639_3);
		set_if_present(dictionary, "glottocode", language.glottocode);
		set_if_present(dictionary, "iso3166_1", language.iso3166_1);
		set_if_present(dictionary, "iso3166_2", language.iso3166_2);
		set_if_present(dictionary, "custom_dialect", language.custom_dialect);
		set_if_present(dictionary, "name_in_english", language.name_in_english);
		set_if_present(dictionary, "autonym", language.autonym);
		languages.push_back(dictionary);
	}
	return languages;
}

} // namespace

String godot::thespeon_version_to_string(const thespeon_version &p_version) {
	return vformat("%d.%d.%d", p_version.major, p_version.minor, p_version.patch);
}

Array godot::thespeon_character_modules_to_array(const thespeon_character_module *p_modules,
		uint64_t p_count) {
	Array modules;
	for (uint64_t i = 0; i < p_count; ++i) {
		const thespeon_character_module &module = p_modules[i];
		Dictionary dictionary;
		dictionary["identifier"] = String::utf8(module.identifier);
		dictionary["name"] = String::utf8(module.name);
		dictionary["moduleType"] = String::utf8(module.module_type);
		dictionary["version"] = thespeon_version_to_string(module.version);
		dictionary["languages"] = languages_to_array(module.languages, module.language_count);
		// The emotions themselves come from Thespeon::get_emotions_for_character.
		dictionary["emotionCount"] = static_cast<int64_t>(module.emotion_count);
		modules.push_back(dictionary);
	}
	return modules;
}

Array godot::thespeon_language_modules_to_array(const thespeon_language_module *p_modules,
		uint64_t p_count) {
	Array modules;
	for (uint64_t i = 0; i < p_count; ++i) {
		const thespeon_language_module &module = p_modules[i];
		Dictionary dictionary;
		dictionary["identifier"] = String::utf8(module.identifier);
		dictionary["name"] = String::utf8(module.name);
		dictionary["version"] = thespeon_version_to_string(module.version);
		dictionary["languages"] = languages_to_array(module.languages, module.language_count);
		modules.push_back(dictionary);
	}
	return modules;
}

Dictionary godot::thespeon_catalog_to_dictionary(const thespeon_catalog &p_catalog) {
	PackedStringArray problems;
	for (uint64_t i = 0; i < p_catalog.problem_count; ++i) {
		problems.push_back(String::utf8(p_catalog.problems[i]));
	}

	Dictionary result;
	result["characterModules"] = thespeon_character_modules_to_array(
			p_catalog.character_modules, p_catalog.character_module_count);
	result["languageModules"] = thespeon_language_modules_to_array(
			p_catalog.language_modules, p_catalog.language_module_count);
	result["problems"] = problems;
	return result;
}

Dictionary godot::thespeon_delete_result_to_dictionary(Error p_error,
		const thespeon_delete_result &p_result) {
	Dictionary dictionary;
	dictionary["status"] = p_error;
	dictionary["binaries_removed"] = static_cast<int64_t>(p_result.binaries_removed);
	dictionary["bytes_freed"] = static_cast<int64_t>(p_result.bytes_freed);
	dictionary["binaries_kept"] = static_cast<int64_t>(p_result.binaries_kept);
	dictionary["stranded_characters"] = static_cast<int64_t>(p_result.stranded_characters);
	dictionary["problems"] = static_cast<int64_t>(p_result.problems);
	dictionary["swept"] = p_result.swept != 0;
	return dictionary;
}
