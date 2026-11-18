# 11-led-test：User LED Blink + WS2812 RGB 测试

覆盖 F&P **项10**（User_LED Blink）+ **项12**（RGB_LED 颜色切换），一个固件两项。

- 实施依据（规格书、验收标准）：测试仓库 `awesome-se/xiao-testplan/xiao-ch32x035-test-plan` 的 `docs/sample-specs/11-led-test-spec.md`
- 本目录是该仓库 `11-led-test/` 的镜像；改动先改测试仓库，再同步过来
- 与镜像源的差别仅一处：BSP 别名头 `xiao_ch32x035.h` 随本目录 `src/` 自带（源头在测试仓库 `bsp/`，经 `-I../bsp` 引用），board json 在本平台仓库 `boards/`

## 固件行为

- **User LED**（D7 绿，PA4 低有效）：1Hz 闪烁（500ms 亮 / 500ms 灭），上电即跑
- **WS2812 RGB**（WS2812B-2020-V6，DIN=PB8）：红→绿→蓝→白→灭循环，每色 1s，插 USB 即可目视判定
- **调试打印**：USART1 TX=PB10（XIAO D6），115200 8N1，每换色一行 `RGB: <COLOR> frame=<n>`，仅打印不接收
- **驱动**：bit-bang——每帧 30µs 关中断，`mcycle` CSR 周期锚定脉宽，BSHR 原子沿；时序常量在 `src/ws2812_bitbang.h` 顶部单点可调

## 编译

```bash
cd examples/xiao-ch32x035/11-led-test
pio run                                   # LinkE 烧录环境
pio run -e seeed_xiao_ch32x035_isp        # ROM USB ISP 环境
pio run -e seeed_xiao_ch32x035_uf2        # UF2 拖拽环境（0x4000 重链接）
```

参考体积：Flash 9.6K/62K（15%），RAM 0.5K/20K。

## 烧录

| 方式 | 操作 |
| --- | --- |
| WCH-LinkE | SWDIO→TP6(PC18)、SWCLK→TP5(PC19)、GND→TP8，`pio run -t upload` |
| ROM USB ISP | 按住 K2(BOOT) 插 USB，`pio run -e seeed_xiao_ch32x035_isp -t upload` |
| UF2 拖拽 | 先刷一次 TinyUF2 bootloader（ch32x035 port），之后 `pio run -e seeed_xiao_ch32x035_uf2` + `python ..\..\tools\mkuf2.py .pio\build\seeed_xiao_ch32x035_uf2\firmware.bin`，把 `.uf2` 拖到 `XIAO035BOOT` 盘 |

验证：User LED 1Hz 闪、RGB 四色+灭每色 1s；串口工具 RX 接 D6/PB10（共地，115200）每秒一行，行序与目视颜色一致。
