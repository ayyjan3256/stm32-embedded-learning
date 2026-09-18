# Week 5, Day 7: Review, Rebuild From Memory, and Full Integration

## Goal
Prove retention by rebuilding the week's core drivers from memory, then combine everything into one project: MPU6050 data on the LCD (shared I2C1 bus), with SPI loopback proven independently.

## Hours 1-4: Rebuild From Memory

Each driver was reconstructed from memory (no reference code open) before comparing against the working version:

- **Hour 1 — I2C1:** GPIO config (open-drain, AF4, pull-up), peripheral init order (SWRST → CR2/CCR/TRISE → PE last), `I2C1_WriteReg`, and `I2C1_ReadRegs` including the ACK-disable/ADDR-clear/STOP ordering for the final byte.
- **Hour 2 — MPU6050 driver:** wake sequence (`PWR_MGMT_1 = 0x00`), WHO_AM_I check (`0x75` → `0x68`), 14-byte burst read starting at `ACCEL_XOUT_H` (`0x3B`), byte reassembly into signed `int16_t`, and accel/gyro scale factors (16384.0 / 131.0).
- **Hour 3 — LCD driver:** PCF8574 pin mapping, why a register-less byte write is required (not `I2C1_WriteReg`), `LCD_PulseEnable`/`SendNibble`/`SendByte`, and the full HD44780 init sequence in order (three `0x03` nibbles → `0x02` → `0x28`/`0x0C`/`0x06`/`0x01`).
- **Hour 4 — Hardware SPI1:** GPIO AF config with push-pull, `SPI1_CR1` bit-by-bit (MSTR, BR on APB2, CPOL/CPHA, SSM/SSI, SPE last), and `SPIHW_TransferByte` with correct TXE-before-write ordering.

**Recurring bug classes caught during rebuild and real coding this week**, worth remembering as a checklist for future drivers:
- Using `I2C1->SR` instead of `I2C1->SR1`/`SR2` — no combined `SR` register exists on this peripheral.
- Function name mismatches between definition and call site (e.g. `LCDPulseEnable` defined, `LCD_PulseEnable` called) — compiles to a link error, easy to miss on a quick read.
- ACK-disable / ADDR-clear / STOP ordering for the last byte of an I2C read — got this backwards at least once even after fixing it previously, confirming it's a genuinely sticky detail worth double-checking every time, not just conceptually understood once.
- Reusing a register-based write function (`I2C1_WriteReg`) for a register-less device (PCF8574) — sends an extra phantom byte that becomes a real, corrupting falling edge on the shared data lines.
- Integer division silently truncating float math (`accel_x/16384` vs `accel_x/16384.0f`).
- Wrong constant reused across unrelated calculations (gyro divisor accidentally set to the accel constant).

## Hour 5: Combined Project

MPU6050 + LCD share I2C1 (PB6/PB7); SPI1 hardware loopback runs independently on separate pins (PA5/6/7), proving the drivers coexist in one build without interfering — confirmed by construction, since each uses entirely distinct GPIO pins and peripheral clock-enable bits.

## Hour 6: Reference Sheet + Repo Commit

One-page reference sheet covering I2C transaction structure, the STM32 I2C register set and CCR/TRISE timing math, the NACK-on-last-byte read quirk, PCF8574/HD44780 init sequence, SPI's four signals and CPOL/CPHA, the SPI register set, and what loopback testing does and doesn't prove — filed as `week5-reference-sheet.md`.

## Week 5 Checkpoint

Before moving to Week 6, answer without looking anything up:
1. What is the STM32 I2C read quirk around NACK on the last byte, and why does getting it wrong hang the bus?
2. Why must I2C GPIO pins be open-drain while SPI pins are push-pull?
3. What do CPOL and CPHA each control, and what happens if master and slave disagree on either?
4. How can the MPU6050 (0x68) and LCD backpack (0x27) share the same two wires without interfering?
5. What is the PCF8574's role relative to the HD44780 — does it understand LCD commands?
6. Why does a full-duplex SPI transfer always both send and receive on every call?
7. What would a loopback test fail to catch that only a real slave device would expose?
