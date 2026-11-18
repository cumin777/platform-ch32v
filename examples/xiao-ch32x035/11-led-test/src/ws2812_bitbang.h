/*
 * ws2812_bitbang.h - bit-banged WS2812 driver for a single LED (no cascade)
 *
 * Implementation route A of docs/sample-specs/11-led-test-spec.md §2:
 * PB8 has no SPI / positive TIM PWM channel, so DIN is driven by plain
 * GPIO with mcycle-anchored busy waits. One frame (24 bits * 1.25us = 30us)
 * is sent with interrupts masked; the reset latch is waited out afterwards.
 *
 * Designed for later reuse in the 20b AT firmware: the module only needs
 * a GPIO port/pin pair and F_CPU-correct cycle constants.
 */
#ifndef WS2812_BITBANG_H
#define WS2812_BITBANG_H

#include <stdint.h>

/*
 * Timing constants at F_CPU = 48 MHz (1 cycle = 20.83 ns).
 * Nominals follow the spec §3: T0H 300ns, T1H 700ns, bit period 1.25us.
 * WS2812B-2020-V6 acceptance windows: T0H 220..380ns, T1H 580..1000ns,
 * bit period 1.25us +/- 600ns. Tune here first if a scope disagrees.
 */
#define WS2812_T0H_CYCLES   14  /* ~300 ns high for a 0 bit   */
#define WS2812_T1H_CYCLES   34  /* ~700 ns high for a 1 bit   */
#define WS2812_BIT_CYCLES   60  /* 1.25 us total bit period   */
#define WS2812_RESET_US     500 /* latch: V6 needs >280us low */

/* GPIO_TypeDef comes from the device header via Arduino.h */
#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bind the driver to one GPIO. The caller has already configured the pin
 * as push-pull output (pinMode) and driven it low; this only caches the
 * BSHR set/reset masks used by the bit loop.
 */
void ws2812_attach(GPIO_TypeDef *port, uint8_t pin_num);

/*
 * Send one RGB frame to a single WS2812 and wait out the reset latch.
 * Blocks for 24 * 1.25us with interrupts masked, then ~WS2812_RESET_US
 * with interrupts enabled. Leaves DIN low (LED keeps showing the color).
 */
void ws2812_send_rgb(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif /* WS2812_BITBANG_H */
