/*
 * ws2812_bitbang.h - bit-banged WS2812 driver for a single LED (no cascade)
 *
 * Implementation route A of docs/sample-specs/11-led-test-spec.md §2:
 * PB8 has no SPI / positive TIM PWM channel, so DIN is driven by plain
 * GPIO busy waits. One frame (24 bits * 1.25us = 30us) is sent with
 * interrupts masked; the reset latch is waited out afterwards.
 *
 * v2: pulses no longer anchor on the mcycle CSR. On the QingKe V4C neither
 * the official EVT nor the openwch core ever reads mcycle, and the first
 * on-board run hung exactly in the mcycle-anchored wait loop (app dead
 * right after start, 2026-11-18). Instead the delay loop is calibrated at
 * boot against millis(): whatever cycle cost one loop iteration really has
 * (pipeline, flash waits included) is measured, not assumed.
 */
#ifndef WS2812_BITBANG_H
#define WS2812_BITBANG_H

#include <stdint.h>

/*
 * Nominal timings (spec §3) and the acceptance windows of the
 * WS2812B-2020-V6: T0H 220..380ns, T1H 580..1000ns, bit period
 * 1.25us +/- 600ns. High-pulse targets are set slightly below nominal to
 * absorb the fixed store overhead inside the pulse (~2..4 cycles).
 */
#define WS2812_T0H_NS   280
#define WS2812_T1H_NS   680
#define WS2812_BIT_NS   1250
#define WS2812_RESET_US 500 /* latch: V6 needs >280us of DIN low */

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
 * Calibrate the delay loop against millis(). Call once from setup() after
 * Arduino time keeping is running, before the first ws2812_send_rgb().
 * Blocks ~60ms.
 */
void ws2812_calibrate(void);

/* Iterations per millisecond measured by ws2812_calibrate(). */
uint32_t ws2812_iters_per_ms(void);

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
