/*
 * ws2812_bitbang.c - see ws2812_bitbang.h for the route rationale.
 *
 * Timing rules (why plain digitalWrite is unusable here):
 *  - digitalWrite() does pin-table lookups, far slower than the 300ns
 *    T0H pulse. Edges are written straight to GPIO BSHR: one store sets
 *    or clears the pin atomically.
 *  - Pulses are sub-microsecond, so the delay loop below is calibrated
 *    at boot: ws2812_calibrate() counts how many loop iterations fit
 *    into a 50ms window measured by millis(). The measured rate absorbs
 *    the real per-iteration cost (pipeline, taken-branch penalty, flash
 *    wait states); only the few cycles of fixed store overhead inside
 *    each pulse stay uncalibrated, which the lowered NS targets cover.
 */
#include "ws2812_bitbang.h"

static GPIO_TypeDef *ws_port;
static uint16_t ws_set_mask; /* BSHR bits [15:0]: atomic set   */
static uint32_t ws_clr_mask; /* BSHR bits [31:16]: atomic reset */

static uint32_t ws_per_ms;      /* measured delay-loop iterations per ms */
static uint32_t ws_t0h_iters;
static uint32_t ws_t1h_iters;
static uint32_t ws_bit_iters;

/* Two-instruction decrement loop; per-iteration cost is calibrated, not
 * assumed (2 or 3 cycles depending on core/flash, does not matter). */
static inline void ws_dly(uint32_t n)
{
    __asm volatile("1: addi %0, %0, -1\n"
                   "   bnez %0, 1b"
                   : "+r"(n));
}

void ws2812_attach(GPIO_TypeDef *port, uint8_t pin_num)
{
    ws_port = port;
    ws_set_mask = (uint16_t)(1u << pin_num);
    ws_clr_mask = (uint32_t)1u << (pin_num + 16);
    port->BSHR = ws_clr_mask; /* DIN low = idle / reset level */
}

void ws2812_calibrate(void)
{
    uint32_t start, end, n = 0;

    /* Land on a millis() tick edge, then count iterations over 50ms.
     * Quantization error stays within 1-2 ticks => < 4%. */
    start = millis();
    while (millis() == start) {
        /* wait for tick */
    }
    end = start + 50;
    while ((int32_t)(millis() - end) < 0) {
        ws_dly(1000);
        n += 1000;
    }
    ws_per_ms = n / 50;

    ws_t0h_iters = (uint32_t)((uint64_t)ws_per_ms * WS2812_T0H_NS / 1000000);
    ws_t1h_iters = (uint32_t)((uint64_t)ws_per_ms * WS2812_T1H_NS / 1000000);
    ws_bit_iters = (uint32_t)((uint64_t)ws_per_ms * WS2812_BIT_NS / 1000000);
    if (ws_t0h_iters == 0) ws_t0h_iters = 1;
    if (ws_t1h_iters < ws_t0h_iters + 1) ws_t1h_iters = ws_t0h_iters + 1;
    if (ws_bit_iters < ws_t1h_iters + 1) ws_bit_iters = ws_t1h_iters + 1;
}

uint32_t ws2812_iters_per_ms(void)
{
    return ws_per_ms;
}

/* One WS2812 bit: high pulse (length depends on the bit value), then low
 * for the rest of the 1.25us slot. */
static void ws2812_bit(uint8_t bit)
{
    uint32_t high = bit ? ws_t1h_iters : ws_t0h_iters;

    ws_port->BSHR = ws_set_mask;
    ws_dly(high);
    ws_port->BSHR = ws_clr_mask;
    ws_dly(ws_bit_iters - high);
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
