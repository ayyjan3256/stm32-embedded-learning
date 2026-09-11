## Goal
Move from a raw address probe to real single-register write/read functions, verify the MPU6050 identity, and pull live multi-byte physics data (Acceleration, Gyro, Temperature).

## I2C1_WriteReg(dev_addr, reg_addr, data) — sequence
`START → wait SB → DR = dev_addr<<1 | 0 (write bit) → wait ADDR → read SR1, SR2 (clears ADDR) → DR = reg_addr → wait TXE → DR = data → wait BTF → STOP`

*Waiting on BTF (byte transfer finished) before STOP — not just TXE — matters: TXE only means the data register is empty. BTF means the byte has actually finished clocking out the shift register. Using TXE here risks cutting off the STOP condition mid-transfer.*

## I2C1_ReadReg(dev_addr, reg_addr) — sequence (Single Byte)
`START → wait SB → DR = dev_addr<<1 | 0 (write bit) → wait ADDR → read SR1, SR2 (clears ADDR) → DR = reg_addr → wait TXE → REPEATED START → wait SB → DR = dev_addr<<1 | 1 (read bit) → wait ADDR → clear ACK bit (CR1) → read SR1, SR2 (clears ADDR) → set STOP → wait RXNE → read DR`

*ACK-clear / ADDR-clear / STOP ordering is strict. Setting STOP before clearing ADDR queues STOP while the clock is still stretched, which hangs the peripheral.*

## I2C1_ReadRegs(dev_addr, reg_addr, buffer, len) — sequence (Burst Read)
Utilizes the MPU6050's auto-incrementing address pointer to read consecutive registers (0x3B through 0x48) in a single transaction, entirely preventing data tearing.
*   **The NACK Quirk:** To terminate a burst read, the STM32 must Acknowledge (ACK) every byte *except* the final byte. The final byte must be NACK'd so the slave releases the SDA line before the Master generates the STOP condition. 

## Data Reconstruction & Conversion
*   **Two's Complement:** The MPU6050 returns 16-bit signed integers. Reconstructed via `(high_byte << 8) | low_byte`.
*   **Physical Units:** 
    *   Accel default range is ±2g. Scale factor: `1g = 16384`.
    *   Gyro default range is ±250°/s. Scale factor: `1°/s = 131.0`.

## Known Quirks / Gotchas
*   **WHO_AM_I (0x75)** returning 0x68 confirms the device identity, but a flaky breadboard connection can still intermittently fail during burst reads. Logic analyzer verification of the final NACK is critical.
*   **Floating Point Printf:** Standard STM32 GCC setups disable `%f` formatting to save memory. A custom formatting function using modulo math (`print_fixed2`) is required to print decimal values without bloating the binary.
