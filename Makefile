# Makefile for ZX Spectrum 48K emulator (MSYS2 MINGW64 + SDL2)

# Path to your MSYS2/MINGW64 installation
PREFIX      := E:/msys64/mingw64

# Toolchain
CC          := $(PREFIX)/bin/gcc.exe

# SDL2 include + libs
SDL_INC     := -I$(PREFIX)/include/SDL2
SDL_LIBPATH := -L$(PREFIX)/lib
SDL_LIBS    := -lmingw32 -lSDL2main -lSDL2

# Sources, objects, target
SRC         := main.c z80.c
OBJ         := $(SRC:.c=.o)
TARGET      := zx48.exe

# Compiler & linker flags
CFLAGS      := -std=c11 -O2 $(SDL_INC)
LDFLAGS     := $(SDL_LIBPATH) $(SDL_LIBS)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c z80.h
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
