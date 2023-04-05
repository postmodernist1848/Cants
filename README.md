# Cants
Can'ts is a game about ants made in C with SDL2 (original simulation was made in Python, but rewritten and turned into a game)

![image](https://user-images.githubusercontent.com/101038833/230065865-6a3e47de-3dd6-4f7f-bc40-f046cf24012b.png)

Requirements for compilation:
SDL2, SDL2_image, SDL2_ttf libraries
## Build instructions:
```console
make package-linux | cross | native-win64
./cants
```
Use `make native-win64` to compile for Windows or `make package-linux` to compile on Linux.
Use 'cross' option to compile for Windows on Linux with mingw.

In cants_config.h you may set ANDROID_BUILD to 1 to compile with Android features

--- Controls ---

WASD to move

Space to upgrade anthill when inside

Android:

Tap on the right (left) of the screen to turn right (left)

Tap on the top of the screen in the center to move forward or on the bottom to move backwards

When inside the anthill, tap on it to upgrade, granted that you have enough leaves

------------------------------------------------------------------------------------------
## Map Editor

Cants includes a map editor! It allows anyone to create their own maps. It is very easy to use:
```console
editor <file> | create <filename> <width> <height> | info <file>
```
Use the first variant to open a map for editing.
You can scroll when middle mouse button is pressed, choose what type of a tile you want with number keys and
paint with it with left mouse button or delete a tile with right mouse button.
Use arrow keys to translate the entire map (the map rotatates on the other side).
And, most importantly, save with Ctrl+s.

Info command gives a quick summary on the size and tile counts for the map.

You can also create a map with 'create' command by providing its dimensions.


