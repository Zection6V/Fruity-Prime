## Quick Start

1. Download and unzip the [latest MphRead release](https://github.com/NoneGiven/MphRead/releases).
2. Obtain a Metroid Prime Hunters ROM and drag the `.nds` file onto the `MphRead` binary. This will extract files necessary for the viewer.
3. If you want to view First Hunt files, obtain a First Hunt ROM and drag it onto `MphRead` as well.
4. Run the `MphRead` binary (i.e. by double-clicking) and observe the prompt.

Note: Version 0.19.0.0 of MphRead performs file extraction differently than previous versions. If you have existing files extracted with an older version of MphRead, it will not be able to read them. It is recommended that you simply delete the extracted files and perform setup with the ROM again.

## Path Setup

When using MphRead to extract ROM files as described above, a `paths.txt` file will be created in the folder. You can create this file yourself or edit if you already have extracted files you want MphRead to use. See [[Path Setup]] for more information.

## Gameplay

Running MphRead by double-clicking (not passing any arguments) will present prompts for entering information about the scene. You can choose the room, game mode, and players, and/or specify a list of individual models you want to view. See the [room list](https://github.com/NoneGiven/MphRead/wiki/Rooms) for valid room names and indices, and the [model gallery](https://github.com/NoneGiven/MphRead/wiki/Images:-MPH-Models) or [model list](https://github.com/NoneGiven/MphRead/wiki/Models) for valid model names. You can also quickly toggle through all possible values for an option by using the left/right arrow keys when the option is selected.

Once you launch the viewer, your selected settings will be saved and remembered later. They are stored in the `settings.json` file be created inside the `Savedata` folder, where adventure mode game saves are also stored. For more information on [playing in adventure mode](https://github.com/NoneGiven/MphRead/wiki/Setup:-Gameplay-Options), see the linked page.

See also the [command line arguments guide](https://github.com/NoneGiven/MphRead/wiki/Command-Line-Arguments) and the [guide to viewer controls](https://github.com/NoneGiven/MphRead/wiki/Viewer-Controls).

## Audio

The Windows version of MphRead is bundled with [OpenAL Soft](https://github.com/kcat/openal-soft) to enable playing sound effects (music playback is handled separately). Other versions of OpenAL are not supported on Windows. To enable audio on Linux or macOS, you may need to install OpenAL Soft if your system doesn't already have it. You can build OpenAL Soft from source, or find an existing distribution for your platform. Make sure the OpenAL Soft binary is accessible under the name `openal32.dll` for Windows, `libopenal.so.1` for Linux, or `/System/Library/Frameworks/OpenAL.framework/OpenAL` for macOS.

## Exporter

Models can be exported with the `-export` argument. There is no recolor index, and either a model name or room name may be passed. Examples:

```
MphRead -export Trace_lod0
MphRead -export Gorea1A_lod0
MphRead -export UNIT3_RM4
MphRead -export "Gorea Prison"
```

The exported files are as follows:

* One texture folder per recolor with one PNG file per texture
* One COLLADA (.dae) file per recolor
* One Python Blender script

Each COLLADA file can be used along with its corresponding textures, but not all exported information is contained in the COLLADA export. The COLLADA file contains mesh, texture, and vertex color information. Material parameters, bone positions, and animation data are all contained in the Python script. It is strongly recommended that the Python script be used to import the models into Blender, from which they can be manipulated and exported as desired.

The special values `layer2d` and `object2d` may be used instead of a model or room name to export 2D HUD layers and other objects, respectively. All exports will be placed in a subfolder named `_2D`. It is not possible to select specific items to export when using this feature.

For MPH, the special values `sfx`, `wfs`, and `strm` may be used to export SFX samples, the lower-quality SFX samples used in DS Download Play (internally called "WFS"), and the streaming audio. Streaming audio consists of voice lines (like "_hunter massacre_") and the title screen music only. Exporting other music tracks is not yet supported.

For MPH, the special value `movie` may be used to export the ActImagine VX movie files (FMVs/cutscene videos). Each frame of the video will be exported as a separate PNG. Audio export is currently not supported. All movies are exported by default, but you can also provide a movie name or full file path after the `movie` keyword (such as `-export movie 01_top.vx` or `-export movie C:\Users\user\some_movie.vx`). The export feature should still work for VX files from other Nintendo DS games besides MPH.

For FH, the special value `fhsfx` may be used to export all sounds, including both SFX and music tracks.

Note: An export path must be configured in `paths.txt` in order to use the exporter.

## Blender Import

### Setup

1. Download and install [Blender](https://www.blender.org/).
2. Locate the `modules\mph_common.py` folder in the MphRead directory and copy it to a directory for Blender scripts.
3. In Blender, open Edit > Preferences and ensure the script directory is set up as the folder with the `modules` folder containing the Python script. After changing the path, Blender must be restarted for it to take effect.

    a. In earlier versions of Blender, the path is in File Paths > Data > Scripts.

    b. In later versions of Blender, the path is in File Paths > Script Directories and the add button must be used.

Example: If the script is located at `C:\Users\user\MphRead\modules\mph_common.py`, the Blender script path should be `C:\Users\user\MphRead`.

Note: New versions of MphRead will come with new versions of `mph_common.py`. When upgrading to a new version, replace the file in the Blender scripts directory as well.

### Import

After exporting a model, the output files will include a Python script (e.g. `import_Trace_lod0.py`). Drag this file into Blender's Scripting tab.

Before executing the script, observe the variables at the top.

#### Recolor

```python
# recolors: pal_01, pal_02, pal_03, pal_04, pal_Team01, pal_Team02
recolor = 'pal_01'
```

* The comment on the first line provides a list of available recolor names. The variable value should be the name of the recolor you wish to import.

#### Animation

```python
# uv anims: 0, mat anims: 0, node anims: 26, tex anims: 0
uv_index = -1
mat_index = -1
node_index = 0
tex_index = -1
```

* These variables control which animation to import. The comment on the first line provides the total number of available animations of each type. If there are 0 animations, then the only valid index is -1, which indicates no animation. If there are 26 animations, then valid indices are -1 and 0-25.
  * `uv_index` controls the animation of UVs (texture coordinates).
  * `mat_index` controls the animation of material color and alpha.
  * `node_index` controls the animation of bones.
  * `tex_index` controls the animation of texture and palette choice (only used by First Hunt models).
* Currently, there is no way to import multiple animations of one type at once. To import a different animation, open Blender and execute the script with different animation index values.

Note: Prior to version 0.23.0.0 of MphRead, import scripts generated using older versions would not work with new versions of `mph_common.py`. Starting with version 0.23.0.0, there is instead a minimum supported version, so prior generated import script will continue to work with new versions of `mph_common.py` unless a breaking change is made and the minimum version is updated.