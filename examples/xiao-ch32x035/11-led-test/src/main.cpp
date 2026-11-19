/*
 * 11-led-test - F&P item 10 (User LED blink) + item 12 (WS2812 RGB cycle)
 *
 * Behavior (docs/sample-specs/11-led-test-spec.md §4):
 *  - User LED D7 (PA4, active-LOW): 1 Hz blink, 500ms on / 500ms off,
 *    running from boot, independent of the RGB cycle.
 *  - WS2812B-2020-V6 on PB8: red -> green -> blue -> white -> off,
 *    one second per step, forever. Visible by just plugging USB.
 *  - USART1 on PB10 (XIAO D6) 115200 8N1 prints one line per color
 *    change: "RGB: <COLOR> frame=<n>". TX only, no command interface.
 *
 * Boot self-test, LED language on the green User LED (v3). v1 hung right
 * after start; v2 froze between the first blinks and the mcycle probe,
 * leaving two suspects: Serial.println blocking, or csrr mcycle raising
 * an illegal-instruction trap on the QingKe V4C. v3 removes BOTH from
 * the early boot path and separates the stages with blink markers:
 *   3 quick blinks  = core + GPIO + millis alive (v1/v2 got this far)
 *   2 quick blinks  = delay loop calibrated (millis-only path)
 *   RGB WHITE flash = one WS2812 frame sent and latched
 *   2 quick blinks  = frame done, about to touch Serial for the 1st time
 *   2 quick blinks  = Serial prints survived (freezing here = UART issue)
 * then normal operation: green 1 Hz + RGB color wheel + per-step prints.
 * No csrr anywhere: the v1 driver anchored pulses on mcycle and hung;
 * v2+ calibrates a plain delay loop against millis() instead.
 *
 * On-board verification passed 2026-11-19 (all boot stages, full color
 * wheel, 1 Hz user LED, serial log). Keep the boot LED language: it is
 * the fastest way to tell "app dead" from "board in bootloader DFU".
 */
#include <Arduino.h>
#include "xiao_ch32x035.h"      /* BSP: LED_USER, RGB_LED_DIN, ...   */
#include "ws2812_bitbang.h"     /* bit-bang route, spec §2 route A   */

/* Color wheel (spec §4.2): step every COLOR_STEP_MS, cyclic */
static const struct {
    const char *name;
    uint8_t r, g, b;
} colors[] = {
    { "RED",   255,   0,   0 },
    { "GREEN",   0, 255,   0 },
    { "BLUE",    0,   0, 255 },
    { "WHITE", 255, 255, 255 },
    { "OFF",     0,   0,   0 },
};
#define COLOR_COUNT  (sizeof(colors) / sizeof(colors[0]))

#define USER_LED_BLINK_MS  500   /* 1 Hz: 500ms on / 500ms off */
#define COLOR_STEP_MS      1000  /* one color per second       */

/* Runtime state (anchored in setup(), never negative-init: see the v5
 * note above the loop) */
static uint32_t g_last_toggle;
static uint8_t g_user_led_on;
static uint32_t frame_count;     /* RGB frames sent since boot */

static void blink_green(uint8_t times, uint16_t on_ms, uint16_t off_ms)
{
    while (times--) {
        digitalWrite(LED_USER, LED_USER_ON);
        delay(on_ms);
        digitalWrite(LED_USER, LED_USER_OFF);
        delay(off_ms);
    }
}

void setup()
{
    /* No Serial, no CSR reads before the LED-marked stages below. */
    led_user_init();             /* PA4 active-low LED, starts OFF */
    pinMode(RGB_LED_DIN, OUTPUT);
    digitalWrite(RGB_LED_DIN, LOW);
    ws2812_attach(GPIOB, 8);     /* PB8 == RGB_LED_DIN, see bsp header */

    blink_green(3, 50, 100);     /* stage 1: core + GPIO + millis */

    ws2812_calibrate();
    blink_green(2, 50, 100);     /* stage 2: delay loop calibrated */

    ws2812_send_rgb(255, 255, 255);
    delay(400);
    ws2812_send_rgb(0, 0, 0);
    blink_green(2, 50, 100);     /* stage 3: WS2812 frame sent */

    Serial.begin(115200);        /* USART1, TX = PB10 = XIAO_D6 */
    Serial.println();
    Serial.println("11-led-test: user LED 1Hz blink + RGB cycle (F&P 10+12)");
    Serial.print("ws2812 delay ");
    Serial.print(ws2812_iters_per_ms());
    Serial.println(" iters/ms");
    blink_green(2, 50, 100);     /* stage 4: Serial prints survived */

    g_last_toggle = millis();    /* deadline anchored AFTER setup, never
                                    a negative constant (see v5 note) */
}

/* Diagnostic build (v4) verified the driver on hardware: all 8 channel /
 * level test frames rendered correctly. v3's "only RED ever shows" was
 * traced to a burst defect: the millis()-scheduled loop initialized
 * last_color = -COLOR_STEP_MS assuming loop() starts at t=0, but the boot
 * self-test blinks ran ~1.6s, so the first pass fired three frames
 * back-to-back within one millisecond (last_color += 1000 can never catch
 * up with now). The WS2812 apparently locks onto the last frame it
 * accepted (RED) and rejects everything after the burst. v5 therefore
 * uses a blocking per-color loop: frame -> hold 1s -> print -> next.
 * Rule for every future sample: NEVER init a millis() deadline to a
 * negative/zero constant when setup() has consumed measurable time. */
static void user_led_tick(void)
{
    if (millis() - g_last_toggle >= USER_LED_BLINK_MS) {
        g_last_toggle += USER_LED_BLINK_MS;
        g_user_led_on ^= 1;
        digitalWrite(LED_USER, g_user_led_on ? LED_USER_ON : LED_USER_OFF);
    }
}

void loop()
{
    static uint8_t color_idx = 0;

    ws2812_send_rgb(colors[color_idx].r,
                    colors[color_idx].g,
                    colors[color_idx].b);

    /* hold the color for COLOR_STEP_MS, user LED toggling at 2 Hz inside */
    int32_t remaining = COLOR_STEP_MS;
    while (remaining > 0) {
        uint32_t seg = (remaining > 50) ? 50 : (uint32_t)remaining;
        delay(seg);
        user_led_tick();
        remaining -= seg;
    }

    Serial.print("RGB: ");
    Serial.print(colors[color_idx].name);
    Serial.print(" frame=");
    Serial.println(frame_count);
    frame_count++;

    color_idx = (color_idx + 1) % COLOR_COUNT;
}
