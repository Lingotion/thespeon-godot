# thespeon-gdextension

Godot 4.4+ GDExtension for Lingotion Thespeon TTS.

This is a public beta release, and nothing in this repository can be considered as stable for now. Limitations are listed in [known-issues.md](./known-issues.md).

## Installing

1. Extract `lingotion-thespeon-<platform>.zip` at the root of your Godot project.
2. Restart the editor.
3. Enable **Lingotion Thespeon** under **Project > Project Settings > Plugins**.
4. Import `.lingotion` packs from the **Lingotion Thespeon** dock.

## Building

```
git submodule update --init --recursive
cmake -S . -B build [-DGODOT_PROJECT_DIR=path/to/project]
cmake --build build --config Debug --target thespeon_gdextension
```

On Linux and macOS, configure with `-DCMAKE_BUILD_TYPE=Debug` and leave out `--config Debug`.
With `GODOT_PROJECT_DIR` set, the build copies the addon into `addons/lingotion-thespeon/` in that project. 

## Documentation

The editor help comes from `doc_classes/*.xml`, which is compiled into the extension and will show up in the Godot local docs. After you change the bound API, regenerate the XML from a project that has the new build, then fill in the empty descriptions:

```
godot --headless --doctool path/to/this/repo --gdextension-docs
```

The demo scene and script in the `demo` directory can be used to get started.

## Packaging

```
cmake --build build --config Debug --target package_addon
```

This writes `dist/lingotion-thespeon-<platform>.zip`. Build each platform on that platform. To ship extra files, list them in `release-files.txt`.
