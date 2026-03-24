# ATmega328P UART — Interrupt-Driven Ring Buffer Implementation

A complete, production-pattern UART driver for the ATmega328P (Arduino Uno) written in bare-metal C++. Built with PlatformIO. No Arduino framework — direct register access only.

---

## Features

| Feature | Status |
|---|---|
| Bidirectional TX + RX | ✓ |
| Interrupt-driven both directions | ✓ |
| Non-blocking send and receive | ✓ |
| Ring buffers for both directions | ✓ |
| CPU free during all I/O | ✓ |

---

## Architecture

### Ring Buffers

Two separate circular arrays — one for incoming bytes (`rx_buffer`), one for outgoing (`tx_buffer`). They allow ISRs and main code to operate independently without blocking each other. Declared `volatile` to prevent the compiler from caching values in registers, since both ISRs and main code access them.

### `uart_init()`

Configures the UART hardware once at startup:
- `UBRR0H/L` — loads the baud rate divisor for correct bit timing
- `UCSR0B` — enables TX, RX, and the RX complete interrupt (`RXCIE0`). The TX data register empty interrupt (`UDRIE0`) is left off until there is data to send
- `UCSR0C` — sets 8-bit frame format

### `ISR(USART_RX_vect)`

Fires automatically when a full byte arrives. Reads `UDR0` immediately (which clears the hardware flag), writes into `rx_buffer` at `rx_head`, and advances `rx_head`. If the buffer is full, the byte is silently dropped. Main code is unaware of the arrival — it simply reads from the buffer at its own pace.

### `ISR(USART_UDRE_vect)`

Fires automatically when `UDR0` is empty and ready for the next byte. Reads the next byte from `tx_buffer` at `tx_tail`, writes it to `UDR0`, and advances `tx_tail`. Disables itself (`UDRIE0 = 0`) when the buffer is empty — required, otherwise the ISR would fire indefinitely.

### `uart_transmit(uint8_t data)`

Called by application code to queue a byte for sending. Writes the byte into `tx_buffer`, advances `tx_head`, and enables `UDRIE0`. Returns immediately — the ISR handles the actual transmission asynchronously. Busy-waits only if the TX buffer is completely full, which at 9600 baud is rare.

### `uart_read(uint8_t* out)`

Non-blocking receive check. Returns `0` immediately if nothing is waiting; returns `1` and fills `*out` if a byte is available, then advances `rx_tail`.

### `uart_print(const char* str)`

Convenience wrapper. Iterates over a null-terminated string calling `uart_transmit()` per character. Fully non-blocking.

### `sei()`

Sets the Global Interrupt Enable bit in the AVR status register (`SREG`). Required — without this, no ISRs fire regardless of what was enabled in `UCSR0B`.

---

## Hardware

| Parameter | Value |
|---|---|
| MCU | ATmega328P |
| Board | Arduino Uno |
| Baud rate | 9600 |
| Frame format | 8N1 |

---

## Project Setup

### Requirements
- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
- Arduino Uno (or any ATmega328P board)

### Build & Upload

```bash
# Build
pio run

# Upload
pio run --target upload

# Open serial monitor
pio device monitor
```

### Find Your Serial Port (macOS)
```bash
ls /dev/cu.*
```

Or use the hardware ID directly in `platformio.ini` — the Uno's USB vendor/product ID is fixed and survives port renumbering:
```bash
# Confirm IDs
system_profiler SPUSBDataType 2>/dev/null | grep -E "Arduino|Product ID|Vendor ID"
```

```ini
upload_port = hwid://2341:0043
monitor_port = hwid://2341:0043
```

---

## What a Production System Might Add

- **Error detection** — framing errors, parity errors, overflow flags from `UCSR0A`
- **Larger buffers** — necessary at higher baud rates (115200+)
- **DMA transfer** — for very high throughput scenarios
- **Flow control** — RTS/CTS hardware handshaking lines

---

## Testing

This project uses [Unity](https://github.com/ThrowTheSwitch/Unity) via PlatformIO's test runner.

```bash
pio test
```

---

## License

MIT
