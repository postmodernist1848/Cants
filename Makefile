CC=gcc
CFLAGS=-Wall -Wextra -Wno-switch
LIBS=-lSDL2 -lSDL2_image -lSDL2_ttf -lm

DEBUG_OBJS=main-debug-linux.o map-debug-linux.o
PACKAGE_OBJS=main-package-linux.o map-package-linux.o

.PHONY: clean

all: main

# Linux debug build
main: $(DEBUG_OBJS)
	$(CC) $(CFLAGS) -ggdb -DDEBUGMODE=1 $(LIBS) -o main $(DEBUG_OBJS)

%-debug-linux.o: %.c
	$(CC) $(CFLAGS) -ggdb -DDEBUGMODE=1 $(LIBS) -c -o $@ $<

# Linux package build
package-linux: $(PACKAGE_OBJS)
	$(CC) $(CFLAGS) -O3 $(LIBS) -o cants $(PACKAGE_OBJS) && strip --strip-unneeded $<

%-package-linux.o: %.c
	$(CC) $(CFLAGS) -O3 $(LIBS) -c -o $@ $<

map_editor: map_editor.c map.c
	$(CC) $(CFLAGS) -ggdb -O3 $(LIBS) -o map_editor map_editor.c map.c

clean:
	rm -rf *.o cants main cants.exe

#crosscompilation from Linux to Windows or native compilation requires headers and libs copied to the following dirs
CROSS_CC=x86_64-w64-mingw32-gcc
CROSS_INCLUDE_DIR=-Iwin64/mingw_dev_lib/include
CROSS_LIB_DIR=-Lwin64/mingw_dev_lib/lib
CROSS_LIBS=-lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf
CROSS_CFLAGS=$(CFLAGS) -Wl,-subsystem,windows -m64 -DDEBUGMODE=0 -O3 #-lmingw32 #not sure if this is needed
WIN_OBJS=main-win64.o map-win64.o
CROSS_OBJS=main-win64-cross.o map-win64-cross.o

native-win64: $(WIN_OBJS)
	$(CC) $(WIN_OBJS) $(CROSS_INCLUDE_DIR) $(CROSS_LIB_DIR) $(CROSS_CFLAGS) $(CROSS_LIBS) -o cants.exe 

%-win64.o: %.c
	$(CC) $(CROSS_INCLUDE_DIR) $(CROSS_LIB_DIR) $(CROSS_CFLAGS) $(CROSS_LIBS) -c -o $@ $<

cross: $(CROSS_OBJS)
	$(CROSS_CC) $(CROSS_OBJS) $(CROSS_INCLUDE_DIR) $(CROSS_LIB_DIR) $(CROSS_CFLAGS) $(CROSS_LIBS) -o cants.exe

%-win64-cross.o: %.c
	$(CROSS_CC) $(CROSS_INCLUDE_DIR) $(CROSS_LIB_DIR) $(CROSS_CFLAGS) $(CROSS_LIBS) -c -o $@ $<

