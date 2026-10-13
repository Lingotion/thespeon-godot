// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
// The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

#ifndef THESPEON_NODE_H
#define THESPEON_NODE_H

#include "thespeon_audio_stream.h"
#include "thespeon_input.h"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <memory>
#include <thread>
#include <vector>

struct thespeon_engine;

namespace godot {

// Every call except synthesize_stream blocks on the calling thread, so calling one
// from the main thread freezes the game until it finishes. Models stay loaded in
// the node's engine between calls.
class Thespeon : public Node {
	GDCLASS(Thespeon, Node)

	// Created on first use, so that a missing runtime-files directory is reported
	// by the call that needs it rather than at construction.
	thespeon_engine *engine = nullptr;

	// One per synthesize_stream call, touched only on the main thread. The job
	// holds the stream by ID so that dropping the stream cancels its synthesis.
	struct StreamJob {
		std::thread thread;
		std::shared_ptr<ThespeonStreamState> state;
		uint64_t stream_id = 0;
	};
	std::vector<std::unique_ptr<StreamJob>> stream_jobs;

	thespeon_engine *ensure_engine();
	// Emits the stream signals and reaps finished workers; workers defer a call
	// to it whenever they have something to report.
	void poll_streams();

protected:
	static void _bind_methods();

public:
	~Thespeon();

	// Runtime ABI version of thespeon.dll, for comparison with the header's
	// THESPEON_ABI_VERSION.
	int get_abi_version() const;

	// characterModules, languageModules, and the problems of any config that could
	// not be read. Characters carry only an emotionCount; see
	// get_emotions_for_character.
	Dictionary list_models();
	// The emotions a character accepts, as {name, guide}. model is a module
	// identifier or a character name; a name selects the largest, then newest,
	// match, narrowed by module_type and module_version when they are not "".
	// Empty, with the error printed, when nothing matches.
	Array get_emotions_for_character(const String &p_model, const String &p_module_type,
			const String &p_module_version);
	// As list_models, restricted to the modules whose sessions are resident, and
	// without problems.
	Dictionary list_loaded();

	// OK when synthesis would accept the input, including every emotion it names
	// being one the character lists. Runs no model, so a word the language module
	// cannot pronounce is not caught.
	Error validate_input(const Ref<ThespeonInput> &p_input);

	// A ThespeonInput must not be used from two threads at once.
	Error synthesize_wav(const Ref<ThespeonInput> &p_input, const String &p_output_path);

	// Returns at once and synthesizes on a worker thread; the stream plays as audio
	// arrives. Its signals are emitted on the main thread, no earlier than the next
	// idle frame, so connecting right after this call misses nothing. Freeing this
	// node cancels every stream it started. The input is copied, so it may be changed
	// or reused straight away.
	Ref<ThespeonAudioStream> synthesize_stream(const Ref<ThespeonInput> &p_input);

	// Loads a character module ahead of its first synthesis. With warmup, also runs
	// a throwaway synthesis so the first real one reaches steady-state latency.
	Error preload_module(const String &p_module_identifier, bool p_warmup);
	Error unload_module(const String &p_module_identifier);
	// As preload_module and unload_module, for the character and languages the input
	// selects.
	Error preload_input(const Ref<ThespeonInput> &p_input, bool p_warmup);
	Error unload_input(const Ref<ThespeonInput> &p_input);
	// module_identifier names a character or a language module. A character counts
	// as loaded whatever languages it was preloaded with.
	bool is_loaded(const String &p_module_identifier);
	// Whether preload_input would load nothing.
	bool is_input_loaded(const Ref<ThespeonInput> &p_input);

	// Re-reads the catalog after ThespeonImporter has imported or deleted modules.
	Error refresh();
	int64_t get_resident_bytes();
};

} // namespace godot

#endif // THESPEON_NODE_H
