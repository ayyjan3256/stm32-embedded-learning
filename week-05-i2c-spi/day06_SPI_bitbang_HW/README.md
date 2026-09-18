# Week 5, Day 6: SPI Bit-Bang vs Hardware Peripheral, Both via Loopback

## Goal
Implement SPI manually on plain GPIO, verify it via loopback, then implement it again using the hardware SPI1 peripheral, and compare both on the logic analyzer.

## Pin Assignments
| Signal | Bit-Bang (manual GPIO) | Hardware SPI1 |
|---|---|---|
| SCK | PA0 | PA5 |
| MOSI | PA1 | PA7 |
| MISO | PA2 | PA6 |
| SS | PA3 (software-driven) | software slave management (SSM/SSI) |

Both implementations use loopback: MOSI jumpered directly to MISO on their respective pin set, so whatever byte is transmitted should be received back unchanged.

## Bit-Bang Implementation

Plain GPIO output/input, no alternate function — SCK/MOSI/SS as outputs (`MODER=01`), MISO as input (`MODER=00`).

**Core transfer loop (mode 0: CPOL=0, CPHA=0):**
```
SS_LOW()  (assert)
for each of 8 bits, MSB first:
    set MOSI to bit value        <- BEFORE raising SCK; CPHA=0 samples on
                                     the first edge, so data must be valid first
    SCK_HIGH()                    <- rising edge = sample point
    read MISO into received byte  <- same edge as the rise
    SCK_LOW()                     <- prep next bit here
SS_HIGH() (deassert)
```

**Why data is set before the clock edge, not after:** CPHA=0 means the very first SCK transition is already a sample point. If MOSI were set after `SCK_HIGH()`, the sampling side would read whatever was on the line *before* the intended bit — a real ordering bug, not just style.

**Small per-toggle delays** (`for(volatile int d=0; d<50; d++);` inside each SCK/MOSI toggle function) are deliberate — a bare register-toggle loop at 168MHz produces edges on the order of tens of nanoseconds, too fast for most USB logic analyzers to resolve cleanly. The delay exists purely to make the waveform capturable, not because the protocol requires it.

## Hardware SPI1 Implementation

**GPIO:** PA5/6/7 in alternate function mode, AF5 (SPI1's AF number — confirmed against the datasheet AF table, not assumed), **push-pull** (not open-drain — no bus-contention risk in SPI, only one device drives MOSI and only the selected slave drives MISO at a time).

**`SPI1_CR1`:**
- `MSTR` — master mode.
- `BR[2:0] = 011` — baud rate divider off APB2 (SPI1 lives on APB2, not APB1).
- `CPOL=0, CPHA=0` — left at reset default, matching the bit-bang implementation's mode 0.
- `SSM | SSI` — software slave management. With no real hardware NSS pin wired, `SSM` tells the peripheral to ignore hardware NSS entirely, and `SSI` feeds it a software-forced "I am selected" state — without both, the peripheral can fault thinking it's been deselected.
- `SPE` — enabled last, same ordering rule as I2C's `PE`.

**`SPIHW_TransferByte(byte)`:**
```
wait TXE           <- confirm TX buffer is empty BEFORE writing
write DR = byte
wait RXNE          <- wait for THIS transfer's received byte specifically
return DR
```
Waiting on `TXE` before writing (not after) matters — writing to `DR` while a previous byte is still unshifted would silently overwrite and drop it.

## Why `SPI_DR` Can't Return Your Own Echo

`SPI_DR` is one address aliasing two physically separate buffers: a write-only TX buffer and a read-only RX buffer. A write always lands in TX; a read always comes from RX. They're locked together by the same 8 clock pulses — RX for a given transfer always holds that same transfer's received byte, never a leftover from a previous one, *provided* the code correctly waits on `RXNE` before reading (skipping that wait is the actual way to accidentally read stale data — not a hardware ambiguity, a polling bug).

## Loopback: What It Proves and What It Doesn't

Proves: your driver's bit order, bracket timing (SS/CS), and sampling-edge logic are internally self-consistent — a real code bug (wrong bit order, backwards SS polarity, wrong nibble/edge pairing) will produce a mismatched received byte.

Does not prove: correctness against a real external device, since CPOL/CPHA is a single setting on one chip — there's no independently-configured second party in a loopback setup to disagree with.

## Encountered Pitfalls

- **PCF8574-style bug pattern applies here too, conceptually:** any function name mismatch (e.g. calling `LCD_PulseEnable` when the function is actually named `LCDPulseEnable`) is a link error, not a logic error — worth double-checking names match exactly between declaration, definition, and call site.
- SS/CS active-low logic is easy to get backwards in naming (`SS_LOW()` should *drive the pin low*, not confusingly do the opposite) — verify the function body matches its name, not just that the call sequence happens to produce the right net electrical result.
- Capturing a single one-shot transfer on the LA is timing-impractical; wrapping the test calls in `while(1)` with `HAL_Delay(500)` between iterations makes it capturable on your own schedule instead of racing NRST.
- Sample rate must be high enough to resolve whichever implementation is under test — bit-bang's added delays keep it in reach of most analyzers; hardware SPI at even a moderate baud divider is much faster and may need a higher sample rate or a larger `BR` divider to slow it down for capture purposes.
