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

static uint32_t frame_count;     /* RGB frames sent since boot */

void setup()
{
    Serial.begin(115200);        /* USART1, TX = PB10 = XIAO_D6 */
    led_user_init();             /* PA4 active-low LED, starts OFF */

    pinMode(RGB_LED_DIN, OUTPUT);
    digitalWrite(RGB_LED_DIN, LOW);
    ws2812_attach(GPIOB, 8);     /* PB8 == RGB_LED_DIN, see bsp header */

    Serial.println();
    Serial.println("11-led-test: user LED 1Hz blink + RGB cycle (F&P 10+12)");
}

void loop()
{
    /* Color-step timer starts "one step ago" so the first frame (RED)
     * goes out immediately at boot instead of after a dark second. */
    static uint32_t last_blink = 0;
    static uint32_t last_color = (uint32_t)-(int32_t)COLOR_STEP_MS;
    static uint8_t color_idx = 0;
    static uint8_t user_led_on = 0;
    uint32_t now = millis();

    /* User LED: toggle every 500ms, independent of the RGB cycle */
    if (now - last_blink >= USER_LED_BLINK_MS) {
        last_blink += USER_LED_BLINK_MS;
        user_led_on ^= 1;
        digitalWrite(LED_USER, user_led_on ? LED_USER_ON : LED_USER_OFF);
    }

    /* RGB: advance the color wheel every 1000ms and log one line */
    if (now - last_color >= COLOR_STEP_MS) {
        last_color += COLOR_STEP_MS;

        ws2812_send_rgb(colors[color_idx].r,
                        colors[color_idx].g,
                        colors[color_idx].b);

        Serial.print("RGB: ");
        Serial.print(colors[color_idx].name);
        Serial.print(" frame=");
        Serial.println(frame_count);
        frame_count++;

        color_idx = (color_idx + 1) % COLOR_COUNT;
    }
}
