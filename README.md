# Cants
Can'ts is an Ant simulation made in C with SDL2 (originally made in Python, but rewritten and improved to prove my competence)

Requirements for compilation:
SDL2, SDL2_image, SDL2_ttf libraries

## Build instructions:
```console
make package-linux | cross | native_win64
./world_of_ants
```
Use `make native_win64` to compile for Windows or `make package_linux` to compile on Linux.
Use 'cross' option to compile for Windows on Linux with mingw.

Controls:
    WASD to move
    Space to upgrade anthill when inside
