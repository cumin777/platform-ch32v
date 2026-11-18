/*
 * ws2812_bitbang.c - see ws2812_bitbang.h for the route rationale.
 *
 * Hard timing requirements (why plain digitalWrite is unusable here):
 *  - digitalWrite() does pin-table lookups, far slower than the 300ns
 *    T0H pulse. Edges are written straight to GPIO BSHR: one store sets
 *    or clears the pin atomically.
 *  - delayMicroseconds() has 1us granularity; the pulses are sub-us.
 *    Waits are anchored to the mcycle CSR instead: read the counter
 *    once per bit, then spin until (now - start) reaches the target.
 *    Loop/branch overhead is absorbed by the anchor, only the poll
 *    granularity (a few cycles) lands on top of the pulse width.
 */
#include "ws2812_bitbang.h"

static GPIO_TypeDef *ws_port;
static uint16_t ws_set_mask; /* BSHR bits [15:0]: atomic set   */
static uint32_t ws_clr_mask; /* BSHR bits [31:16]: atomic reset */

/* Read the free-running 32-bit cycle counter (1 count = 1/F_CPU). */
static inline uint32_t ws_cycle(void)
{
    uint32_t c;
    __asm volatile("csrr %0, mcycle" : "=r"(c) : /* no inputs */ : "memory");
    return c;
}

void ws2812_attach(GPIO_TypeDef *port, uint8_t pin_num)
{
    ws_port = port;
    ws_set_mask = (uint16_t)(1u << pin_num);
    ws_clr_mask = (uint32_t)1u << (pin_num + 16);
    port->BSHR = ws_clr_mask; /* DIN low = idle / reset level */
}

/* One WS2812 bit: high pulse (length depends on the bit value), then low
 * for the rest of the 1.25us slot, both anchored to the same start count. */
static void ws2812_bit(uint8_t bit)
{
    uint32_t start = ws_cycle();
    uint32_t high = bit ? WS2812_T1H_CYCLES : WS2812_T0H_CYCLES;

    ws_port->BSHR = ws_set_mask;
    while ((int32_t)(ws_cycle() - start) < (int32_t)high) {
        /* spin */
    }
    ws_port->BSHR = ws_clr_mask;
    while ((int32_t)(ws_cycle() - start) < (int32_t)WS2812_BIT_CYCLES) {
        /* spin */
    }
}

void ws2812_send_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    /* WS2812 wire order is GRB, most significant bit of each byte first */
    const uint8_t grb[3] = { g, r, b };
    uint8_t i, k;

    noInterrupts(); /* 24 bits * 1.25us = 30us critical section */
    for (i = 0; i < 3; i++) {
        uint8_t v = grb[i];
        for (k = 0; k < 8; k++) {
            ws2812_bit(v & 0x80);
            v <<= 1;
        }
    }
    interrupts();

    /* DIN stays low after the last bit: hold it low for the reset latch
     * (WS2812B-2020-V6 latches after >280us, we wait 500us for margin). */
    delayMicroseconds(WS2812_RESET_US);
}
