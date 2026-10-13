# This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
# The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

@tool
extends VBoxContainer

# Lists the installed Thespeon modules and imports and deletes them. Lives in the
# editor dock that thespeon_editor_plugin.gd creates.

const RUNTIME_FILES_PATH := "res://addons/lingotion-thespeon/runtime-files/"

enum Column { NAME, TYPE, VERSION, LANGUAGES }

var _importer := ThespeonImporter.new()
var _tree: Tree
var _import_button: Button
var _delete_button: Button
var _refresh_button: Button
var _problems_label: Label
var _status_label: Label
var _file_dialog: EditorFileDialog
var _confirm_dialog: ConfirmationDialog

# What the open confirmation dialog is about: the checked modules, or every
# module when all are checked, and whether deleting needs force because
# characters depend on a language module in it.
var _pending_delete: Array[Dictionary] = []
var _pending_delete_all := false
var _pending_force := false

# Runs import_pack off the main thread, so the editor stays responsive.
var _import_thread: Thread


func _init() -> void:
	name = "ThespeonInfoWindow"
	add_theme_constant_override("separation", 8)

	var path_label := Label.new()
	path_label.text = "Modules in %s" % RUNTIME_FILES_PATH
	_make_single_line(path_label)
	add_child(path_label)

	var toolbar := HBoxContainer.new()
	add_child(toolbar)

	_import_button = Button.new()
	_import_button.text = "Import…"
	_import_button.tooltip_text = "Import one or more .lingotion model packs"
	_import_button.pressed.connect(_on_import_pressed)
	toolbar.add_child(_import_button)

	_delete_button = Button.new()
	_delete_button.text = "Delete"
	_delete_button.tooltip_text = "Delete the checked modules"
	_delete_button.disabled = true
	_delete_button.pressed.connect(_on_delete_pressed)
	toolbar.add_child(_delete_button)

	_refresh_button = Button.new()
	_refresh_button.text = "Refresh"
	_refresh_button.tooltip_text = "Re-read the installed modules"
	_refresh_button.pressed.connect(refresh)
	toolbar.add_child(_refresh_button)

	_tree = Tree.new()
	_tree.columns = Column.size()
	_tree.column_titles_visible = true
	# The root is the "All modules" checkbox.
	_tree.hide_root = false
	_tree.select_mode = Tree.SELECT_ROW
	_tree.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_tree.custom_minimum_size = Vector2(0, 160)
	_tree.set_column_title(Column.NAME, "Name")
	_tree.set_column_title(Column.TYPE, "Type")
	_tree.set_column_title(Column.VERSION, "Version")
	_tree.set_column_title(Column.LANGUAGES, "Languages")
	for column in Column.values():
		_tree.set_column_clip_content(column, true)
		_tree.set_column_title_alignment(column, HORIZONTAL_ALIGNMENT_LEFT)
	# Name takes all the extra width; the rest stay at a fixed size and clip,
	# with the full text in their tooltips.
	_tree.set_column_expand(Column.NAME, true)
	for column: int in [Column.TYPE, Column.VERSION, Column.LANGUAGES]:
		_tree.set_column_expand(column, false)
	_tree.set_column_custom_minimum_width(Column.TYPE, 40)
	_tree.set_column_custom_minimum_width(Column.VERSION, 56)
	_tree.set_column_custom_minimum_width(Column.LANGUAGES, 220)
	_tree.item_edited.connect(_on_item_edited)
	add_child(_tree)

	_problems_label = Label.new()
	_problems_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_problems_label.visible = false
	add_child(_problems_label)

	_status_label = Label.new()
	_make_single_line(_status_label)
	add_child(_status_label)

	_file_dialog = EditorFileDialog.new()
	_file_dialog.title = "Import Lingotion Model Packs"
	_file_dialog.file_mode = EditorFileDialog.FILE_MODE_OPEN_FILES
	_file_dialog.access = EditorFileDialog.ACCESS_FILESYSTEM
	_file_dialog.add_filter("*.lingotion", "Lingotion model packs")
	_file_dialog.files_selected.connect(_on_packs_selected)
	add_child(_file_dialog)

	_confirm_dialog = ConfirmationDialog.new()
	_confirm_dialog.title = "Delete Thespeon Module"
	_confirm_dialog.ok_button_text = "Delete"
	_confirm_dialog.dialog_autowrap = true
	_confirm_dialog.confirmed.connect(_on_delete_confirmed)
	add_child(_confirm_dialog)


func _ready() -> void:
	_problems_label.add_theme_color_override("font_color",
			EditorInterface.get_editor_theme().get_color("warning_color", "Editor"))
	refresh()


func _exit_tree() -> void:
	# The import cannot be cancelled, so a dock closing mid-import waits for it.
	if _import_thread != null and _import_thread.is_started():
		_import_thread.wait_to_finish()


# Re-reads the runtime-files directory and rebuilds the module list.
func refresh() -> void:
	_tree.clear()

	var catalog: Dictionary = _importer.list_models()
	var characters: Array = catalog.get("characterModules", [])
	var languages: Array = catalog.get("languageModules", [])
	characters.sort_custom(_by_name_then_version)
	languages.sort_custom(_by_name_then_version)

	var root := _tree.create_item()
	_make_checkable(root, "All modules (%d)" % (characters.size() + languages.size()),
			not (characters.is_empty() and languages.is_empty()))
	_add_group(root, "Character modules (%d)" % characters.size(), characters, true)
	_add_group(root, "Language modules (%d)" % languages.size(), languages, false)
	_update_delete_button()

	var problems: PackedStringArray = catalog.get("problems", PackedStringArray())
	_problems_label.visible = not problems.is_empty()
	_problems_label.text = "Could not read:\n" + "\n".join(problems)

	if catalog.is_empty():
		_set_status("Could not read the modules; see the Output panel.")
	elif characters.is_empty() and languages.is_empty():
		_set_status("No modules installed. Import a .lingotion pack to add some.")
	else:
		_set_status("")


func _add_group(root: TreeItem, title: String, modules: Array, is_character: bool) -> void:
	var group := _tree.create_item(root)
	_make_checkable(group, title, not modules.is_empty())

	for module: Dictionary in modules:
		var item := _tree.create_item(group)
		var module_name: String = module.get("name", "")
		var identifier: String = module.get("identifier", "")
		var version: String = module.get("version", "")
		var module_type: String = module.get("moduleType", "")

		_make_checkable(item, module_name, true)
		item.set_tooltip_text(Column.NAME, identifier if identifier else module_name)
		item.set_text(Column.TYPE, module_type)
		item.set_text(Column.VERSION, version)

		var language_names := PackedStringArray()
		var language_details := PackedStringArray()
		for language: Dictionary in module.get("languages", []):
			language_names.append(_language_label(language))
			language_details.append(_language_details(language))
		item.set_text(Column.LANGUAGES, ", ".join(language_names))
		item.set_tooltip_text(Column.LANGUAGES, "\n".join(language_details))

		# Packs built before identifiers existed leave it "", so such a module is
		# named by name, type and version instead.
		var label := "%s (%s, %s)" % [module_name, module_type, version] if is_character \
				else "%s (%s)" % [module_name, version]
		item.set_metadata(Column.NAME, {
			"model": identifier if identifier else module_name,
			"module_type": "" if identifier else module_type,
			"module_version": "" if identifier else version,
			"label": label,
			"is_character": is_character,
		})


func _make_checkable(item: TreeItem, text: String, editable: bool) -> void:
	item.set_cell_mode(Column.NAME, TreeItem.CELL_MODE_CHECK)
	item.set_text(Column.NAME, text)
	item.set_editable(Column.NAME, editable)


# "English (US)", from the English name or else the ISO 639-2 code, and the most
# specific region or dialect the module gives.
func _language_label(language: Dictionary) -> String:
	var language_name: String = language.get("name_in_english", language.get("iso639_2", "?"))
	for key in ["custom_dialect", "iso3166_2", "iso3166_1"]:
		if language.has(key):
			return "%s (%s)" % [language_name, language[key]]
	return language_name


func _language_details(language: Dictionary) -> String:
	var parts := PackedStringArray()
	for key in ["iso639_2", "iso639_3", "glottocode", "iso3166_1", "iso3166_2",
			"custom_dialect", "autonym"]:
		if language.has(key):
			parts.append("%s: %s" % [key, language[key]])
	return "%s — %s" % [_language_label(language), ", ".join(parts)]


func _by_name_then_version(a: Dictionary, b: Dictionary) -> bool:
	var a_name: String = a.get("name", "")
	var b_name: String = b.get("name", "")
	if a_name.nocasecmp_to(b_name) != 0:
		return a_name.nocasecmp_to(b_name) < 0
	return String(a.get("version", "")).naturalnocasecmp_to(b.get("version", "")) < 0


# Keeps a label from driving the dock's minimum size
func _make_single_line(label: Label) -> void:
	label.autowrap_mode = TextServer.AUTOWRAP_OFF
	label.clip_text = true
	label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	label.custom_minimum_size = Vector2.ZERO
	label.tooltip_text = label.text


func _set_status(text: String) -> void:
	_status_label.text = text
	_status_label.tooltip_text = text


# The checked modules, characters first: deleting them before a language module
# keeps libthespeon from counting them as stranded.
func _checked_modules() -> Array[Dictionary]:
	var modules: Array[Dictionary] = []
	var root := _tree.get_root()
	if root == null:
		return modules
	for group in root.get_children():
		for item in group.get_children():
			if item.is_checked(Column.NAME):
				modules.append(item.get_metadata(Column.NAME))
	return modules


func _all_checked() -> bool:
	var root := _tree.get_root()
	return root != null and root.is_editable(Column.NAME) and root.is_checked(Column.NAME)


func _on_item_edited() -> void:
	# Checks or unchecks everything below, and updates the groups and root above.
	_tree.get_edited().propagate_check(Column.NAME, false)
	_update_delete_button()


func _update_delete_button() -> void:
	var count := _checked_modules().size()
	_delete_button.disabled = count == 0
	_delete_button.text = "Delete (%d)" % count if count > 0 else "Delete"


func _on_import_pressed() -> void:
	_file_dialog.popup_file_dialog()


func _on_packs_selected(pack_paths: PackedStringArray) -> void:
	_set_busy(true)
	var imported := 0
	var skipped := 0
	var uncounted := 0  # packs imported whose contents could not be listed
	var failures := PackedStringArray()
	for i in pack_paths.size():
		var pack_path := pack_paths[i]
		var progress := " (%d of %d)" % [i + 1, pack_paths.size()] if pack_paths.size() > 1 else ""
		_set_status("Importing %s%s…" % [pack_path.get_file(), progress])

		# Counted before importing, since afterwards every module in the pack exists.
		var configs: Variant = _pack_configs(pack_path)
		var existing := 0
		if configs != null:
			for config: String in configs:
				if FileAccess.file_exists(RUNTIME_FILES_PATH.path_join("configs").path_join(config)):
					existing += 1

		var result: Error = await _import_in_background(pack_path)
		if result != OK:
			failures.append("%s (%s)" % [pack_path.get_file(), error_string(result)])
		elif configs == null:
			uncounted += 1
		else:
			imported += configs.size() - existing
			skipped += existing
	_set_busy(false)

	_after_change()
	var summary := "Imported %s." % _plural(imported, "module")
	if skipped > 0:
		summary = "Imported %s, skipped %s already installed." % [
				_plural(imported, "module"), _plural(skipped, "module")]
	if uncounted > 0:
		summary += " Also imported %s whose modules could not be counted." % _plural(uncounted, "pack")
	if failures.is_empty():
		_set_status(summary)
	else:
		_set_status("%s Failed: %s; see the Output panel." % [summary, ", ".join(failures)])


func _import_in_background(pack_path: String) -> Error:
	_import_thread = Thread.new()
	_import_thread.start(_importer.import_pack.bind(pack_path))
	while _import_thread.is_alive():
		await get_tree().process_frame
	var result: Error = _import_thread.wait_to_finish()
	_import_thread = null
	return result


# The module descriptors in a pack, one per module: each configs/<name>.json entry,
# by file name, as libthespeon's importer installs them. Null when the pack cannot
# be read here; import_pack then reports any real problem.
func _pack_configs(pack_path: String) -> Variant:
	var configs := PackedStringArray()
	var zip := ZIPReader.new()
	if zip.open(pack_path) != OK:
		return null
	for entry in zip.get_files():
		var parts := entry.replace("\\", "/").split("/", false)
		if parts.size() >= 2 and parts[-2] == "configs" and parts[-1].get_extension().to_lower() == "json":
			configs.append(parts[-1])
	zip.close()
	return configs


# Locks out everything that reads or changes the modules while an import runs.
func _set_busy(busy: bool) -> void:
	_import_button.disabled = busy
	_refresh_button.disabled = busy
	_tree.mouse_filter = Control.MOUSE_FILTER_IGNORE if busy else Control.MOUSE_FILTER_STOP
	if busy:
		_delete_button.disabled = true
	else:
		_update_delete_button()


func _on_delete_pressed() -> void:
	var modules := _checked_modules()
	if modules.is_empty():
		return
	# The running game may have the modules loaded; deletes know nothing of its engine.
	if EditorInterface.is_playing_scene():
		_set_status("Stop the running scene before deleting modules.")
		return

	var message: String
	var force := false
	if _all_checked():
		var preview: Dictionary = _importer.preview_delete("")
		if preview.status != OK:
			_set_status("Cannot delete: %s" % error_string(preview.status))
			return
		message = "Delete every installed module?\n\nThis removes %s and frees %s." % [
				_plural(preview.binaries_removed, "file"),
				String.humanize_size(preview.bytes_freed)]
	elif modules.size() == 1:
		var module := modules[0]
		var preview := _preview(module)
		if preview.status != OK:
			_set_status("Cannot delete %s: %s" % [module.label, error_string(preview.status)])
			return
		message = "Delete %s?\n\nThis removes %s and frees %s." % [
				module.label,
				_plural(preview.binaries_removed, "file"),
				String.humanize_size(preview.bytes_freed)]
		if preview.stranded_characters > 0:
			message += "\n\n%s will be left with no language module and stop working until one is imported again." % \
					_plural(preview.stranded_characters, "character module")
			force = true
	else:
		# Each preview sees only its own module, so a binary that only the checked
		# modules share is left out of every one, and a language module counts
		# checked characters as stranded. Hence "at least" and "up to".
		var bytes_freed := 0
		var warnings := PackedStringArray()
		for module in modules:
			var preview := _preview(module)
			if preview.status != OK:
				_set_status("Cannot delete %s: %s" % [module.label, error_string(preview.status)])
				return
			bytes_freed += preview.bytes_freed
			if preview.stranded_characters > 0:
				warnings.append("%s may leave up to %s with no language module." % [
						module.label, _plural(preview.stranded_characters, "character module")])
				force = true
		message = "Delete %s?\n\n%s\n\nThis frees at least %s." % [
				_plural(modules.size(), "module"), _list_labels(modules),
				String.humanize_size(bytes_freed)]
		if not warnings.is_empty():
			message += "\n\n" + "\n".join(warnings)

	_pending_delete = modules
	_pending_delete_all = _all_checked()
	_pending_force = force
	_confirm_dialog.dialog_text = message
	_confirm_dialog.popup_centered(Vector2i(420, 0))


func _on_delete_confirmed() -> void:
	var modules := _pending_delete
	_pending_delete = []
	if modules.is_empty():
		return
	if EditorInterface.is_playing_scene():
		_set_status("Stop the running scene before deleting modules.")
		return

	var bytes_freed := 0
	var failures := PackedStringArray()
	if _pending_delete_all:
		var result: Dictionary = _importer.delete_all_models()
		bytes_freed = result.bytes_freed
		if result.status != OK:
			failures.append(error_string(result.status))
	else:
		for module in modules:
			var result: Dictionary = _importer.delete_model(
					module.model, module.module_type, module.module_version, _pending_force)
			bytes_freed += result.bytes_freed
			if result.status != OK:
				failures.append("%s (%s)" % [module.label, error_string(result.status)])

	_after_change()
	if failures.is_empty():
		_set_status("Deleted %s, freeing %s." % [
				_plural(modules.size(), "module"), String.humanize_size(bytes_freed)])
	else:
		_set_status("Delete failed for %s; see the Output panel." % ", ".join(failures))


func _preview(module: Dictionary) -> Dictionary:
	return _importer.preview_delete(module.model, module.module_type, module.module_version)


# One label per line, cut short so the dialog stays on screen.
func _list_labels(modules: Array[Dictionary]) -> String:
	const SHOWN := 10
	var lines := PackedStringArray()
	for module in modules.slice(0, SHOWN):
		lines.append("• " + module.label)
	if modules.size() > SHOWN:
		lines.append("…and %d more" % (modules.size() - SHOWN))
	return "\n".join(lines)


func _plural(count: int, noun: String) -> String:
	return "%d %s%s" % [count, noun, "" if count == 1 else "s"]


func _after_change() -> void:
	refresh()
	EditorInterface.get_resource_filesystem().scan()
