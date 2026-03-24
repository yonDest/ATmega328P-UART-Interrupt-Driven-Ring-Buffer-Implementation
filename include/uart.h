#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_init(void);
void uart_transmit(uint8_t data);
uint8_t uart_read(uint8_t* out);
void uart_print(const char* str);

#ifdef __cplusplus
}
#endif
