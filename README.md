# StructureLimitRemover

[![build](https://github.com/fangkuaichaoge/StructureLimitRemover/actions/workflows/build.yml/badge.svg)](https://github.com/fangkuaichaoge/StructureLimitRemover/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

一个 LeviLauncher（LeviLamina Android）原生 mod：把 Minecraft 结构方块的
64x64x64 大小上限改成 999x999x999，保存、导出、导入、`/structure` 命令全部生效。

> 适用于 libminecraftpe.so **26.40 / 2.64.0**（arm64-v8a）。

## 功能

- 结构方块界面可以输入并保存最大 999x999x999 的结构
- `/structure` 命令保存/加载不再限制 64
- 脚本结构管理器（`StructureManager`）同样放开
- 导入 `.mcstructure` 文件后应用到大尺寸
- 签名带 `??` 通配符，对编译器寄存器分配的细微差异更宽容

## 原理

限制由两处代码共同构成，mod 各打一组签名补丁（64 → 999）：

### 1. 尺寸校验 helper `sub_10056C48`

被四条路径共用：结构方块界面保存、`/structure` 命令、脚本结构管理器、服务端
结构方块校验。它返回三个方向的最大尺寸：

```asm
SXTH W8, W1
MOV  W9, #0x40          ; maxX = 64
MOV  W1, #0x40          ; maxZ = 64
SUB  W8, W8, W0,SXTH    ; maxY
ORR  X0, X9, X8,LSL#32
RET
```

三个 64 全部改成 999（`MOV Wn, #0x3E7`）。

### 2. 编辑器尺寸钳制 `sub_100572C8`（客户端关键）

结构方块界面输入/应用尺寸时，`StructureEditorData::setSize` 会把 X 和 Z 钳到
[1, 64]（Y 不钳，所以大结构会显示成 "64 x 258 x 64"）。这个函数被客户端界面和
服务端方块实体共用：

```asm
MOV  W12, #0x40         ; 钳制值 64
CMP  W9,  #0x40         ; sizeX > 64 ?
CMP  W13, #0x40         ; sizeZ > 64 ?
```

同样全部改成 999。两处都打上，输入、保存、导出、导入才都能用 999。

## 构建

### GitHub Actions（推荐）

仓库自带 `.github/workflows/build.yml`，push 后自动用 NDK r29 构建，产物
`StructureLimitRemover.zip`（含 `manifest.json` 和 `libStructureLimitRemover.so`）
会作为 artifact 上传。

### 本地构建

需要 Android SDK cmake、ninja、NDK（r29 或相近版本）。

Windows：

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

Linux / macOS：

```bash
export ANDROID_NDK_HOME=/path/to/ndk
bash build.sh
```

本机如果有 `vendor/` 目录（含各依赖副本），构建完全离线；没有则自动走
FetchContent 联网拉取。`vendor/preloader-android`（LeviLauncher mod SDK）已提交
在仓库内，其余第三方依赖通过 `.gitignore` 排除。

产物：`build/libStructureLimitRemover.so`（arm64-v8a）。

## 安装

把 `StructureLimitRemover/` 目录（`manifest.json` + `libStructureLimitRemover.so`）
放进 LeviLauncher 的 mods 目录，启动游戏即可。

mod 不写任何日志，补丁静默执行；签名不匹配时自动跳过该处，不影响启动。

## 免责声明

- 补丁针对 26.40 / 2.64.0 版本；游戏更新后签名可能失效，mod 会安全跳过并写日志。
- 超过世界高度/边界的结构仍会被游戏自身拒绝，这是正常行为。
- 超大结构（如 999^3 方块）可能造成明显卡顿或内存压力，请谨慎使用。

## 许可证

[MIT](LICENSE)
