// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
// The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

#ifndef THESPEON_AUDIO_STREAM_H
#define THESPEON_AUDIO_STREAM_H

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_playback_resampled.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>

#include <thespeon.h>

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace godot {

inline constexpr float THESPEON_SAMPLE_RATE = 44100.0f;

// Shared between the synthesis worker, the stream, its playbacks and the Thespeon
// node that owns the worker. Everything except the token is guarded by mutex.
struct ThespeonStreamState {
	std::mutex mutex;
	// A deque, so appending never relocates samples the audio thread is reading.
	std::deque<float> samples;
	bool audio_complete = false; // the final chunk has arrived
	bool done = false; // the synthesis call has returned
	thespeon_status status = THESPEON_STATUS_OK;
	std::vector<std::vector<int64_t>> pending_sample_requests;
	PackedInt64Array sample_indices;

	thespeon_cancel_token *cancel = nullptr;

	~ThespeonStreamState() { thespeon_cancel_token_destroy(cancel); }
};

// Audio from Thespeon.synthesize_stream(). Plays while it is still being
// synthesized, and can be replayed once it has been.
class ThespeonAudioStream : public AudioStream {
	GDCLASS(ThespeonAudioStream, AudioStream)

	std::shared_ptr<ThespeonStreamState> state;

protected:
	static void _bind_methods();

public:
	~ThespeonAudioStream();

	void set_state(const std::shared_ptr<ThespeonStreamState> &p_state) { state = p_state; }

	// Stops synthesis; audio received so far still plays. Freeing the stream does
	// the same.
	void cancel();
	bool is_synthesizing() const;
	// The sample indices of the THESPEON_AUDIO_SAMPLE_REQUEST markers, once known.
	PackedInt64Array get_sample_indices() const;

	Ref<AudioStreamPlayback> _instantiate_playback() const override;
	String _get_stream_name() const override;
	double _get_length() const override;
	bool _is_monophonic() const override;
};

class ThespeonAudioStreamPlayback : public AudioStreamPlaybackResampled {
	GDCLASS(ThespeonAudioStreamPlayback, AudioStreamPlaybackResampled)

	std::shared_ptr<ThespeonStreamState> state;
	// Touched by the audio thread in _mix_resampled and by the main thread in
	// _start/_seek.
	std::atomic<uint64_t> cursor{ 0 };
	std::atomic<bool> playing{ false };

protected:
	static void _bind_methods() {}

public:
	void set_state(const std::shared_ptr<ThespeonStreamState> &p_state) { state = p_state; }

	void _start(double p_from_pos) override;
	void _stop() override;
	bool _is_playing() const override;
	int32_t _get_loop_count() const override;
	double _get_playback_position() const override;
	void _seek(double p_position) override;

	int32_t _mix_resampled(AudioFrame *p_dst_buffer, int32_t p_frame_count) override;
	float _get_stream_sampling_rate() const override;
};

} // namespace godot

#endif // THESPEON_AUDIO_STREAM_H
