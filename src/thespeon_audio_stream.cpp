// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
// The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

#include "thespeon_audio_stream.h"

#include <godot_cpp/core/class_db.hpp>

#include <algorithm>

using namespace godot;

// --- ThespeonAudioStream ------------------------------------------------------

void ThespeonAudioStream::_bind_methods() {
	ClassDB::bind_method(D_METHOD("cancel"), &ThespeonAudioStream::cancel);
	ClassDB::bind_method(D_METHOD("is_synthesizing"), &ThespeonAudioStream::is_synthesizing);
	ClassDB::bind_method(D_METHOD("get_sample_indices"), &ThespeonAudioStream::get_sample_indices);

	ADD_SIGNAL(MethodInfo("samples_requested",
			PropertyInfo(Variant::PACKED_INT64_ARRAY, "sample_indices")));
	ADD_SIGNAL(MethodInfo("synthesis_finished", PropertyInfo(Variant::INT, "error")));
}

ThespeonAudioStream::~ThespeonAudioStream() {
	cancel();
}

void ThespeonAudioStream::cancel() {
	if (state) {
		thespeon_cancel_token_cancel(state->cancel);
	}
}

bool ThespeonAudioStream::is_synthesizing() const {
	if (!state) {
		return false;
	}
	std::lock_guard<std::mutex> lock(state->mutex);
	return !state->done;
}

PackedInt64Array ThespeonAudioStream::get_sample_indices() const {
	if (!state) {
		return PackedInt64Array();
	}
	std::lock_guard<std::mutex> lock(state->mutex);
	return state->sample_indices;
}

Ref<AudioStreamPlayback> ThespeonAudioStream::_instantiate_playback() const {
	Ref<ThespeonAudioStreamPlayback> playback;
	playback.instantiate();
	playback->set_state(state);
	return playback;
}

String ThespeonAudioStream::_get_stream_name() const {
	return "Thespeon";
}

double ThespeonAudioStream::_get_length() const {
	if (!state) {
		return 0.0;
	}
	std::lock_guard<std::mutex> lock(state->mutex);
	// Unknown until the final chunk has arrived.
	return state->audio_complete ? state->samples.size() / double(THESPEON_SAMPLE_RATE) : 0.0;
}

bool ThespeonAudioStream::_is_monophonic() const {
	return false;
}

// --- ThespeonAudioStreamPlayback ----------------------------------------------

void ThespeonAudioStreamPlayback::_start(double p_from_pos) {
	begin_resample();
	_seek(p_from_pos);
	playing = state != nullptr;
}

void ThespeonAudioStreamPlayback::_stop() {
	playing = false;
}

bool ThespeonAudioStreamPlayback::_is_playing() const {
	return playing;
}

int32_t ThespeonAudioStreamPlayback::_get_loop_count() const {
	return 0;
}

double ThespeonAudioStreamPlayback::_get_playback_position() const {
	return cursor / double(THESPEON_SAMPLE_RATE);
}

void ThespeonAudioStreamPlayback::_seek(double p_position) {
	const uint64_t target = uint64_t(std::max(0.0, p_position) * THESPEON_SAMPLE_RATE);
	if (!state) {
		cursor = 0;
		return;
	}
	// Only as far as has been synthesized.
	std::lock_guard<std::mutex> lock(state->mutex);
	cursor = std::min<uint64_t>(target, state->samples.size());
}

int32_t ThespeonAudioStreamPlayback::_mix_resampled(AudioFrame *p_dst_buffer, int32_t p_frame_count) {
	if (!playing || !state) {
		return 0;
	}

	int32_t mixed = 0;
	bool finished = false;
	{
		std::lock_guard<std::mutex> lock(state->mutex);
		const uint64_t position = cursor;
		const uint64_t available = state->samples.size();
		if (position < available) {
			mixed = int32_t(std::min<uint64_t>(p_frame_count, available - position));
			auto sample = state->samples.begin() + std::ptrdiff_t(position);
			for (int32_t i = 0; i < mixed; ++i, ++sample) {
				p_dst_buffer[i] = { *sample, *sample };
			}
			cursor = position + mixed;
		}
		finished = mixed < p_frame_count && (state->audio_complete || state->done);
	}

	if (finished) {
		playing = false;
		return mixed;
	}

	// Synthesis has fallen behind: pad with silence and wait for more, without
	// moving the cursor past audio that has not arrived yet.
	for (int32_t i = mixed; i < p_frame_count; ++i) {
		p_dst_buffer[i] = { 0.0f, 0.0f };
	}
	return p_frame_count;
}

float ThespeonAudioStreamPlayback::_get_stream_sampling_rate() const {
	return THESPEON_SAMPLE_RATE;
}
