# Day 6 — pyserial on the PC Side

## Goal
First Python of the whole program. Read the `COUNT:N` lines the board
has been sending since Day 5's `printf` retargeting, and parse them
into a real Python integer instead of just eyeballing text in a
terminal.

## File
- `day06_pyserial_read.py` — opens the board's COM port, reads lines,
  parses out the count.

## What it does
1. Opens the ST-Link's virtual COM port at 115200 baud (must match the
   MCU's BRR-configured baud rate from Day 3, or output is garbled).
2. `ser.readline()` reads until it sees `\n` — same terminator the
   MCU's `printf("COUNT:%d\n", count)` sends, so each call grabs one
   complete message.
3. What comes back is a **bytes object** (`b'COUNT:0\n'`), not a
   string — `pyserial` hands you the raw bytes exactly as received,
   since ASCII bytes and text characters are the same thing, just
   interpreted differently.
4. `.decode('ascii')` converts those bytes into a real Python string;
   `.strip()` removes the trailing `\n`.
5. `.split(':')` breaks `"COUNT:0"` into `["COUNT", "0"]`.
6. `int(parts[1])` converts the digit string into an actual integer
   you can do math/comparisons on — mirrors, in reverse, what the
   MCU's `printf` did converting an int into ASCII text to transmit.

## Possible Pitfalls
- `timeout=2` is set generously **longer** than the MCU's ~1s send
  interval, not equal to it — a timeout equal to the send interval
  risks `readline()` timing out mid-line under normal scheduling
  jitter, returning a truncated fragment.
- `if not line: continue` skips empty reads (a timeout with nothing
  received returns `b''`), avoiding an `IndexError` on `parts[1]`
  when there's no data to parse.
- `len(parts) != 2` guards against a malformed/partial line reaching
  `int()` and crashing the script.

## Setup / usage
1. Flash the board with Day 5's `printf` code (or Day 7's, which also
   sends confirmation lines — either works for testing this script).
2. Confirm your board's COM port in Device Manager (Ports → STMicro-
   electronics STLink Virtual COM Port) and update `PORT` if not COM4.
3. `pip install pyserial` if not already installed.
4. Run: `python day06_pyserial_read.py`
5. Expect continuous output: `Received count: 0`, `Received count: 1`, ...
   Ctrl+C to stop.
