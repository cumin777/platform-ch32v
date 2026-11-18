/*
 * XIAO CH32X035 板级抽象（BSP）
 * 芯片: CH32X035G8U6 (QFN28, QingKe V4C RV32IMAC @48MHz, 62K flash / 20K SRAM)
 * 板:   Seeed XIAO CH32X035 V1.0a (原理图 261117 复核冻结, 见 docs/implementation-plan.md §3)
 *
 * 用法: samples 里 #include "xiao_ch32x035.h"（经 -Ibsp 或 lib 搜索路径）
 * 依赖: framework-arduino-openwch 的 stock variant CH32X035/CH32X035G8U
 *       （本头只做 XIAO 信号名 → 芯片端口别名，不修改 core）
 */
#ifndef XIAO_CH32X035_H
#define XIAO_CH32X035_H

#include "Arduino.h"

/* ================= XIAO 座 (U5, 14P: D0~D10 + 3V3/GND/VBUS) ================= */

#define XIAO_D0   PA0     /* A0, ADC_IN0 / TIM2_CH1 */
#define XIAO_D1   PA1     /* A1, ADC_IN1 */
#define XIAO_D2   PA2     /* A2, ADC_IN2 */
#define XIAO_D3   PA3     /* A3, ADC_IN3 / TIM2_CH4 (PWM 项9) */
#define XIAO_D4   PC19    /* SDA (I2C1 FullRemap) — 兼 SWCLK(SDI 调试口) */
#define XIAO_D5   PC18    /* SCL (I2C1 FullRemap) — 兼 SWDIO(SDI 调试口) */
#define XIAO_D6   PB10    /* TX  (USART1) */
#define XIAO_D7   PB11    /* RX  (USART1) */
#define XIAO_D8   PA5     /* SCK  (SPI1) */
#define XIAO_D9   PA6     /* MISO (SPI1) */
#define XIAO_D10  PA7     /* MOSI (SPI1) */

/* 板内走线、仅 TP 焊盘（B 面），不在 XIAO 座上 —— GPIO 项4 经探针测 */
#define XIAO_D11  PC3     /* TP1, 原理图标注 A4, ADC_IN13 */
#define XIAO_D12  PB0     /* TP2, 原理图标注 A5, ADC_IN8  */
#define XIAO_D13  PB3     /* TP3  */
#define XIAO_D14  PB4     /* TP4  */
#define XIAO_D15  PB1     /* TP11 — G8U6 内部 PB1≡PB5 合封, core 只提供 PB1 */
#define XIAO_D16  PB6     /* TP10 */
#define XIAO_D17  PB7     /* TP9  */

/* 板级模拟口命名（跟原理图网络名 D11/A4、D12/A5） */
#define XIAO_A0   XIAO_D0
#define XIAO_A1   XIAO_D1
#define XIAO_A2   XIAO_D2
#define XIAO_A3   XIAO_D3
#define XIAO_A4   XIAO_D11     /* PC3, TP1 焊盘 */
#define XIAO_A5   XIAO_D12     /* PB0, TP2 焊盘 */

/* ================= 板载器件（非 XIAO 信号） ================= */

/* 用户 LED D7(绿): 3V3 → R15 1.5K → LED → PA4，低电平点亮 */
#define LED_USER        PA4
#define LED_USER_ON     LOW
#define LED_USER_OFF    HIGH
#define led_user_init() do { pinMode(LED_USER, OUTPUT); digitalWrite(LED_USER, LED_USER_OFF); } while (0)

/* WS2812B-2020-V6: DIN ← PB8 (经 R17 0Ω), VDD=3V3 常供电; DO 悬空 */
#define RGB_LED_DIN     PB8

/* 电池监测: VBAT → U6 GLF72101(EN=PB9, R7 100K 下拉默认关) → R9/R10 10K+10K ÷2 → PC0(ADC_IN10)
 * 读电压: 拉高 BAT_EN → 延时 ~1ms 稳定 → analogRead(BAT_ADC) → 换算 ×2 → 拉低省电 */
#define BAT_EN          PB9
#define BAT_ADC         PC0
#define BAT_DIV_FACTOR  2       /* 分压比 1/2, 读数 ×2 = 实际电压 */
#define VREF_MV         3300

static inline uint32_t xiao_bat_mv(uint32_t raw, uint32_t full_scale)
{
    return (uint32_t)((uint64_t)raw * VREF_MV * BAT_DIV_FACTOR / full_scale);
}

/* K1 "复位"键: GND ↔ K1 ↔ RESET 网络 → R19 0Ω → PB12 (R11 10K 上拉 + C5 100nF 硬件去抖)
 * G8U6 无硬件复位引脚（EE 查证 2026-11-18），按 K1 = EXTI 下降沿 → NVIC_SystemReset 软复位
 * 软件去抖 ~20ms 后再复位 */
#define K1_SOFT_RST     PB12
#define K1_PRESSED      LOW
#define K1_DEBOUNCE_MS  20

/* K2 BOOT 键: 经 R13 4.7K 上拉 + R16 1K 耦合到 USB_DP(PC17)。
 * 上电时 PC17 为高 → 进 BOOT 区 USB ISP（DS 注8）。固件内可读 PC17 判断 K2，
 * 但 USB 使能时 PC17 被外设占用，仅在 USB 关闭场景下可靠。 */
#define K2_BOOT         PC17

/* USB FS Device: PC16=DM, PC17=DP (R3/R4 0Ω 至 Type-C) */
#define PIN_USB_DM      PC16
#define PIN_USB_DP      PC17

/* ================= 外设默认口与已知坑 =================
 * - Serial = USART1, 默认 TX=PB10/RX=PB11 = XIAO_D6/D7, Serial 直接可用
 * - Wire:  stock PinMap 默认 SDA=PC17(=USB DP!) —— 必须 Wire.begin(XIAO_D4, XIAO_D5)
 *          显式传参（I2C1 FullRemap 由 core 自动配置）；PC18/19 板上无上拉，
 *          依赖传感器板或使能内部上拉；I2C 前建议关 SDI 调试口（DS §1.4.22）
 * - SPI:   SPI1 默认 SCK=PA5/MISO=PA6/MOSI=PA7 = XIAO_D8/D9/D10, 直接可用
 * - PWM:   analogWrite(XIAO_D3) = TIM2_CH4（项9 25/50/75% 占空比测试脚）
 * - 调试:  两线 SDI: SWDIO=PC18(TP6) SWCLK=PC19(TP5) GND=TP8/TP12, WCH-Link(E) 专用
 * - 烧录:  ① WCH-LinkE (upload_protocol=wch-link)
 *          ② ROM USB ISP: 按住 K2 插 USB → upload_protocol=isp (wchisp)
 * */

#endif /* XIAO_CH32X035_H */
