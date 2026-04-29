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

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)