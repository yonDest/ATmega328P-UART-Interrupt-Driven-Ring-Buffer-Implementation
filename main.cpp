#include  <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// This code is for a bare-metal UART communication on an AVR microcontroller (like the ATmega328P used in Arduino Uno)
// Universal Asynchronous Receiver/Transmitter (UART) - a hardware communication interface that allows two devices to send and receive serial data without a clock signal
// Asynchronous communication (no clock line); Uses start bit, data bits, optional parity bit, and stop bit; Requires both devices to use the same baud rate
// Baud rate math:
// UBRR = (F_CPU / (16 * BAUD)) - 1 USART Baud Rate Register value calculation
// -> BAUD = F_CPU / (16 * (UBRR + 1)) Baud rate formula | 16 = UART oversampling factor (normal mode)
// F_CPU = 16MHz  Microcontroller clock frequency for Arduino Uno (ATmega328P)
// BAUD = 9600 
// 9600 bits per sec provides good balance between speed and reliability
// Reasons include: 
// Less sensitive to noise
// Lower timing error
// Works reliably with long cables
// Works with low-cost oscillators
// For microcontrollers like the ATmega328P used in the Arduino Uno, 9600 produces very small timing error, making communication stable
// UBRR = (16000000 / (16 * 9600)) - 1 =  (16000000/153600) - 1 = 103

#define BAUD 9600
#define UBRR_VALUE ((F_CPU / (16UL * BAUD)) - 1)

// --- Ring buffer ---
// Fixed size must be power of 2 for efficient wrapping with bitwise AND
// 16 bytes is plenty for 9600 baud on a 16MHz CPU
#define BUFFER_SIZE 16
#define BUFFER_MASK (BUFFER_SIZE - 1)

volatile uint8_t rx_buffer[BUFFER_SIZE];  // volatile = don't cache, ISR writes this
volatile uint8_t rx_head = 0;             // ISR writes here
volatile uint8_t rx_tail = 0;             // main code reads here

// TX buffer — written by main, read by ISR
volatile uint8_t tx_buffer[BUFFER_SIZE];
volatile uint8_t tx_head = 0;  // main advances on write
volatile uint8_t tx_tail = 0;  // ISR advances on read

// buffer is empty when head == tail
// buffer is full when (head + 1) & MASK == tail
// volatile tells the compiler these can change outside normal program flow

// --- UART init ---
void uart_init() {
    // Set baud rate across two 8-bit registers (UBRR0H and UBRR0L)
    UBRR0H = (UBRR_VALUE >> 8); // High byte
    UBRR0L = (UBRR_VALUE);      // Low byte

    // UCSR0B - USART Control and Status Register B
    // TXEN0 bit = enable transmitter
    // RXEN0 bit = enable receiver
    // RXCIE0 = RX Complete Interrupt Enable
    // fires the ISR below whenever a byte arrives instead of polling RXC0
    // UDRIE0 = TX data register empty interrupt enable — fires USART_UDRE_vect
    //          NOT enabled here — only enabled when we have data to send
    //          enabling it with empty buffer causes infinite ISR loop
    UCSR0B = (1 << TXEN0) | (1 << RXEN0) | (1 << RXCIE0);

    // UCSR0C — USART Control and Status Register C
    // UCSZ01 + UCSZ00 = 8 data bits
    // default is already 1 stop bit, no parity
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// --- RX ISR --- 
// This function is called automatically by hardware when a byte arrives
// Fires automatically when a full byte arrives on RX pin
// Must be as short as possible — no delays, no heavy work here
// USART_RX_vect is the AVR interrupt vector name for UART receive complete
ISR(USART_RX_vect) {
    uint8_t data = UDR0;  // read byte immediately, clears RXC0 flag
    uint8_t next_head = (rx_head + 1) & BUFFER_MASK;

    if (next_head != rx_tail) {
        // buffer has space, store the byte
        rx_buffer[rx_head] = data;
        rx_head = next_head;
    }
    // if buffer is full, byte is silently dropped
    // production code would set an overflow error flag here
}

// --- TX ISR ---
// Fires when UDR0 is empty and ready for next byte
// Feeds next byte from tx_buffer, or disables itself if buffer empty
ISR(USART_UDRE_vect) {
    if (tx_tail != tx_head) {
        // buffer has data, send next byte
        UDR0 = tx_buffer[tx_tail];
        tx_tail = (tx_tail + 1) & BUFFER_MASK;
    } else {
        // buffer empty — disable this interrupt or it fires forever
        // UDRIE0 = TX data register empty interrupt enable
        UCSR0B &= ~(1 << UDRIE0);
    }
}

// --- Transmit ---
// Loads byte into TX buffer and enables the TX interrupt
// Returns immediately — ISR handles actual sending
void uart_transmit(uint8_t data) {
    uint8_t next_head = (tx_head + 1) & BUFFER_MASK;
    // UCSR0A — Status Register A
    // UDRE0 bit = 1 means transmit buffer is empty and ready
    // spin here until buffer is ready
    // spin only if buffer is full — rare at 9600 baud
    while (next_head == tx_tail);

    tx_buffer[tx_head] = data;
    tx_head = next_head;

    // enable UDRIE0 to start firing TX ISR
    // safe to call even if already enabled
    UCSR0B |= (1 << UDRIE0);
}

// --- Receive from RX buffer ---
// Non-blocking — returns 1 if byte available, 0 if empty
uint8_t uart_read(uint8_t* out) {
    if (rx_head == rx_tail) return 0;  // buffer empty

    *out = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) & BUFFER_MASK;
    return 1;
}

void uart_print(const char* str) {
    while (*str) {
        uart_transmit(*str++);
    }
}

int main() {
    uart_init();
    sei();  // sei = Set Enable Interrupts, globally enables the interrupt system
            // AVR boots with global interrupts disabled
            // without this, RXCIE0 does nothing

    uart_print("Fully interrupt-driven UART ready.\r\n");

    while (1) {
        uint8_t c;
        if (uart_read(&c)) {
            // a byte arrived, echo it back
            uart_transmit(c);
        }

        // CPU is free to do other work here while waiting for input
        // this is the key difference from busy-waiting
        uart_print("main loop running...\r\n");
        _delay_ms(500);
    }

    return 0;
}

/*
** What each register is doing:
**
** UCSR0B — control register B. Enables hardware subsystems:
**          TXEN0  = enable transmitter (TX pin)
**          RXEN0  = enable receiver (RX pin)
**          RXCIE0 = RX complete interrupt enable. Fires ISR(USART_RX_vect)
**                   whenever a full byte arrives. Replaces polling RXC0.
**          UDRIE0 = TX data register empty interrupt enable. Fires ISR(USART_UDRE_vect)
**                   whenever UDR0 is ready for another byte.
**                   Only enabled when TX buffer has data. Disabled by ISR when empty.
**                   Never enable with an empty buffer — causes infinite ISR loop.
**
** UCSR0C — control register C. Sets the frame format:
**          UCSZ01 + UCSZ00 = 8 data bits per frame
**          No parity and 1 stop bit are hardware defaults, no bits needed
**
** UCSR0A — status register A. Hardware sets these flags automatically:
**          UDRE0 = transmit buffer empty. Now handled by UDRIE0/ISR, not polled.
**          RXC0  = receive complete. Handled by RXCIE0/ISR, not polled.
**          Both were previously busy-waited. Now fully interrupt-driven.
**
** UDR0 — the data register. Dual purpose:
**        Writing to it loads a byte into the transmit buffer and fires it out TX.
**        Reading from it returns the byte that arrived on RX and clears RXC0
*/