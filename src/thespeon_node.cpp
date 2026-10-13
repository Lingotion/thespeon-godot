// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
// The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

#include "thespeon_node.h"

#include "thespeon_catalog.h"
#include "thespeon_constants.h"
#include "thespeon_status.h"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <thespeon.h>

#include <cstring>

using namespace godot;

namespace {

// --- character lookup -----------------------------------------------------------

// Ranks module types XS through XL; -1 for anything else.
int module_type_rank(const char *p_module_type) {
	static const char *const TYPES[] = { "XS", "S", "M", "L", "XL" };
	for (int i = 0; i < 5; ++i) {
		if (std::strcmp(p_module_type, TYPES[i]) == 0) {
			return i;
		}
	}
	return -1;
}

bool is_newer(const thespeon_version &p_a, const thespeon_version &p_b) {
	if (p_a.major != p_b.major) {
		return p_a.major > p_b.major;
	}
	if (p_a.minor != p_b.minor) {
		return p_a.minor > p_b.minor;
	}
	return p_a.patch > p_b.patch;
}

// Selects as libthespeon's CLI does: an exact identifier wins; otherwise the
// largest, then newest, module with that character name, narrowed by module type
// and version when given. Null when nothing matches.
const thespeon_character_module *find_character_module(const thespeon_catalog &p_catalog,
		const String &p_model, const String &p_module_type, const String &p_module_version) {
	// Case-insensitive, as libthespeon matches them.
	const thespeon_character_module *best = nullptr;
	for (uint64_t i = 0; i < p_catalog.character_module_count; ++i) {
		const thespeon_character_module &module = p_catalog.character_modules[i];
		if (p_model.nocasecmp_to(String::utf8(module.identifier)) == 0) {
			return &module;
		}
		if (p_model.nocasecmp_to(String::utf8(module.name)) != 0 ||
				(!p_module_type.is_empty() &&
						p_module_type.nocasecmp_to(String::utf8(module.module_type)) != 0) ||
				(!p_module_version.is_empty() &&
						p_module_version != thespeon_version_to_string(module.version))) {
			continue;
		}
		const int rank = module_type_rank(module.module_type);
		const int best_rank = best ? module_type_rank(best->module_type) : -2;
		if (rank > best_rank || (rank == best_rank && is_newer(module.version, best->version))) {
			best = &module;
		}
	}
	return best;
}

Error to_error(const char *p_call, thespeon_status p_status) {
	if (p_status != THESPEON_STATUS_OK) {
		thespeon_print_error(p_call, p_status);
	}
	return thespeon_status_to_error(p_status);
}

// --- streaming worker ---------------------------------------------------------
// Runs off the main thread, so it touches only the shared state and the notify
// Callable, never a Godot object.

struct StreamContext {
	ThespeonStreamState *state;
	const Callable *notify;
};

void on_stream_audio(const float *p_samples, uint64_t p_count, uint8_t p_is_final,
		void *p_user_data) {
	ThespeonStreamState *state = static_cast<StreamContext *>(p_user_data)->state;
	std::lock_guard<std::mutex> lock(state->mutex);
	state->samples.insert(state->samples.end(), p_samples, p_samples + p_count);
	state->audio_complete = state->audio_complete || p_is_final != 0;
}

void on_stream_samples(const int64_t *p_indices, uint64_t p_count, void *p_user_data) {
	const StreamContext *context = static_cast<StreamContext *>(p_user_data);
	{
		std::lock_guard<std::mutex> lock(context->state->mutex);
		context->state->pending_sample_requests.emplace_back(p_indices, p_indices + p_count);
	}
	context->notify->call_deferred();
}

// Takes ownership of p_input, a copy made for this thread alone.
void run_stream(thespeon_engine *p_engine, thespeon_input *p_input,
		std::shared_ptr<ThespeonStreamState> p_state, Callable p_notify) {
	StreamContext context{ p_state.get(), &p_notify };
	const thespeon_status status = thespeon_engine_synthesize_stream_ex(p_engine,
			p_input, on_stream_audio, on_stream_samples, &context, p_state->cancel);
	thespeon_input_destroy(p_input);

	// The error text is thread-local, so it has to be read here.
	if (status != THESPEON_STATUS_OK && status != THESPEON_STATUS_CANCELLED) {
		thespeon_print_error("thespeon_engine_synthesize_stream_ex", status);
	}

	{
		std::lock_guard<std::mutex> lock(p_state->mutex);
		p_state->status = status;
		p_state->done = true;
	}
	p_notify.call_deferred();
}

} // namespace

void Thespeon::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_abi_version"), &Thespeon::get_abi_version);
	ClassDB::bind_method(D_METHOD("list_models"), &Thespeon::list_models);
	ClassDB::bind_method(
			D_METHOD("get_emotions_for_character", "model", "module_type", "module_version"),
			&Thespeon::get_emotions_for_character, DEFVAL(String()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("list_loaded"), &Thespeon::list_loaded);
	ClassDB::bind_method(D_METHOD("validate_input", "input"), &Thespeon::validate_input);
	ClassDB::bind_method(D_METHOD("synthesize_wav", "input", "output_path"),
			&Thespeon::synthesize_wav);
	ClassDB::bind_method(D_METHOD("synthesize_stream", "input"),
			&Thespeon::synthesize_stream);
	ClassDB::bind_method(D_METHOD("preload_module", "module_identifier", "warmup"),
			&Thespeon::preload_module, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("unload_module", "module_identifier"), &Thespeon::unload_module);
	ClassDB::bind_method(D_METHOD("preload_input", "input", "warmup"),
			&Thespeon::preload_input, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("unload_input", "input"), &Thespeon::unload_input);
	ClassDB::bind_method(D_METHOD("is_loaded", "module_identifier"), &Thespeon::is_loaded);
	ClassDB::bind_method(D_METHOD("is_input_loaded", "input"), &Thespeon::is_input_loaded);
	ClassDB::bind_method(D_METHOD("refresh"), &Thespeon::refresh);
	ClassDB::bind_method(D_METHOD("get_resident_bytes"), &Thespeon::get_resident_bytes);
}

Thespeon::~Thespeon() {
	// The engine must not be destroyed while a worker is still using it.
	for (const std::unique_ptr<StreamJob> &job : stream_jobs) {
		thespeon_cancel_token_cancel(job->state->cancel);
	}
	for (const std::unique_ptr<StreamJob> &job : stream_jobs) {
		job->thread.join();
	}
	stream_jobs.clear();
	thespeon_engine_destroy(engine);
}

thespeon_engine *Thespeon::ensure_engine() {
	if (engine) {
		return engine;
	}

	const String data_directory = ProjectSettings::get_singleton()->globalize_path(
			thespeon_constants::RUNTIME_FILES_PATH);
	const thespeon_status status = thespeon_engine_create(
			data_directory.utf8().get_data(), &engine);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_engine_create", status);
		engine = nullptr;
	}
	return engine;
}

int Thespeon::get_abi_version() const {
	return static_cast<int>(thespeon_get_abi_version());
}

Dictionary Thespeon::list_models() {
	thespeon_engine *current = ensure_engine();
	if (!current) {
		return Dictionary();
	}

	thespeon_catalog *catalog = nullptr;
	const thespeon_status status = thespeon_engine_list_models(current, &catalog);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_engine_list_models", status);
		return Dictionary();
	}

	const Dictionary result = thespeon_catalog_to_dictionary(*catalog);
	thespeon_catalog_free(catalog);
	return result;
}

Array Thespeon::get_emotions_for_character(const String &p_model, const String &p_module_type,
		const String &p_module_version) {
	thespeon_engine *current = ensure_engine();
	if (!current) {
		return Array();
	}

	thespeon_catalog *catalog = nullptr;
	const thespeon_status status = thespeon_engine_list_models(current, &catalog);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_engine_list_models", status);
		return Array();
	}

	Array emotions;
	const thespeon_character_module *module = find_character_module(*catalog, p_model,
			p_module_type, p_module_version);
	if (module) {
		for (uint64_t i = 0; i < module->emotion_count; ++i) {
			Dictionary emotion;
			emotion["name"] = String::utf8(module->emotions[i].name);
			emotion["guide"] = String::utf8(module->emotions[i].guide);
			emotions.push_back(emotion);
		}
	}
	thespeon_catalog_free(catalog);

	ERR_FAIL_NULL_V_MSG(module, Array(), vformat("No character module matches '%s'", p_model));
	return emotions;
}

Dictionary Thespeon::list_loaded() {
	thespeon_engine *current = ensure_engine();
	if (!current) {
		return Dictionary();
	}

	thespeon_module_list *modules = nullptr;
	const thespeon_status status = thespeon_engine_list_loaded(current, &modules);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_engine_list_loaded", status);
		return Dictionary();
	}

	Dictionary result;
	result["characterModules"] = thespeon_character_modules_to_array(
			modules->character_modules, modules->character_module_count);
	result["languageModules"] = thespeon_language_modules_to_array(
			modules->language_modules, modules->language_module_count);
	thespeon_module_list_free(modules);
	return result;
}

Error Thespeon::validate_input(const Ref<ThespeonInput> &p_input) {
	ERR_FAIL_COND_V_MSG(p_input.is_null(), ERR_INVALID_PARAMETER, "input is null");
	thespeon_engine *current = ensure_engine();
	if (!current) {
		return FAILED;
	}

	return to_error("thespeon_engine_validate_input",
			thespeon_engine_validate_input(current, p_input->get_handle()));
}

Error Thespeon::synthesize_wav(const Ref<ThespeonInput> &p_input, const String &p_output_path) {
	ERR_FAIL_COND_V_MSG(p_input.is_null(), ERR_INVALID_PARAMETER, "input is null");
	thespeon_engine *current = ensure_engine();
	if (!current) {
		return FAILED;
	}

	const String output_path = ProjectSettings::get_singleton()->globalize_path(p_output_path);
	return to_error("thespeon_engine_synthesize_wav",
			thespeon_engine_synthesize_wav(current,
					p_input->get_handle(),
					output_path.utf8().get_data(),
					nullptr));
}

Ref<ThespeonAudioStream> Thespeon::synthesize_stream(const Ref<ThespeonInput> &p_input) {
	ERR_FAIL_COND_V_MSG(p_input.is_null(), Ref<ThespeonAudioStream>(), "input is null");
	thespeon_engine *current = ensure_engine();
	if (!current) {
		return Ref<ThespeonAudioStream>();
	}

	std::shared_ptr<ThespeonStreamState> state = std::make_shared<ThespeonStreamState>();
	const thespeon_status status = thespeon_cancel_token_create(&state->cancel);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_cancel_token_create", status);
		return Ref<ThespeonAudioStream>();
	}

	// A handle must not be used from two threads at once, so the worker gets its
	// own copy and the caller may keep changing or reusing theirs. Copying also
	// validates, so a bad input fails here rather than on the worker.
	thespeon_input *input = p_input->clone_handle();
	if (!input) {
		return Ref<ThespeonAudioStream>();
	}

	Ref<ThespeonAudioStream> stream;
	stream.instantiate();
	stream->set_state(state);

	std::unique_ptr<StreamJob> job = std::make_unique<StreamJob>();
	job->state = state;
	job->stream_id = stream->get_instance_id();
	job->thread = std::thread(run_stream, current, input, state,
			callable_mp(this, &Thespeon::poll_streams));
	stream_jobs.push_back(std::move(job));

	return stream;
}

void Thespeon::poll_streams() {
	struct Event {
		Ref<ThespeonAudioStream> stream;
		std::vector<PackedInt64Array> sample_requests;
		bool finished = false;
		Error error = OK;
	};

	// Gather first and emit afterwards: a handler may start another stream.
	std::vector<Event> events;
	for (auto it = stream_jobs.begin(); it != stream_jobs.end();) {
		StreamJob &job = **it;
		Event event;
		event.stream = Ref<ThespeonAudioStream>(
				Object::cast_to<ThespeonAudioStream>(ObjectDB::get_instance(job.stream_id)));

		{
			std::lock_guard<std::mutex> lock(job.state->mutex);
			for (const std::vector<int64_t> &request : job.state->pending_sample_requests) {
				PackedInt64Array indices;
				indices.resize(int64_t(request.size()));
				std::copy(request.begin(), request.end(), indices.ptrw());
				job.state->sample_indices = indices;
				event.sample_requests.push_back(indices);
			}
			job.state->pending_sample_requests.clear();
			event.finished = job.state->done;
			event.error = thespeon_status_to_error(job.state->status);
		}

		if (event.finished) {
			job.thread.join();
			it = stream_jobs.erase(it);
		} else {
			++it;
		}

		if (event.stream.is_valid()) {
			events.push_back(std::move(event));
		}
	}

	for (const Event &event : events) {
		for (const PackedInt64Array &indices : event.sample_requests) {
			event.stream->emit_signal("samples_requested", indices);
		}
		if (event.finished) {
			event.stream->emit_signal("synthesis_finished", event.error);
		}
	}
}

Error Thespeon::preload_module(const String &p_module_identifier, bool p_warmup) {
	thespeon_engine *current = ensure_engine();
	if (!current) {
		return FAILED;
	}

	return to_error("thespeon_engine_preload",
			thespeon_engine_preload(current,
					p_module_identifier.utf8().get_data(),
					p_warmup ? THESPEON_PRELOAD_WARMUP : THESPEON_PRELOAD_SESSIONS,
					nullptr, nullptr, nullptr));
}

Error Thespeon::unload_module(const String &p_module_identifier) {
	thespeon_engine *current = ensure_engine();
	if (!current) {
		return FAILED;
	}

	return to_error("thespeon_engine_unload",
			thespeon_engine_unload(current, p_module_identifier.utf8().get_data()));
}

Error Thespeon::preload_input(const Ref<ThespeonInput> &p_input, bool p_warmup) {
	ERR_FAIL_COND_V_MSG(p_input.is_null(), ERR_INVALID_PARAMETER, "input is null");
	thespeon_engine *current = ensure_engine();
	if (!current) {
		return FAILED;
	}

	return to_error("thespeon_engine_preload_input",
			thespeon_engine_preload_input(current, p_input->get_handle(),
					p_warmup ? THESPEON_PRELOAD_WARMUP : THESPEON_PRELOAD_SESSIONS,
					nullptr, nullptr, nullptr));
}

Error Thespeon::unload_input(const Ref<ThespeonInput> &p_input) {
	ERR_FAIL_COND_V_MSG(p_input.is_null(), ERR_INVALID_PARAMETER, "input is null");
	thespeon_engine *current = ensure_engine();
	if (!current) {
		return FAILED;
	}

	return to_error("thespeon_engine_unload_input",
			thespeon_engine_unload_input(current, p_input->get_handle()));
}

bool Thespeon::is_loaded(const String &p_module_identifier) {
	thespeon_engine *current = ensure_engine();
	if (!current) {
		return false;
	}

	uint8_t loaded = 0;
	const thespeon_status status = thespeon_engine_is_loaded(current,
			p_module_identifier.utf8().get_data(), &loaded);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_engine_is_loaded", status);
		return false;
	}
	return loaded != 0;
}

bool Thespeon::is_input_loaded(const Ref<ThespeonInput> &p_input) {
	ERR_FAIL_COND_V_MSG(p_input.is_null(), false, "input is null");
	thespeon_engine *current = ensure_engine();
	if (!current) {
		return false;
	}

	uint8_t loaded = 0;
	const thespeon_status status = thespeon_engine_is_input_loaded(current,
			p_input->get_handle(), &loaded);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_engine_is_input_loaded", status);
		return false;
	}
	return loaded != 0;
}

Error Thespeon::refresh() {
	// A fresh engine reads the catalog on creation, so there is nothing to refresh.
	if (!engine) {
		return ensure_engine() ? OK : FAILED;
	}

	return to_error("thespeon_engine_refresh", thespeon_engine_refresh(engine));
}

int64_t Thespeon::get_resident_bytes() {
	thespeon_engine *current = ensure_engine();
	if (!current) {
		return 0;
	}

	uint64_t bytes = 0;
	const thespeon_status status = thespeon_engine_resident_bytes(current, &bytes);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_engine_resident_bytes", status);
		return 0;
	}
	return static_cast<int64_t>(bytes);
}
