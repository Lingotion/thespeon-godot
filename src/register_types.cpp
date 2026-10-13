// This code and software are protected by intellectual property law and is the property of Lingotion AB, reg. no. 559341-4138, Sweden.
// The code and software may only be used and distributed according to the Terms of Service and Use found at https://lingotion.com/terms-of-service/.

#include "register_types.h"

#include "thespeon_audio_stream.h"
#include "thespeon_importer.h"
#include "thespeon_input.h"
#include "thespeon_node.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_thespeon_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(ThespeonImporter);
	GDREGISTER_CLASS(ThespeonInput);
	GDREGISTER_CLASS(Thespeon);
	GDREGISTER_CLASS(ThespeonAudioStream);
	GDREGISTER_CLASS(ThespeonAudioStreamPlayback);
}

void uninitialize_thespeon_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

extern "C" {
// The name here must match entry_symbol in lingotion-thespeon.gdextension.
GDExtensionBool GDE_EXPORT thespeon_library_init(
		GDExtensionInterfaceGetProcAddress p_get_proc_address,
		GDExtensionClassLibraryPtr p_library,
		GDExtensionInitialization *r_initialization) {
	GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_thespeon_module);
	init_obj.register_terminator(uninitialize_thespeon_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
