# Nachos 3.4 — COP 4610

Instructional Nachos OS (v3.4) for FIU COP 4610. This repository starts from the
stock Linux/x86 Nachos 3.4 tree; we will extend it with course-assigned features
over the semester (threads, user programs, VM, file system, etc.).

## Team — Group 19

- Kristian Correa
- Sheila Perez Vitorero
- Skyler Hall

## Layout

- `nachos-3.4/` — Nachos source, docs, and C++ primer examples
- `gnu-decstation-ultrix/` — MIPS cross-compiler support for user programs

Working directory for builds is typically `nachos-3.4/code/`.

## Dependencies

Nachos builds as a **32-bit** host binary (`g++ -m32`, `as --32`). The bundled
MIPS cross-compiler under `gnu-decstation-ultrix/` is also 32-bit, so i686
libraries are required.

### Fedora Toolbox (recommended)

Keeps 32-bit packages off the host. Fedora 40 works well with this tree:

```bash
toolbox create --distro fedora --release 40 nachos
toolbox enter nachos
```

Inside the toolbox:

```bash
sudo dnf install -y \
  make gcc gcc-c++ binutils ed tcsh gdb \
  glibc-devel glibc-devel.i686 \
  libstdc++-devel libstdc++-devel.i686 \
  libgcc.i686
```

| Package | Purpose |
|---|---|
| `gcc` / `gcc-c++` / `binutils` / `make` | Host Nachos build (`-m32`) |
| `glibc-devel.i686`, `libstdc++-devel.i686`, `libgcc.i686` | 32-bit compile and running the MIPS cross-compiler |
| `libstdc++-devel` (x86_64) | Multilib C++ headers (`bits/c++config.h`) |
| `ed` | `make depend` updates Makefiles via `ed` |
| `tcsh` | Top-level `clean` / `print` targets call `/bin/csh` |
| `gdb` | Optional debugging |

Always build and run from inside the toolbox (`toolbox enter nachos`).

## Build & run (Project 0)

```bash
cd nachos-3.4/code
make

# threads demo
cd threads && ./nachos

# userprog demo
cd ../userprog && ./nachos -x ../test/halt

# single-step
./nachos -s -x ../test/halt
```

## Notes

- Use **Nachos 3.4**, not 4.0.
- Build artifacts (`nachos`, `*.o`, etc.) are gitignored; compile locally before demos or submission.
- If `make depend` embeds stale GCC include paths, re-run `make` after a clean toolchain install so dependency lines refresh.
