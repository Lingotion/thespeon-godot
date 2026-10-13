// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
// The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

#ifndef THESPEON_IMPORTER_H
#define THESPEON_IMPORTER_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

// Manages the modules installed in the addon's fixed runtime-files directory
// inside the project. Every call blocks and holds no engine, so it suits the
// editor. A Thespeon node that already exists needs refresh() afterwards to see
// the changes. Do not delete while any Thespeon node, in any process, has models
// loaded: the deletes know nothing of other engines.
class ThespeonImporter : public RefCounted {
	GDCLASS(ThespeonImporter, RefCounted)

protected:
	static void _bind_methods();

public:
	// Imports a .lingotion pack.
	Error import_pack(const String &p_pack_path);

	// As Thespeon::list_models: characterModules, languageModules and problems.
	// Empty, with the error printed, when the directory cannot be read.
	Dictionary list_models();

	// model is a module identifier or a display name. module_type applies only to
	// characters; it and module_version are ignored when "". All three return the
	// thespeon_delete_result fields plus "status" (an Error). preview_delete
	// changes nothing, and with model "" previews delete_all_models; its
	// stranded_characters counts the characters a delete would leave with no
	// language module, which delete_model refuses unless force is set.
	Dictionary preview_delete(const String &p_model, const String &p_module_type,
			const String &p_module_version);
	Dictionary delete_model(const String &p_model, const String &p_module_type,
			const String &p_module_version, bool p_force);
	Dictionary delete_all_models();
};

} // namespace godot

#endif // THESPEON_IMPORTER_H
