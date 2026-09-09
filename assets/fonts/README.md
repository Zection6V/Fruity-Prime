# Inter

The launcher's typeface, in the two weights `GuiTheme.Face` asks for: regular
and semibold.

The managed launcher gets Inter from Avalonia's own font package. The native
one has no package manager, so the two files sit here and the Win32 head loads
them privately at startup with `AddFontResourceEx(FR_PRIVATE)` -- the program
never installs a font on the machine it runs on, and a build with these files
missing falls back to whatever `CreateFont` substitutes rather than failing.

Version 4.1, from <https://github.com/rsms/inter/releases>.

`OFL.txt` is the SIL Open Font License 1.1 the files are distributed under,
kept beside them because the licence requires it: the font may be bundled,
embedded and redistributed, including commercially, as long as its copyright
notice and licence travel with it.
