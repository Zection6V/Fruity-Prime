A full `paths.txt` file looks like this this:

```
0.19.0.0
AMFE0=C:\Path\Files\amfe\data
AMFP0=C:\Path\Files\amfp\data
A76E0=C:\Path\Files\a76e
AMHE0=C:\Path\Files\amhe0
AMHE1=C:\Path\Files\amhe1
AMHP0=C:\Path\Files\amhp0
AMHP1=C:\Path\Files\amhp1
AMHJ0=C:\Path\Files\amhj0
AMHJ1=C:\Path\Files\amhj1
AMHK0=C:\Path\Files\amhk
Export=C:\Path\Export
```

The first line is the version of MphRead that was used to perform setup. This is relevant because version 0.19.0.0 introduced this new paths file format, so extracted file setup done by older versions will no longer be compatible.

A line beginning with `Export=` will refer to the path used when exporting models, textures, etc. using those command line options. This value is optional.

The other lines are all prefixed with the ROM version of Metroid Prime Hunters (e.g. `AMHE1`, the second revision of the North American release of MPH) or First Hunt (e.g. `AMFP0`, the first revision of the European release of FH) to whose extracted files they refer. If you have just one game version on hand, then you can have just that corresponding line specified, with the others blank. MphRead will detect the version when extracting files from a ROM and set up the paths file accordingly.