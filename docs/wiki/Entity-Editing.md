MphRead can be used to edit [[entity files|Entities]], but there is currently no graphical user interface for this feature, and in order to use it you must run the program from source and specify your edits by writing C# code.

## Setup

Open the file `Utility/RepackEntity.cs` and find the `TestEntityEdit` method. In the first line, replace `"Level SP Regulator"` with the name of the room whose entities you want to edit. Then remove the lines highlighted below. This space is where you will write your code to manipulate the entity file. After your code is finished, the updated entity file will be written to the `_pack` directory in your specified export location.

<img src="https://user-images.githubusercontent.com/2163967/191591679-45499ab2-a67e-49e6-8986-31ba8646cb06.png" height="350">

To execute your code and perform the entity edits, open `Program.cs` and insert `Utility.Repack.TestEntityEdit();` in the `Main` method, above the line containing the call to `ParseArguments`.

## Finding Specific Entities

Use MphRead to view the room. Enter selection mode (more info: [controls](https://github.com/NoneGiven/MphRead/wiki/Viewer-Controls#Selection) and [visualization](https://github.com/NoneGiven/MphRead/wiki/Visualization-Guide#Selection)) and cycle through entities until you find the one you're looking for. It may help to turn on invisible entity display as well. When you have the entity selected, the console window will print out some associated information.

<img src="https://user-images.githubusercontent.com/2163967/191590695-642e47e0-3738-47a6-b87a-67d75c229e2a.png" height="450">

The number in brackets is the entity's ID, which will be the easiest way to locate it in the entity file. Note that if the ID is -1, that means the entity is not part of the entity file, but rather is spawned in during gameplay (such as an item instance or beam projectile).

In the `TestEntityEdit` method, use code like the following to find and update that entity:

```cs
var affinitySpawner = (ItemSpawnEntityEditor)entities.First(e => e.Id == 8);
affinitySpawner.ItemType = ItemType.Deathalt;
affinitySpawner.Position = affinitySpawner.Position.AddY(5);
```

Each entity type that can be found in an entity file has an associated `EntityEditor` type, and the `EntityEditorBase` type of the `entities` list must be cast to the specific type (such as `ItemSpawnEntityEditor`) before any properties of that entity type can be updated.

## Adding and Removing Entities

Once you've found an entity as shown above, you can remove it from the room entirely by removing it from the `entities` list.

To add a new entity, a new `EntityEditor` object of the corresponding type must be added to the `entities` list. When you construct a new object, it will be important to set properties such as ID, node name, position, and layer mask. If you're unsure of what node name or layer mask to use, it may be a good idea to use the same values as an existing entity which is near your new entity.

```cs
short maxId = entities.Max(x => x.Id);
var newItemSpawn = new ItemSpawnEntityEditor()
{
    Id = ++maxId,
    NodeName = "rmMain",
    LayerMask = 0xFFFF,
    Position = new Vector3(5, 0, 13),
    ItemType = ItemType.OmegaCannon,
    Enabled = true,
    // etc.
};
entities.Add(newItemSpawn);
```