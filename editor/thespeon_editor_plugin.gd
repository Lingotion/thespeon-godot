# This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
# The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

@tool
extends EditorPlugin

const ThespeonInfoWindow := preload("thespeon_info_window.gd")

var _dock: EditorDock


func _enter_tree() -> void:
	var info_window := ThespeonInfoWindow.new()

	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 8)
	margin.add_theme_constant_override("margin_top", 8)
	margin.add_theme_constant_override("margin_right", 8)
	margin.add_theme_constant_override("margin_bottom", 8)
	margin.add_child(info_window)

	_dock = EditorDock.new()
	_dock.add_child(margin)
	_dock.title = "Lingotion Thespeon"
	# Makes the dock size and shape survive a restart
	_dock.layout_key = "lingotion_thespeon"
	_dock.default_slot = EditorDock.DOCK_SLOT_LEFT_UR
	# Allow the dock to be on the left or right of the editor, and to be made floating.
	_dock.available_layouts = EditorDock.DOCK_LAYOUT_VERTICAL | EditorDock.DOCK_LAYOUT_FLOATING

	add_dock(_dock)


func _exit_tree() -> void:
	if is_instance_valid(_dock):
		remove_dock(_dock)
		_dock.queue_free()
	_dock = null


func _get_plugin_name() -> String:
	return "Lingotion Thespeon"
