// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
// The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

#include "thespeon_importer.h"

#include "thespeon_catalog.h"
#include "thespeon_constants.h"
#include "thespeon_status.h"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <thespeon.h>

using namespace godot;

namespace {

String data_directory() {
	return ProjectSettings::get_singleton()->globalize_path(
			thespeon_constants::RUNTIME_FILES_PATH);
}

// Null for "", which libthespeon reads as "not given". p_utf8 must outlive the call.
const char *optional(const CharString &p_utf8) {
	return p_utf8.length() > 0 ? p_utf8.get_data() : nullptr;
}

} // namespace

void ThespeonImporter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("import_pack", "pack_path"), &ThespeonImporter::import_pack);
	ClassDB::bind_method(D_METHOD("list_models"), &ThespeonImporter::list_models);
	ClassDB::bind_method(D_METHOD("preview_delete", "model", "module_type", "module_version"),
			&ThespeonImporter::preview_delete, DEFVAL(String()), DEFVAL(String()));
	ClassDB::bind_method(
			D_METHOD("delete_model", "model", "module_type", "module_version", "force"),
			&ThespeonImporter::delete_model, DEFVAL(String()), DEFVAL(String()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("delete_all_models"), &ThespeonImporter::delete_all_models);
}

Error ThespeonImporter::import_pack(const String &p_pack_path) {
	const String pack_path = ProjectSettings::get_singleton()->globalize_path(p_pack_path);

	thespeon_import_result result{};
	const thespeon_status status = thespeon_import_pack(
			data_directory().utf8().get_data(),
			pack_path.utf8().get_data(),
			&result);

	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_import_pack", status);
	}
	return thespeon_status_to_error(status);
}

Dictionary ThespeonImporter::list_models() {
	thespeon_catalog *catalog = nullptr;
	const thespeon_status status = thespeon_list_models(
			data_directory().utf8().get_data(), &catalog);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_list_models", status);
		return Dictionary();
	}

	const Dictionary result = thespeon_catalog_to_dictionary(*catalog);
	thespeon_catalog_free(catalog);
	return result;
}

Dictionary ThespeonImporter::preview_delete(const String &p_model, const String &p_module_type,
		const String &p_module_version) {
	const CharString model = p_model.utf8();
	const CharString module_type = p_module_type.utf8();
	const CharString module_version = p_module_version.utf8();

	thespeon_delete_result result{};
	const thespeon_status status = thespeon_delete_preview(
			data_directory().utf8().get_data(),
			optional(model),
			optional(module_type),
			optional(module_version),
			&result);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_delete_preview", status);
	}
	return thespeon_delete_result_to_dictionary(thespeon_status_to_error(status), result);
}

Dictionary ThespeonImporter::delete_model(const String &p_model, const String &p_module_type,
		const String &p_module_version, bool p_force) {
	const CharString module_type = p_module_type.utf8();
	const CharString module_version = p_module_version.utf8();

	thespeon_delete_result result{};
	const thespeon_status status = thespeon_delete_model(
			data_directory().utf8().get_data(),
			p_model.utf8().get_data(),
			optional(module_type),
			optional(module_version),
			p_force ? 1 : 0,
			&result);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_delete_model", status);
	}
	return thespeon_delete_result_to_dictionary(thespeon_status_to_error(status), result);
}

Dictionary ThespeonImporter::delete_all_models() {
	thespeon_delete_result result{};
	const thespeon_status status = thespeon_delete_all_models(
			data_directory().utf8().get_data(), &result);
	if (status != THESPEON_STATUS_OK) {
		thespeon_print_error("thespeon_delete_all_models", status);
	}
	return thespeon_delete_result_to_dictionary(thespeon_status_to_error(status), result);
}
