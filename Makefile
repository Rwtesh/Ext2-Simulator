CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -Werror -O2 -Iinclude
LDFLAGS := -lncurses

SRC := \
	src/main.c \
	src/mkfs.c \
	src/info.c \
	src/disk.c \
	src/alloc.c \
	src/inode.c \
	src/dirent.c \
	src/path.c \
	src/touch.c \
	src/ls.c \
	src/rm.c \
	src/mkdir.c \
	src/fileio.c

OBJ := $(SRC:.c=.o)

BIN := ext2sim

# ── FUSE3 support ────────────────────────────────────────────────────
FUSE_CFLAGS  := $(shell pkg-config --cflags fuse3 2>/dev/null) -D_FILE_OFFSET_BITS=64
FUSE_LDFLAGS := $(shell pkg-config --libs   fuse3 2>/dev/null)
FUSE_BIN     := ext2sim-fuse

# Shared object files needed by the FUSE binary (no main.c, no CLI commands)
FUSE_SHARED_OBJ := \
	src/disk.o \
	src/alloc.o \
	src/inode.o \
	src/dirent.o \
	src/path.o

.PHONY: all clean

all: $(BIN) $(FUSE_BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

# fuse_fs.c needs the FUSE include path and _FILE_OFFSET_BITS=64
src/fuse_fs.o: src/fuse_fs.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -c $< -o $@

$(FUSE_BIN): src/fuse_fs.o $(FUSE_SHARED_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(FUSE_LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) src/fuse_fs.o $(BIN) $(FUSE_BIN)