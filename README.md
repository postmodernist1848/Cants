# Cants
Can'ts is an Ant simulation made in C with SDL2 (originally made in Python, but rewritten and improved to prove my competence)

Requirements for compilation:
SDL2, SDL2_image, SDL2_ttf libraries

## Build instructions:
```console
make package-linux | cross | native-win64
./cants
```
Use `make native-win64` to compile for Windows or `make package-linux` to compile on Linux.
Use 'cross' option to compile for Windows on Linux with mingw.

Controls:
    WASD to move
    Space to upgrade anthill when inside


-------------------------------- Map Editor ----------------------------------------------
Cants includes a map editor! It allows anyone to create their own maps. It is very easy to use:
```console
editor <file> | create <filename> <width> <height> | info <file>
```
Use the first variant to open a map for editing.
You can scroll when middle mouse button is pressed, choose what type of a tile you want with number keys and
paint with it with left mouse button or delete a tile with right mouse button.

Info command gives a quick summary on the size and tile counts for the map.

You can also create a map with 'create' command by providing its dimensions.

------------------------------------------------------------------------------------------

