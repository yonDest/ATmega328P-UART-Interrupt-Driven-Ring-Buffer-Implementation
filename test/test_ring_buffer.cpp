#include <unity.h>
#include "uart.h"

void test_transmit_does_not_block(void) {
    // Send a byte and confirm it returns (doesn't hang)
    uart_transmit('A');
    TEST_PASS();
}

void test_read_returns_zero_when_empty(void) {
    uint8_t byte;
    uint8_t result = uart_read(&byte);
    TEST_ASSERT_EQUAL(0, result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_transmit_does_not_block);
    RUN_TEST(test_read_returns_zero_when_empty);
    return UNITY_END();
}
