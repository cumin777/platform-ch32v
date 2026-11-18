# CLAUDE.md

本仓库是 [Community-PIO-CH32V/platform-ch32v](https://github.com/Community-PIO-CH32V/platform-ch32v) 的 fork，除跟随上游外承担两件事：

- `boards/seeed_xiao_ch32x035.json` — Seeed XIAO CH32X035 板定义（**唯一正本**，上游无此板）
- `examples/xiao-ch32x035/` — XIAO 测试 sample 镜像（源头在测试仓库 `awesome-se/xiao-testplan/xiao-ch32x035-test-plan`，改动先改源头再同步过来；`ch32x035_uf2.ld` 为该目录共享文件）
- `tools/mkuf2.py` — 固件转 UF2 打包脚本

## cumin 的开发习惯

**交流与文档**

- 中文交流；结论/方案先行，依据随后；不写总结性尾段和客套话
- 代码注释一律英文（源码 / 构建脚本 / 链接脚本 / 配置文件），README 等文档用中文
- 文档先行：写 sample 前先在测试仓库出 `docs/sample-specs/NN-*.md` 实施文档并拍板，代码照文档写；决策变更回写文档，不留只在对话里的决定

**构建与交付**

- 交付必须本机 `pio run` 编译通过才算完成；README 里的环境版本写实测值（`pio pkg list`），不写"应该兼容"
- sample 命名 `NN-名称`；固件三种烧录通道齐备才算交付：WCH-LinkE（SDI 两线 TP5/TP6/TP8）、ROM USB ISP（按住 K2 插 USB）、TinyUF2 UF2 拖拽
- 一切 demo 固件要出 UF2：`*_uf2` env（0x4000 重链接）+ `tools/mkuf2.py` 打包，流程见测试仓库 `docs/uf2-sop.md`
- 交付 Release 时 `.bin`（0x4000 版）与 `.uf2` 一并附上

**代码边界**

- 不改 Arduino core（framework-arduino-openwch-ch32）：板级差异一律走 board json + 别名头 `xiao_ch32x035.h` 解决
- 裸机外设寄存器行为的 ground truth 是官方 EVT（`ch32x035_eval`），不确定先查它

**分支与同步**

- 工作分支 `develop`（本 fork 默认分支）；定期 merge 上游 develop，再确认本地 `boards/`、`examples/xiao-ch32x035/`、`tools/` 完整
- 环境：Windows + PowerShell，PlatformIO Core 6.2.0；WCH-LinkE 固件 ≥2.11
