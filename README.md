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
