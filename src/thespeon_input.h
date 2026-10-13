// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
// The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

#ifndef THESPEON_INPUT_H
#define THESPEON_INPUT_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <thespeon.h>

namespace godot {

// One synthesis input, built with thespeon_input's typed setters. Starts as
// version 1.0.0 with English as the default language and no segments.
//
// Setters reject only malformed arguments. Schema errors, such as a missing
// character, show up from to_json() and the calls that take the input.
class ThespeonInput : public RefCounted {
	GDCLASS(ThespeonInput, RefCounted)

	thespeon_input *input = nullptr;

protected:
	static void _bind_methods();

public:
	enum Endpoint {
		ENDPOINT_START = THESPEON_ENDPOINT_START,
		ENDPOINT_END = THESPEON_ENDPOINT_END,
	};

	ThespeonInput();
	~ThespeonInput();

	const thespeon_input *get_handle() const { return input; }
	// A validated, independent copy, for a call that runs on another thread while
	// this input stays free to change. Null, with the error printed, on failure.
	thespeon_input *clone_handle() const;

	// Null, with the error printed, when the document is invalid.
	static Ref<ThespeonInput> from_json(const String &p_json);
	// Validates the input as the synthesis calls would; "" on error.
	String to_json() const;

	// These two replace each other.
	Error set_character_module(const String &p_module_identifier);
	Error set_character(const String &p_character_name, const String &p_module_type,
			const String &p_module_version);

	// "" unsets.
	Error set_default_emotion(const String &p_emotion);
	// Keys: iso639_2 (required), iso639_3, glottocode, iso3166_1, iso3166_2,
	// custom_dialect.
	Error set_default_language(const Dictionary &p_language);

	// Returns the new segment's index, or -1 on error.
	int64_t add_segment(const String &p_text);
	// The segment's text is then IPA.
	Error segment_set_custom_pronounced(int64_t p_segment, bool p_enabled);
	// An empty Dictionary unsets.
	Error segment_set_language(int64_t p_segment, const Dictionary &p_language);
	// Single-name shorthand for any endpoint without a blend. "" unsets.
	Error segment_set_emotion(int64_t p_segment, const String &p_emotion);
	// Adds one emotion to an endpoint's blend; the same name overwrites.
	Error segment_add_emotion(int64_t p_segment, Endpoint p_at, const String &p_emotion,
			double p_weight);
	Error segment_set_speed(int64_t p_segment, Endpoint p_at, double p_speed);
	Error segment_set_loudness(int64_t p_segment, Endpoint p_at, double p_loudness);
};

} // namespace godot

VARIANT_ENUM_CAST(ThespeonInput::Endpoint);

#endif // THESPEON_INPUT_H
