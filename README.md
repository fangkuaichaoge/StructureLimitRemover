# StructureLimitRemover

去掉 Minecraft 结构方块 64x64x64 大小上限（改为 999x999x999）的独立原生 mod。

这个版本**不依赖 preloader / LeviLauncher SDK**：整个补丁逻辑自包含在一个
`.so` 里，加载时通过 ELF 构造函数（`.init_array`）自动执行——自己读
`/proc/self/maps` 找 `libminecraftpe.so`，搜通配符签名，校验原始字节后用
`mprotect` + `memcpy` 打补丁。

## 加载方式

- **LeviLauncher**：按 `manifest.json`（`preload-native`）放入 mods 目录即可。
  preloader 用 `dlopen` 加载库，构造函数在 `dlopen` 时就会执行；即使没有
  `PL_REGISTER_MOD` 生命周期入口，也只是打一条警告，库会继续保留在进程里。
- 其它环境：`LD_PRELOAD`、JNI、或任何 `dlopen` 方式加载都会触发构造函数。

## 补丁点

同框架版本一致，共 6 处（两个函数，全部 64 → 999）：

1. `sub_10056C48`：尺寸校验 helper（UI 保存 / `/structure` 命令 / 脚本管理器 /
   服务端校验共用）——maxX/maxY/maxZ。
2. `sub_100572C8`：`StructureEditorData::setSize` 客户端钳制——sizeX/sizeZ 的
   上限比较和钳制常量。

签名带 `??` 通配符，对寄存器分配差异更宽容；原始字节校验不过就安全跳过。

## 构建

Windows：

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

Linux / macOS：

```bash
export ANDROID_NDK_HOME=/path/to/ndk
bash build.sh
```

产物：`build/libStructureLimitRemover.so`（arm64-v8a，无任何第三方依赖）。

## 许可证

[MIT](LICENSE)
