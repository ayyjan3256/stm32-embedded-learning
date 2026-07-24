# Day 5 — printf Retargeting via `_write()`

## Goal
Get `printf()` to transmit over UART, instead of manually calling
`uart_write_byte()` for every character.

## Day 5 vs Day 4
Day 4 gave working `uart_write_byte()` / `uart_read_byte()`. Today adds
one function, `_write()`, which newlib's `printf()` calls internally
once it's done formatting text into bytes. `_write()` just loops over
those bytes and hands each one to `uart_write_byte()` — no new hardware
concept, purely plumbing between the C standard library and code
already written.

## Key points
- `printf` formats, `_write()` transmits — two separate jobs.
- `_write(int file, char *ptr, int len)` ignores `file` here since
  UART is the only output device; that stops being safe once there's
  more than one output channel to choose between.
- `_write()` must have external (non-`static`) linkage — newlib ships
  a *weak* default stub, and a normal (strong) symbol with the same
  name automatically overrides it at link time.
- Return value from `_write()` is bookkeeping only — it does **not**
  gate whether the bytes are sent (the loop already ran). Returning
  the wrong count just misreports success/failure back to printf.

## Build note
`printf` requires `#include <stdio.h>` in the calling file
(`main.c`), or the compiler treats it as an implicit/built-in
declaration and the build fails — unrelated to `_write()` or UART.

## Test
Flash, open terminal at 115200 8N1. Expected output:
```
COUNT:0
COUNT:1
COUNT:2
...
```

## Debugging checklist
- Nothing prints / hangs → confirm bare `uart_write_byte('A')` still
  works unchanged from Day 4. If yes, issue is in the printf → `_write()`
  link, not the UART hardware path.
- `implicit declaration of function 'printf'` → missing
  `#include <stdio.h>`.
- Undefined reference to `_sbrk`/`_close`/`_lseek`/`_fstat` → printf's
  internals pulling in more of newlib's syscall surface than expected;
  don't blanket-stub these without understanding why first.
