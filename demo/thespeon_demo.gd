# This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
# The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

extends Thespeon

const CHARACTER := "Aaron Archer"
const TEXT := "Hi! This is my voice generated in real-time!"

var _id := ""
var _input: ThespeonInput
var _stream: ThespeonAudioStream
var _synth_start_usec := 0
var _waiting_for_audio := false


func _ready() -> void:
	for module in list_models().characterModules:
		if module.name == CHARACTER:
			_id = module.identifier
	if _id.is_empty():
		push_error("No character module named %s" % CHARACTER)
		return

	# Blocks the main thread, so the whole load lands in this measurement.
	var start := Time.get_ticks_usec()
	var error := preload_module(_id, true)
	print("preload: %s in %.1f ms" % [error_string(error), _ms_since(start)])

	# Built once and reused: synthesize_stream copies it for its worker thread.
	_input = ThespeonInput.new()
	_input.set_character_module(_id)
	_input.add_segment(TEXT)
	_input.segment_set_emotion(0, "Anger")
	print("Press space to synthesize.")


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo \
			and event.keycode == KEY_SPACE and not _id.is_empty():
		_synthesize()


func _synthesize() -> void:
	# Replacing the stream drops the previous one, which cancels it if still running.
	_synth_start_usec = Time.get_ticks_usec()
	_stream = synthesize_stream(_input)
	if _stream == null:
		return
	print("synthesize_stream returned in %.1f ms" % _ms_since(_synth_start_usec))

	var start := _synth_start_usec
	_stream.samples_requested.connect(func(indices):
		print("◎ at sample %s (%.1f ms)" % [indices, _ms_since(start)]))
	_stream.synthesis_finished.connect(func(error):
		print("synthesis: %s in %.1f ms" % [error_string(error), _ms_since(start)]))

	_waiting_for_audio = true
	$AudioStreamPlayer.stream = _stream
	$AudioStreamPlayer.play()


func _process(_delta: float) -> void:
	# The playback position only moves once real audio has arrived, so this
	# measures time to first audio, to within a frame plus the mix buffer.
	if _waiting_for_audio and $AudioStreamPlayer.get_playback_position() > 0.0:
		_waiting_for_audio = false
		print("first audio after ~%.1f ms" % _ms_since(_synth_start_usec))


func _ms_since(start_usec: int) -> float:
	return (Time.get_ticks_usec() - start_usec) / 1000.0
