# Prism 11.1.1 合并记录

日期：2026-10-25。

- Luna 基线：`3e98c6466333d179a3e4ec5839981a817729d3c5`（上游 `meson`）。
- Prism 稳定版目标：`7d2d3c1ec6fcf8255ebacd02d9f792688575dfb4`（标签 `11.1.1`）。
- 合并前备份：`codex/backup-luna-before-prism-11.1.1-20261025`。
- Meson 与兼容 CMake 版本定义统一为 `11.1.1`，项目身份仍为 LunaLauncher。

## 兼容处理

逐项整合双方代码，没有通过整文件选择上游版本解决冲突。

- 保留服务端实例、动态 JS/Hangar 资源平台、共享响应与任务所有权、资源版本筛选、镜像失败回退及现有下载后端。
- 保留 PCL 整合包转换及实例设置、LunaUI、CLI/MCP 和 Neo UI 的无界面交互路径。
- 上游新增的不可信模组确认在 GUI 中使用原对话框，在无界面模式中通过统一交互请求处理；非交互导入不能默认为信任。
- 新增 `UntrustedModsDialog` 的源码、UI、moc 接入 Meson；Windows GPU 探测补充 DXGI 链接。维持 Luna 已移除 `vcpkg.json` 的状态。
- 保留资源校验与缓存策略、安装目录翻译扫描、PO/QM 及 `!` 翻译覆盖，恢复启动时立即加载已保存语言。
- 保留 macOS 可用内存统计以兼容 Neo UI 内存展示；同时采用新的内存压力告警，并接回无界面确认。
- 让实例覆盖设置显式写入与当前全局值相同的值，避免未来全局变化破坏 PCL 导入及实例设置；普通设置继续采用上游重复写入优化。
- 修正编译揭示的共享指针、虚函数、QFlags 和测试目标依赖不兼容。
- 资源文件复制成功后，即使目录刷新进入队列，也正确报告安装成功；文件校验和复制失败仍报告失败。

Neo UI 前端、PCL 转换、LunaUI 文档、Terracotta/Yukari 及 Qt/PCL/aria2 核心下载后端没有被上游同名代码替换。

## 验证

Linux x86_64，GCC 16.2.1、Qt 6.11.2、Meson 1.12.1。

- Meson 原生构建及工作区内安装成功。
- 26 项 Meson 测试通过；最终资源安装修复后再次运行 `ResourceFolderModel_test` 通过。
- 安装后 API 回归：23 项通过，1 项在线资源目录测试未启用。覆盖服务端资源/终端、LunaUI、文件路径限制、日志流、账号、资源和实例操作。
- Neo 启动认证回归通过：刷新开始、取消成功、后端仍响应。
- 整合包封面回归通过：覆盖、重复安装、自定义图标、失败回退、取消和重启后恢复。
- 新增不可信整合包回归通过：非交互拒绝、明确拒绝、取消、明确信任后继续到本地元数据夹具。
- Neo UI 类型检查和前端生产构建通过。
- 5 项 Node API 客户端测试通过。

现有回归中与当前 Luna 接口不一致的断言已修正：任务写请求排队、账户快照的 `profile.skin`、Neo UI 使用的实例/控件对象形式的 LunaUI 接口、不同实例图标内容比较、Base64 披风序列化。仍验证原有业务行为和失败路径。

原生验证配置：

```fish
meson setup build-prism-11.1.1 \
  --prefix=$PWD/install-prism-11.1.1 --buildtype=debugoptimized \
  -Dbuild_testing=true -Dbuild_pcl_download=false -Dbuild_yukari_connect=false \
  -Dcurseforge_webview=disabled -Dneo_ui=disabled -Dyggdrasil=disabled
meson compile -C build-prism-11.1.1 -j 10
meson install -C build-prism-11.1.1 --no-rebuild
env QT_QPA_PLATFORM=offscreen meson test -C build-prism-11.1.1 --no-rebuild
python3 tests/api_sidecar_smoke.py --exe install-prism-11.1.1/bin/lunalauncher
python3 tests/neo_launch_auth_regression.py install-prism-11.1.1/bin/lunalauncher
python3 tests/modpack_icon_regression.py --exe install-prism-11.1.1/bin/lunalauncher
python3 tests/untrusted_modpack_regression.py --exe install-prism-11.1.1/bin/lunalauncher
node --experimental-strip-types --test tests/launcher_api_client.test.mjs
```

构建选项只影响本次验证目录，生产默认开关未改。Neo UI 类型检查和前端构建使用仓库锁定依赖，原生 Tauri 宿主因本机缺少 WebKitGTK/GTK 开发依赖未构建；Windows/macOS 原生行为、真实微软认证、实际联机及外部 .NET/Rust 辅助组件没有在本次验证。源码审查及后端测试不替代这些平台和集成运行验证。
