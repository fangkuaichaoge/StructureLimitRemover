# StructureLimitRemover

去掉 Minecraft 结构方块 64x64x64 大小上限（改为 999x999x999）的独立原生 mod。

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
