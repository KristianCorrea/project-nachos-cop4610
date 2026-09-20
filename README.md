# Nachos 3.4 — COP 4610

Instructional Nachos OS (v3.4) for FIU COP 4610. This repository starts from the
stock Linux/x86 Nachos 3.4 tree; we extend it with course-assigned features over
the semester (threads, user programs, VM, file system, etc.).

## Team — Group 19

- Kristian Correa
- Sheila Perez Vitorero
- Skyler Hall

## Layout

- `nachos-3.4/` — Nachos source, docs, and C++ primer examples
- `nachos-3.4/code/` — build root (`threads`, `userprog`, `test`, …)
- `nachos-3.4/code/reports/` — writeups (`project0.txt`, `project1.txt`) and run logs under `outputs/`
- `gnu-decstation-ultrix/` — MIPS cross-compiler support for user programs

Working directory for builds is typically `nachos-3.4/code/` (or `code/threads/` for Project 1).

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

## Build basics

```bash
cd nachos-3.4/code
make clean && make depend && make   # or just build under threads/
```

If `make` fails looking for an old GCC path (e.g. `.../gcc/.../8/include/stddef.h`), run `make depend` inside the toolbox so dependency lines match the current toolchain.

Default `threads/Makefile`:

```make
DEFINES = -DTHREADS
```

Graders (and local demos) add Project 1 flags through this same `DEFINES` variable. Leave it as `-DTHREADS` only for submission.

---

## Project 1 — Threads synchronization

**Status:** Exercises 1–4 complete. Writeup: `nachos-3.4/code/reports/project1.txt`.

| Exercise | Flag | What we built |
|---|---|---|
| 1 | `HW1_SEMAPHORES` | Shared-variable race demo; sync with semaphore + barrier |
| 2 | `HW1_LOCKS` | `Lock` in `synch.cc`; same demo via Acquire/Release |
| 3 | *(always on)* | Mesa `Condition` (Wait / Signal / Broadcast) |
| 4 | `HW1_ELEVATOR` | Elevator + person threads in `elevator.cc` |

### Key files

- `threads/threadtest.cc` — `ThreadTest(int n)`, `SimpleThread`, Ex1/Ex2 sync
- `threads/synch.h`, `threads/synch.cc` — Semaphore (stock), Lock, Condition
- `threads/elevator.cc` — Ex4 controller (`Elevator`, `ArrivingGoingFromTo`)
- `threads/main.cc` — `-q N` → `ThreadTest(N)`; elevator harness under `HW1_ELEVATOR`
- `code/Makefile.common` — links `elevator.cc`; compiles with `$(DEFINES)`

### How to enable a feature

Edit `nachos-3.4/code/threads/Makefile`, then rebuild:

```make
# Ex1
DEFINES = -DTHREADS -DHW1_SEMAPHORES

# Ex2 (do not combine with HW1_SEMAPHORES)
DEFINES = -DTHREADS -DHW1_LOCKS

# Ex4
DEFINES = -DTHREADS -DHW1_ELEVATOR
```

```bash
cd nachos-3.4/code/threads
make clean && make depend && make
```

### Run commands

```bash
# Default (no HW1_*): unsynchronized shared-variable ThreadTest
./nachos          # uses testnum=1
./nachos -q 4     # n = 0..4 forked threads (+ main as thread 0)
./nachos -rs 42 -q 4

# With HW1_SEMAPHORES or HW1_LOCKS: synced demo; finals should be 5*(n+1)
./nachos -q 4
./nachos -rs 42 -q 4

# With HW1_ELEVATOR: ThreadTest skipped; elevator sim only
./nachos
```

Expected synced finals: `-q 0` → 5, `-q 1` → 10, …, `-q 4` → **25**.

### Run logs

Saved under `nachos-3.4/code/reports/outputs/`:

- `exercise_1.txt` — unsynced race vs semaphore sync (+ barrier)
- `exercise_2.txt` — lock sync
- `exercise_4.txt` — elevator livelock before fix, full successful run after

---

## Project 0 — Warmup (reference)

```bash
cd nachos-3.4/code
make

cd threads && ./nachos                    # historically: ping-pong threads
cd ../userprog && ./nachos -x ../test/halt
./nachos -s -x ../test/halt               # single-step
```

Note: after Project 1, the default threads binary runs the shared-variable `ThreadTest` (race version unless `HW1_SEMAPHORES` / `HW1_LOCKS` is set), not the original Project 0 ping-pong loop.

## Notes

- Use **Nachos 3.4**, not 4.0.
- Build artifacts (`nachos`, `*.o`, etc.) are gitignored; compile locally before demos or submission.
- For course turn-in: keep all `HW1_*` flags **disabled**, report in `code/reports/project1.txt`, package as `nachosdir` → `proj1.tgz` per the assignment handout.
- If `make depend` embeds stale GCC include paths, refresh with `make depend` after a clean toolchain install.
