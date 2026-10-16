# Test Play

Scenario launchers share `run.bat` for the Native executable, game-data check,
working directory, and process exit code. Each `.bat` wrapper supplies its
scenario flag and any common game options.

The in-game scenario belongs under `src/MphRead.Native/Mods/Testing/`. Keep its
scene and save setup in that scenario; use `ScenarioRunner::RunInShell` to start it
inside the managed shell so Escape and other shell actions work. Register its
command in `src/MphRead.Native/Mods/Entry/PlayCommands.cpp`, then add a thin
launcher here that calls `run.bat` with that command.
