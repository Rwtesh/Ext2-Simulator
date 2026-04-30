# Ext2-Simulator

A lightweight, on-disk ext2-inspired filesystem simulator written in C.  
It ships two programs:

| Binary | Purpose |
|---|---|
| `ext2sim` | Interactive shell for creating and manipulating disk images |
| `ext2sim-fuse` | FUSE3 driver – mount an image on Linux and access it with standard tools |

---

## Building

### Prerequisites

| Package | Purpose |
|---|---|
| `gcc`, `make` | Toolchain |
| `ncurses-devel` / `libncurses-dev` | Linked by the interactive shell |
| `fuse3-devel` / `libfuse3-dev` | Required to build `ext2sim-fuse` |

**Fedora / RHEL:**
```bash
sudo dnf install gcc make ncurses-devel fuse3 fuse3-devel
```

**Ubuntu / Debian:**
```bash
sudo apt install gcc make libncurses-dev fuse3 libfuse3-dev
```

### Compile

```bash
make          # builds both ext2sim and ext2sim-fuse
make clean    # remove build artefacts
```

---

## `ext2sim` – Interactive Shell

```
ext2sim> mkfs   <img> <size_mb>       create a new image
ext2sim> info   <img>                 show superblock info
ext2sim> ls     <img> [path]          list directory
ext2sim> touch  <img> <path>          create an empty file
ext2sim> mkdir  <img> <path>          create a directory
ext2sim> rm     <img> <path>          delete a file
ext2sim> write  <img> <path> <text>   write text to a file
ext2sim> cat    <img> <path>          print a file
ext2sim> help                         show this list
ext2sim> exit | quit
```

### Quick example

```bash
./ext2sim
ext2sim> mkfs disk.img 4
ext2sim> touch disk.img /hello.txt
ext2sim> write disk.img /hello.txt "Hello, world!"
ext2sim> ls disk.img /
ext2sim> cat disk.img /hello.txt
ext2sim> exit
```

---

## `ext2sim-fuse` – FUSE3 Mount

### Mount

```bash
mkdir /tmp/myfs
./ext2sim-fuse disk.img /tmp/myfs          # background daemon
./ext2sim-fuse disk.img /tmp/myfs -f       # stay in foreground (useful for debugging)
./ext2sim-fuse disk.img /tmp/myfs -o ro    # read-only mount
./ext2sim-fuse disk.img /tmp/myfs -d       # debug mode (very verbose)
```

### Unmount

```bash
fusermount3 -u /tmp/myfs
```

### Supported operations

| FUSE callback | Shell equivalent |
|---|---|
| `getattr` | `stat`, `ls -l` |
| `readdir` | `ls` |
| `open` / `read` | `cat`, `cp` (source) |
| `create` / `write` / `truncate` | `echo >`, `cp` (dest), `>` redirect |
| `unlink` | `rm` |
| `mkdir` | `mkdir` |
| `rmdir` | `rmdir` |
| `statfs` | `df` |

### Limitations

- Maximum file size: **12 KiB** (12 direct blocks × 1 KiB each – no indirect blocks).
- Maximum filesystem size: **~8 MiB** (single 1 KiB block-bitmap → 8192 blocks).
- Timestamps are not stored (shown as epoch 0).
- Permissions are fixed: directories `0755`, regular files `0644`.

---

## End-to-end FUSE test (manual)

```bash
# 1. Create a 4 MiB image and populate it with the interactive shell
./ext2sim
ext2sim> mkfs test.img 4
ext2sim> touch test.img /hello.txt
ext2sim> write test.img /hello.txt "Hello from ext2sim!"
ext2sim> mkdir test.img /docs
ext2sim> touch test.img /docs/readme.txt
ext2sim> write test.img /docs/readme.txt "Documentation."
ext2sim> exit

# 2. Mount the image
mkdir /tmp/testmnt
./ext2sim-fuse test.img /tmp/testmnt -f &

# 3. Use standard shell commands
ls /tmp/testmnt/          # hello.txt  docs
cat /tmp/testmnt/hello.txt
ls /tmp/testmnt/docs/
df -h /tmp/testmnt

# 4. Write through the FUSE mount
echo "Created via FUSE" > /tmp/testmnt/new.txt
mkdir /tmp/testmnt/subdir
cp /tmp/testmnt/hello.txt /tmp/testmnt/subdir/copy.txt
rm /tmp/testmnt/new.txt

# 5. Unmount
fusermount3 -u /tmp/testmnt
```

---

## On-disk format (overview)

| Block # | Content |
|---|---|
| 0 | (unused / boot) |
| 1 | Superblock (`ext2sim_superblock`) |
| 2 | Group descriptor (`ext2sim_group_desc`) |
| 3 | Block bitmap (1 bit per block, 8192 blocks max) |
| 4 | Inode bitmap (1 bit per inode, 1024 inodes) |
| 5 … 5+N | Inode table (1024 × `ext2sim_inode`) |
| 5+N+1 … | Data blocks |

- Block size: 1024 bytes (fixed).
- Inode: 16-bit mode, 16-bit link count, 32-bit size, 12 × 32-bit direct block pointers, 64-byte reserved pad.
- Directory entry: 32-bit inode + 60-byte name (null-terminated, max 59 chars).  Each 1 KiB directory block holds exactly 16 entries.
