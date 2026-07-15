# Build System Guide

## Overview

Build system uses **GNU Make** with delegation scripts (`build.sh`, `build.bat`).

---

## Quick Start

### Linux / Mac / WSL
```bash
make          # Compile
make clean    # Remove object files
make fclean   # Remove objects + binary
make re       # Full rebuild
make help     # Show all targets
```

### Windows
```cmd
build.bat              # Compile (delegates to make)
build.bat clean        # Remove object files
build.bat fclean       # Remove objects + binary
build.bat re           # Full rebuild
build.bat help         # Show all targets
```

---

## What Gets Cleaned?

### Directory Structure
```
webserv-webbita-feature-merge-mvp/
├─ src/                (source files, never deleted)
├─ includes/           (headers, never deleted)
├─ obj/                (object files ← deleted by clean/fclean)
├─ webserv             (binary executable ← deleted by fclean)
├─ Makefile
├─ build.sh
└─ build.bat
```

### `make clean`
```
BEFORE:
  obj/
  ├─ main.o
  ├─ Server.o
  ├─ CGIHandler.o
  ├─ ConfigParser.o
  ├─ ConfigValidator.o
  └─ Request.o
  webserv (binary) ✓ KEPT

AFTER:
  obj/          (directory empty/removed)
  webserv       (binary) ✓ KEPT
```

**Use case:** Recompile all objects but keep binary

### `make fclean`
```
BEFORE:
  obj/
  ├─ main.o
  ├─ Server.o
  └─ ...
  webserv (binary)

AFTER:
  (all removed)
```

**Use case:** Complete clean slate, rebuild everything

### `make re`
```
Equivalent to: make fclean && make all
= Full rebuild from source
```

---

## Build Script Delegation

### `bash build.sh`

**What it does:**
```bash
1. Detects C++ compiler (g++, clang++)
2. Sets CXX environment variable
3. Delegates to: make [target]
4. Shows help on completion
```

**Example:**
```bash
$ bash build.sh
========================================
WebServ Build System
========================================

Make target: all

[✓] Compiler: g++

(makes all)

==========================================
Build Targets Available:
  make              - compile (default)
  make clean        - remove object files
  make fclean       - remove all (objects + binary)
  make re           - full rebuild
==========================================
```

**With target:**
```bash
bash build.sh clean    # delegates to: make clean
bash build.sh fclean   # delegates to: make fclean
bash build.sh re       # delegates to: make re
```

### `build.bat` (Windows)

**What it does:**
```
1. Tries to find: mingw32-make, make, or nmake
2. Sets MAKE_CMD based on what's found
3. Delegates to: %MAKE_CMD% [target]
```

**Attempts in order:**
- `mingw32-make.exe` ← MinGW (preferred)
- `make.exe` ← MSYS2
- `nmake.exe` ← Visual Studio (may require syntax adjustment)

---

## Makefile Targets Reference

```makefile
all              → Compile ($(NAME) = webserv binary)
                   Creates: obj/*.o + webserv

clean            → Remove obj/ directory
                   Keeps: webserv binary

fclean           → clean + remove webserv binary
                   Result: pristine directory

re               → fclean + all (full rebuild)

help             → Display this guide with colors
```

---

## Compilation Process

```
Source Files                Object Files              Binary
src/main.cpp     ──────→   obj/main.o        
src/Server.cpp   ──────→   obj/Server.o      ───────→ webserv
src/Request.cpp  ──────→   obj/Request.o     ─────────
...              ──────→   ...                         (linked)
```

### Step by step
```bash
$ make
(mkdir obj created automatically)
Compiling: src/main.cpp
Compiling: src/Server.cpp
...
Linking object files to create binary...
✔ Webserv compiled successfully!
```

---

## Common Workflows

### Fresh Build
```bash
make clean    # Remove old objects
make          # Compile from source
```

### Full Rebuild
```bash
make re       # (equivalent to: make fclean && make all)
```

### Edit and Recompile
```bash
# Edit src/Server.cpp
make          # Only recompiles Server.cpp (smart rebuild)
```

### Debug Build
```bash
# Edit Makefile: CFLAGS += -g -O0
make clean
make
# Now has debugging symbols, no optimizations
```

---

## Flags Explained

### CFLAGS (Compilation Flags)
```makefile
-Wall           : Enable all warnings
-Wextra         : Extra pedantic warnings
-Werror         : Treat warnings as errors
-std=c++98      : C++98 standard (legacy compatible)
```

Result: **Strict compilation, catches bugs early**

### LDFLAGS (Linking Flags)
```makefile
-lpthread       : Link POSIX threads library
```

Result: **Binary linked with pthreads, supports threading**

---

## Troubleshooting

### "Command not found: make"
```
On Linux:   sudo apt install build-essential
On Mac:     brew install make
On Windows: Install MinGW or MSYS2
```

### "no rule to make target 'Makefile'"
```
Current directory must contain Makefile
$ pwd
/path/to/websrv-webbita-feature-merge-mvp
$ ls Makefile
Makefile ✓
```

### Object files corrupted
```
Solution: Full rebuild
$ make re
```

### Binary won't run
```
Possible causes:
1. Not compiled: $ make
2. Wrong architecture: $ file webserv
3. Missing library: $ ldd webserv
   check for "libpthread.so => not found"
```

---

## Advanced: Manual Compilation

If make fails, compile manually:

### Linux/Mac
```bash
g++ -Wall -Wextra -Werror -std=c++98 -lpthread \
    -Iincludes \
    src/main.cpp src/Server.cpp src/ConfigParser.cpp \
    src/ConfigValidator.cpp src/Request.cpp src/CGIHandler.cpp \
    -o webserv
```

### Windows (MinGW)
```cmd
g++.exe -Wall -Wextra -Werror -std=c++98 -lpthread ^
    -Iincludes ^
    src/main.cpp src/Server.cpp src/ConfigParser.cpp ^
    src/ConfigValidator.cpp src/Request.cpp src/CGIHandler.cpp ^
    -o webserv.exe
```

### Windows (MSVC)
```cmd
cl.exe /EHsc /std:c++latest /W4 ^
    /I includes ^
    src/main.cpp src/Server.cpp src/ConfigParser.cpp ^
    src/ConfigValidator.cpp src/Request.cpp src/CGIHandler.cpp ^
    /link ws2_32.lib /OUT:webserv.exe
```

---

## File Dependency Map

```
Makefile
  ├─ depends on: src/*.cpp, includes/*.hpp
  │
  ├─ main.o:
  │   └─ depends on: main.cpp, ConfigParser.hpp, ConfigValidator.hpp, Server.hpp
  │
  ├─ Server.o:
  │   └─ depends on: Server.cpp, Server.hpp, Request.hpp, CGIHandler.hpp
  │
  ├─ Request.o:
  │   └─ depends on: Request.cpp, Request.hpp
  │
  ├─ CGIHandler.o:
  │   └─ depends on: CGIHandler.cpp, CGIHandler.hpp
  │
  ├─ ConfigParser.o:
  │   └─ depends on: ConfigParser.cpp, ConfigParser.hpp, Config.hpp
  │
  └─ ConfigValidator.o:
      └─ depends on: ConfigValidator.cpp, ConfigValidator.hpp, Config.hpp

webserv (binary)
  └─ depends on: all .o files + -lpthread
```

Change any `.cpp` or `.hpp` → `make` only recompiles affected `.o` files.

---

## Performance Tips

### Faster Rebuilds
```bash
# Only changed files recompile
$ make

# vs.
$ make re  # Everything recompiles (slower)
```

### Parallel Compilation (4 cores)
```bash
make -j4   # Use 4 threads for compilation
```

### Check compilation time
```bash
time make re
```

---

**Status:** ✅ Production build system

**Last Updated:** 2026-07-15
