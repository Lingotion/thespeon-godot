# Known issues

Synthesis itself is done by libthespeon, so its known issues apply here too: see
[libthespeon's known-issues.md](https://github.com/Lingotion/libthespeon/blob/main/known-issues.md). The two that
matter most from Godot:

- **Synthesis is CPU-only** for the beta.
- **`set_default_emotion` is ignored once any segment sets an emotion.** Segments
  without one take theirs from their neighbors instead. Set the emotion on each
  segment with `segment_set_emotion` or `segment_add_emotion` if you mix the two.

The rest of this file covers the Godot extension.

## Release exports do not work on Windows

Only the `template_debug` variant of the extension is built. On Windows, the
manifest's `windows.release.x86_64` entry points at a `template_release` library that
does not exist, so a release export fails to load the extension. Export with
**Export With Debug** enabled for now.

On Linux and macOS, release exports load the `template_debug` library, so they work.

## Only three platforms are supported

The extension builds on Windows x64, Linux x86_64 and Apple Silicon macOS. There
are no builds for Intel macOS, Linux ARM, Android, iOS or the web.

## Every `Thespeon` node loads its own copy of the models

Each `Thespeon` node creates its own engine, so two nodes that use the same
character hold it in memory twice. Use a single `Thespeon` node, for example in an
autoload, and play its streams in as many `AudioStreamPlayer`s as you need.

## Most `Thespeon` calls block the main thread

Every method except `synthesize_stream` runs on the calling thread, so loading a
model or calling `synthesize_wav` from the main thread freezes the game until it
finishes. Call `preload_input` or `preload_module` at a convenient moment, such as a
loading screen, so that the first `synthesize_stream` starts quickly.

## Deleting modules while they are loaded is unsafe

`ThespeonImporter` deletes files without knowing whether any `Thespeon` node has
them loaded, in this process or another, such as a game running from the editor.
The editor dock refuses to delete while a scene is running, but calls from your own
scripts are not checked. Unload the modules or free the nodes first.

A `Thespeon` node that already exists also does not see imports or deletes until you
call its `refresh()`.

## Models can only be imported in the editor

Models are always read from `res://addons/lingotion-thespeon/runtime-files/`, and
another location cannot be given. An exported project's `res://` filesystem may be
read-only, so `ThespeonImporter.import_pack` is only meant for use in the editor.
Import every model the game needs before exporting.
