CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O0 -g -Iinclude
LDFLAGS :=

TARGET := ext2sim

SRC := \
	src/main.c \
	src/disk.c \
	src/mkfs.c \
	src/info.c

OBJ := $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)