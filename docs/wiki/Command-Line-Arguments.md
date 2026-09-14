### Usage

```
MphRead usage:

    -room <room_name -or- room_id>
    -model <model_name> [recolor_index]

At most one room may be specified, while any number of models may be specified.
To load First Hunt models, include -fh in the argument list.
Available room options: -mode, -players, -boss, -node, -entity
```


### Viewing Models

```
-model <model_name> [recolor_index] [-fh]
```

More than one model may be loaded by specifying the `-model` / `-m` option multiple times.

See the [MPH model gallery](https://github.com/NoneGiven/MphRead/wiki/Images:-MPH-Models) or [MPH model list](https://github.com/NoneGiven/MphRead/wiki/Models) for valid model names. See the [recolor list](https://github.com/NoneGiven/MphRead/wiki/Models#recolors) for valid recolor indices. If no recolor index is specified, a default value of 0 will be used.

If `-fh` is included anywhere in the argument list, First Hunt models will be loaded. This option is necessary because some FH models share names with MPH models. For FH models names, see the [First Hunt model gallery](https://github.com/NoneGiven/MphRead/wiki/Images:-FH-Models) or [First Hunt model list](https://github.com/NoneGiven/MphRead/wiki/Models-(First-Hunt)). First Hunt models do not use recolors.


### Viewing Rooms

```
-room <room_name_or_id> [-mode value] [-players value] [-boss value] [-node value] [-entity value]
```

At most one room may be loaded at a time with the `-room` / `-r` option. If specifying a room name with spaces in it, make sure to enclose it in double quotes.

See the [MPH room list](https://github.com/NoneGiven/MphRead/wiki/Rooms) and [First Hunt room list](https://github.com/NoneGiven/MphRead/wiki/Rooms-(First-Hunt)) for valid room names and IDs. If an ID is specified, it must refer to an MPH room only. If a room name is specified, it can refer to either an MPH room or an FH room.

`-mode` / `-g`: The game mode. If not specified, it will default to single-player mode for 1P rooms, and Battle mode for MP rooms.

`-players` / `-p`: The number of players. If not specified, it will default to 1 for 1P rooms, and 2 for MP rooms. This does not cause players to spawn, just changes the active layers or version of the room.

`-boss` / `-b`: The game state flags representing defeated bosses. If not specified, it will default to none.

`-node` / `-n`: The game state flags representing room node layer. If not specified, it will be calculated according to the metadata and game mode.

`-entity` / `-l`: The game state flags representing room entity layer. If not specified, it will be calculated according to the metadata, game mode, and boss flags. 

### Exporting

```
-export <model_or_room_name>
-export <special_value>
```

See the [setup & export guide](https://github.com/NoneGiven/MphRead/wiki/Setup-&-Export-Guide#Exporter) for more information about the `-export` / `-e` option.

### Extracting

```
-extract <archive_name>
```

Individual archives in the `archives` directory can be decompressed and extracted to a corresponding folder in `archives` using the `-extract` / `-x` option. This is not necessary after `-setup` has been run once.

### Setup

```
-setup
```

If you have already extracted MPH files to a location specified in `paths.txt`, and want to extract all the archives so MphRead can use them, run this command.