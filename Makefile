CC=gcc
CFLAGS=-Wall -Wextra -Wno-switch
LIBS=-lSDL2 -lSDL2_image -lSDL2_ttf -lm
STATIC_LIBS=`ldd cants | awk '{print $$3}'`
.PHONY: clean

all: cants

cants: cants.c
	$(CC) $(CFLAGS) -ggdb -DDEBUGMODE=1 $(LIBS) -o cants cants.c

package_linux: cants.c
	$(CC) $(CFLAGS) -g0 -O3 -DDEBUGMODE=0 $(LIBS) -o cants cants.c && strip --strip-unneeded cants

static_linux: cants.c
	$(CC) $(CFLAGS) -g0 -O3 -DDEBUGMODE=0 -o cants cants.c $(STATIC_LIBS) && strip --strip-unneeded cants

clean:
	rm cants

#crosscompilation from Linux to Windows or native compilation requires headers and libs copied to the following dirs

CROSS_CC=x86_64-w64-mingw32-gcc
CROSS_INCLUDE_DIR=-Iwin64/mingw_dev_lib/include
CROSS_LIB_DIR=-Lwin64/mingw_dev_lib/lib
CROSS_LIBS=-lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf
CROSS_CFLAGS=-Wall -Wextra -Wno-switch -Wl,-subsystem,windows -m64 -DDEBUGMODE=0 -O3 #-lmingw32 #not sure if this is needed

native_win64 : cants.c
	$(CC) cants.c $(CROSS_INCLUDE_DIR) $(CROSS_LIB_DIR) $(CROSS_CFLAGS) $(CROSS_LIBS) -o cants.exe 

cross: cants.c
	$(CROSS_CC) cants.c $(CROSS_INCLUDE_DIR) $(CROSS_LIB_DIR) $(CROSS_CFLAGS) $(CROSS_LIBS) -o cants

