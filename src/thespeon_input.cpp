// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
// The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

#include "thespeon_input.h"

#include "thespeon_status.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace {

Error to_error(const char *p_call, thespeon_status p_status) {
	if (p_status != THESPEON_STATUS_OK) {
		thespeon_print_error(p_call, p_status);
	}
	return thespeon_status_to_error(p_status);
}

// Keeps the UTF-8 copies alive for as long as the thespeon_language points at them.
class Language {
	CharString iso639_2, iso639_3, glottocode, iso3166_1, iso3166_2, custom_dialect;

	static const char *field(const Dictionary &p_language, const char *p_key,
			CharString &r_storage) {
		if (!p_language.has(p_key)) {
			return nullptr;
		}
		r_storage = String(p_language[p_key]).utf8();
		return r_storage.get_data();
	}

public:
	thespeon_language value{};

	explicit Language(const Dictionary &p_language) {
		value.iso639_2 = field(p_language, "iso639_2", iso639_2);
		value.iso639_3 = field(p_language, "iso639_3", iso639_3);
		value.glottocode = field(p_language, "glottocode", glottocode);
		value.iso3166_1 = field(p_language, "iso3166_1", iso3166_1);
		value.iso3166_2 = field(p_language, "iso3166_2", iso3166_2);
		value.custom_dialect = field(p_language, "custom_dialect", custom_dialect);
	}
};

bool check_segment(int64_t p_segment) {
	ERR_FAIL_COND_V_MSG(p_segment < 0, false, "Segment index must not be negative");
	return true;
}

} // namespace

void ThespeonInput::_bind_methods() {
	ClassDB::bind_static_method("ThespeonInput", D_METHOD("from_json", "json"),
			&ThespeonInput::from_json);
	ClassDB::bind_method(D_METHOD("to_json"), &ThespeonInput::to_json);

	ClassDB::bind_method(D_METHOD("set_character_module", "module_identifier"),
			&ThespeonInput::set_character_module);
	ClassDB::bind_method(
			D_METHOD("set_character", "character_name", "module_type", "module_version"),
			&ThespeonInput::set_character, DEFVAL(String()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("set_default_emotion", "emotion"),
			&ThespeonInput::set_default_emotion);
	ClassDB::bind_method(D_METHOD("set_default_language", "language"),
			&ThespeonInput::set_default_language);

	ClassDB::bind_method(D_METHOD("add_segment", "text"), &ThespeonInput::add_segment);
	ClassDB::bind_method(D_METHOD("segment_set_custom_pronounced", "segment", "enabled"),
			&ThespeonInput::segment_set_custom_pronounced, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("segment_set_language", "segment", "language"),
			&ThespeonInput::segment_set_language);
	ClassDB::bind_method(D_METHOD("segment_set_emotion", "segment", "emotion"),
			&ThespeonInput::segment_set_emotion);
	ClassDB::bind_method(D_METHOD("segment_add_emotion", "segment", "at", "emotion", "weight"),
			&ThespeonInput::segment_add_emotion);
	ClassDB::bind_method(D_METHOD("segment_set_speed", "segment", "at", "speed"),
			&ThespeonInput::segment_set_speed);
	ClassDB::bind_method(D_METHOD("segment_set_loudness", "segment", "at", "loudness"),
			&ThespeonInput::segment_set_loudness);

	BIND_ENUM_CONSTANT(ENDPOINT_START);
	BIND_ENUM_CONSTANT(ENDPOINT_END);
}

ThespeonInput::ThespeonInput() {
	const thespeon_status status = thespeon_input_create(&input);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_input_create", status);
		input = nullptr;
	}
}

ThespeonInput::~ThespeonInput() {
	thespeon_input_destroy(input);
}

thespeon_input *ThespeonInput::clone_handle() const {
	char *json = nullptr;
	thespeon_status status = thespeon_input_to_json(input, &json);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_input_to_json", status);
		return nullptr;
	}

	thespeon_input *copy = nullptr;
	status = thespeon_input_from_json(json, &copy);
	thespeon_string_free(json);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_input_from_json", status);
		return nullptr;
	}
	return copy;
}

Ref<ThespeonInput> ThespeonInput::from_json(const String &p_json) {
	thespeon_input *handle = nullptr;
	const thespeon_status status = thespeon_input_from_json(p_json.utf8().get_data(), &handle);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_input_from_json", status);
		return Ref<ThespeonInput>();
	}

	Ref<ThespeonInput> result;
	result.instantiate();
	thespeon_input_destroy(result->input);
	result->input = handle;
	return result;
}

String ThespeonInput::to_json() const {
	char *json = nullptr;
	const thespeon_status status = thespeon_input_to_json(input, &json);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_input_to_json", status);
		return String();
	}

	const String result = String::utf8(json);
	thespeon_string_free(json);
	return result;
}

Error ThespeonInput::set_character_module(const String &p_module_identifier) {
	return to_error("thespeon_input_set_character_module",
			thespeon_input_set_character_module(input, p_module_identifier.utf8().get_data()));
}

Error ThespeonInput::set_character(const String &p_character_name, const String &p_module_type,
		const String &p_module_version) {
	const CharString module_type = p_module_type.utf8();
	const CharString module_version = p_module_version.utf8();
	return to_error("thespeon_input_set_character",
			thespeon_input_set_character(input, p_character_name.utf8().get_data(),
					p_module_type.is_empty() ? nullptr : module_type.get_data(),
					p_module_version.is_empty() ? nullptr : module_version.get_data()));
}

Error ThespeonInput::set_default_emotion(const String &p_emotion) {
	const CharString emotion = p_emotion.utf8();
	return to_error("thespeon_input_set_default_emotion",
			thespeon_input_set_default_emotion(input,
					p_emotion.is_empty() ? nullptr : emotion.get_data()));
}

Error ThespeonInput::set_default_language(const Dictionary &p_language) {
	const Language language(p_language);
	return to_error("thespeon_input_set_default_language",
			thespeon_input_set_default_language(input, &language.value));
}

int64_t ThespeonInput::add_segment(const String &p_text) {
	uint64_t index = 0;
	const thespeon_status status = thespeon_input_add_segment(
			input, p_text.utf8().get_data(), &index);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_input_add_segment", status);
		return -1;
	}
	return static_cast<int64_t>(index);
}

Error ThespeonInput::segment_set_custom_pronounced(int64_t p_segment, bool p_enabled) {
	if (!check_segment(p_segment)) {
		return ERR_INVALID_PARAMETER;
	}
	return to_error("thespeon_input_segment_set_custom_pronounced",
			thespeon_input_segment_set_custom_pronounced(
					input, uint64_t(p_segment), p_enabled ? 1 : 0));
}

Error ThespeonInput::segment_set_language(int64_t p_segment, const Dictionary &p_language) {
	if (!check_segment(p_segment)) {
		return ERR_INVALID_PARAMETER;
	}
	const Language language(p_language);
	return to_error("thespeon_input_segment_set_language",
			thespeon_input_segment_set_language(input, uint64_t(p_segment),
					p_language.is_empty() ? nullptr : &language.value));
}

Error ThespeonInput::segment_set_emotion(int64_t p_segment, const String &p_emotion) {
	if (!check_segment(p_segment)) {
		return ERR_INVALID_PARAMETER;
	}
	const CharString emotion = p_emotion.utf8();
	return to_error("thespeon_input_segment_set_emotion",
			thespeon_input_segment_set_emotion(input, uint64_t(p_segment),
					p_emotion.is_empty() ? nullptr : emotion.get_data()));
}

Error ThespeonInput::segment_add_emotion(int64_t p_segment, Endpoint p_at,
		const String &p_emotion, double p_weight) {
	if (!check_segment(p_segment)) {
		return ERR_INVALID_PARAMETER;
	}
	return to_error("thespeon_input_segment_add_emotion",
			thespeon_input_segment_add_emotion(input, uint64_t(p_segment),
					thespeon_endpoint(p_at), p_emotion.utf8().get_data(), p_weight));
}

Error ThespeonInput::segment_set_speed(int64_t p_segment, Endpoint p_at, double p_speed) {
	if (!check_segment(p_segment)) {
		return ERR_INVALID_PARAMETER;
	}
	return to_error("thespeon_input_segment_set_speed",
			thespeon_input_segment_set_speed(input, uint64_t(p_segment),
					thespeon_endpoint(p_at), p_speed));
}

Error ThespeonInput::segment_set_loudness(int64_t p_segment, Endpoint p_at, double p_loudness) {
	if (!check_segment(p_segment)) {
		return ERR_INVALID_PARAMETER;
	}
	return to_error("thespeon_input_segment_set_loudness",
			thespeon_input_segment_set_loudness(input, uint64_t(p_segment),
					thespeon_endpoint(p_at), p_loudness));
}
