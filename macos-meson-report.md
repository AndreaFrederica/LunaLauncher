# macOS Meson 构建适配报告

## 1. 项目背景

- 构建系统：Meson（主）/ CMake（旧）
- Qt 版本：6.10.1（通过 aqt 安装到 `third_party/qt/6.10.1/macos/`）
- Meson 版本：1.11.1
- 系统：macOS 27.0 (arm64)

Qt 以 framework 形式安装，目录结构：
```
third_party/qt/6.10.1/macos/lib/
├── QtCore.framework/
│   ├── Headers/      ← 头文件
│   └── QtCore        ← 二进制文件（Mach-O）
├── QtNetwork.framework/
│   ├── Headers/
│   └── QtNetwork     ← 二进制文件
...
```

## 2. 三个阻塞性问题

### 问题 A：Qt6 cmake dependency 的 framework include 路径错误

**现象：**
```
QtNetwork.framework/QtNetwork:1:1: error: source file is not valid UTF-8
```

**根因：**

Qt6 的 cmake config 文件把 framework 根目录（含二进制）和 Headers 子目录都加到了 `INTERFACE_INCLUDE_DIRECTORIES`：

```cmake
# third_party/qt/6.10.1/macos/lib/cmake/Qt6Network/Qt6NetworkTargets.cmake:65
INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/lib/QtNetwork.framework/Headers;${_IMPORT_PREFIX}/lib/QtNetwork.framework"
```

Meson 的 cmake dependency provider 忠实地把这两个路径都传给了编译器：
```
c++ ... -I.../QtNetwork.framework -I.../QtNetwork.framework/Headers ...
```

当代码 `#include <QtNetwork/QNetworkAccessManager>` 时，编译器先搜索 `-I.../QtNetwork.framework`，在其中找到二进制文件 `QtNetwork.framework/QtNetwork`（不是头文件），尝试解析为 C++ 源码。

**为什么只有 cmake 方法可用：**

| 方法 | 结果 |
|------|------|
| `method: 'cmake'` | ✅ 找到 Qt，但 include 路径有上述 bug |
| `method: 'config-tool'` | ❌ Meson 1.11.1 对 Qt6 不执行，日志无 qmake 记录 |
| `method: 'pkg-config'` | ❌ aqt 安装的 Qt 无 `.pc` 文件 |
| `method: 'auto'` | 顺序：pkg-config → cmake，不试 config-tool |

config-tool 失败的 meson-log（无 qmake 尝试）：
```
Run-time dependency qt6core found: NO
# 无任何 qmake 相关日志
```

auto 方法的 meson-log（明确走 cmake）：
```
LOG: Determining dependency 'Qt6Core' with pkg-config executable '.../pkg-config'
LOG: Called: `pkg-config --modversion Qt6Core` -> 1
LOG: No package 'Qt6Core' found
LOG: Determining dependency 'Qt6Core' with CMake executable '.../cmake'
LOG: Guessed CMake target 'Qt6::Core'
LOG: WARNING: CMake: Dependency -framework IOKit for Qt6Core was not found
LOG: WARNING: CMake: Dependency -framework DiskArbitration for Qt6Core was not found
LOG: WARNING: CMake: Dependency -framework UniformTypeIdentifiers for Qt6Core was not found
LOG: Run-time dependency qt6core found: YES 6.10.1
```

注意：Meson 的 cmake provider 也无法识别 `-framework IOKit` 等 macOS framework 依赖，直接跳过。

**meson.build 中的 Qt 依赖声明（当前）：**
```meson
qt_core_dep = dependency('Qt6Core', method: 'cmake', required: true)
qt_widgets_dep = dependency('Qt6Widgets', method: 'cmake', required: true)
qt_network_dep = dependency('Qt6Network', method: 'cmake', required: true)
# ... 其他 Qt 模块同理
```

**所有 Qt 模块的 Targets.cmake 都有同样的问题（以 Qt6Core 为例）：**
```cmake
# third_party/qt/6.10.1/macos/lib/cmake/Qt6Core/Qt6CoreTargets.cmake
INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/lib/QtCore.framework/Headers;${_IMPORT_PREFIX}/lib/QtCore.framework"
```

---

### 问题 B：moc 看不到 Q_OS_MACOS，导致 signal 未生成

**现象：**
```
Undefined symbols for architecture arm64:
  "Application::clickedOnDock()", referenced from:
      Application::Application(int&, char**) in libLauncher_logic.a(Application.cpp.o)
      Application::event(QEvent*) in libLauncher_logic.a(Application.cpp.o)
ld: symbol(s) not found for architecture arm64
```

**根因代码：**

`launcher/Application.h:204-214`：
```cpp
   signals:
    void updateAllowedChanged(bool status);
    void globalSettingsAboutToOpen();
    void globalSettingsApplied();
    int currentCatChanged(int index);
    void oauthReplyRecieved(QVariantMap);

#ifdef Q_OS_MACOS
    void clickedOnDock();
#endif
```

`Q_OS_MACOS` 定义在 Qt 的 `<qglobal.h>` 中，依赖链：
```cpp
// qglobal.h
#ifdef __APPLE__
#  include <TargetConditionals.h>
#  if TARGET_OS_OSX
#    define Q_OS_MACOS
#  endif
#endif
```

moc 工具运行时需要：
1. 看到 `-D__APPLE__`（已通过 `apple_define_dep` 传入）
2. 能找到 `<qglobal.h>`（Qt include 路径已设置）
3. 能找到 `<TargetConditionals.h>`（Apple SDK 系统头文件）

**当前 meson.build 中的 moc 配置：**
```meson
# meson.build 根文件
if is_macos
  apple_define_dep = declare_dependency(compile_args: ['-D__APPLE__'])
else
  apple_define_dep = dependency('', required: false)
endif

# launcher/meson.build
launcher_moc_header_generated = qt.preprocess(
  moc_headers: files(launcher_moc_headers),
  include_directories: [launcher_inc, launcher_minecraft_inc, ...],
  dependencies: [qt_core_dep, qt_widgets_dep, ..., apple_define_dep],
  preserve_paths: true,
)
```

**moc 实际命令（从 build.ninja 提取）：**
```
/Users/.../third_party/qt/6.10.1/macos/libexec/moc --output-dep-file \
  -I/Users/.../launcher/. \
  -I/Users/.../launcher/include \
  ... (项目 include 路径) ...
  -DQT_CORE_LIB -DQT_NO_DEBUG \
  -F/Users/.../third_party/qt/6.10.1/macos/lib \
  -I/Users/.../third_party/qt/6.10.1/macos/include \
  -I/Users/.../third_party/qt/6.10.1/macos/lib/QtCore.framework/Headers \
  ... (其他 Qt 模块的 Headers 路径) ...
  -D__APPLE__ \
  ../launcher/Application.h \
  -o launcher/libLauncher_logic.a.p/moc_Application.cpp
```

`-D__APPLE__` 在命令末尾，Qt Headers 路径也已设置。但 moc 输出中不包含 `clickedOnDock`：
```bash
grep "clickedOnDock" build-meson-release/launcher/libLauncher_logic.a.p/moc_Application.cpp
# 输出为空
```

**手动测试 moc：**
```bash
# 直接用 -D__APPLE__ 测试，能生成
echo '#include <QObject>
#ifdef __APPLE__
class T : public QObject { Q_OBJECT signals: void s(); };
#endif' | moc -D__APPLE__ -I.../QtCore.framework/Headers /dev/stdin
# 输出包含 s() 的实现

# 但 Application.h 中用的是 #ifdef Q_OS_MACOS，不是 #ifdef __APPLE__
# 问题可能是 moc 的预处理器无法完成 __APPLE__ → TargetConditionals.h → Q_OS_MACOS 的链式解析
```

**与问题 A 的关系：** moc 命令中 `-I.../QtCore.framework/Headers` 路径是正确的，但 `<qglobal.h>` 内部 `#include <TargetConditionals.h>` 可能因为缺少系统 framework 搜索路径（`-F` 或 `-iframework`）而失败，导致 `Q_OS_MACOS` 未定义。

---

### 问题 C：cmake provider 无法识别 macOS framework 依赖

**现象（configure 时）：**
```
WARNING: CMake: Dependency -framework IOKit for Qt6Core was not found
WARNING: CMake: Dependency -framework DiskArbitration for Qt6Core was not found
WARNING: CMake: Dependency -framework UniformTypeIdentifiers for Qt6Core was not found
WARNING: CMake: Dependency -framework AppKit for Qt6Widgets was not found
WARNING: CMake: Dependency -framework OpenGL for Qt6Widgets was not found
WARNING: CMake: Dependency -framework Security for Qt6Test was not found
```

每个 Qt 模块都报了类似的缺失 framework。Meson 的 cmake provider 把 `-framework AppKit` 当作一个普通包名去查找，自然找不到，就跳过了。

这些 framework 最终需要在链接时提供。当前 workaround 是在 `launcher/meson.build` 中手动添加：
```meson
link_args: is_macos ? ['-framework', 'AppKit', '-framework', 'Carbon',
                        '-framework', 'Foundation', '-framework', 'ApplicationServices'] : []
```

但这只覆盖了项目直接依赖的 framework，Qt 自身依赖的 IOKit、DiskArbitration 等没有处理。

---

## 3. 非阻塞性问题

### qyieldcpu.h 的 __yield 警告
```
qyieldcpu.h:37:5: warning: implicitly declaring library function '__yield'
note: include the header <arm_acle.h>
```
Qt 6.10 头文件 bug，Qt 6.11.1/6.12 已修复。当前通过 `-Wno-error=implicit-function-declaration` 压制。

### deployment target 不匹配
```
ld: warning: object file ... was built for newer macOS version (27.0) than being linked (13.0)
```
通过 `MACOSX_DEPLOYMENT_TARGET` 环境变量统一设置，可在 `pixi_meson.py` 的 `task_env()` 中配置，默认值 27.0。

---

## 4. 测试脚本

`test_qt_meson.py` — 测试 config-tool 方法：
```python
#!/usr/bin/env python3
import os, subprocess
QT_BIN = "third_party/qt/6.10.1/macos/bin"
env = os.environ.copy()
env["PATH"] = os.pathsep.join([os.path.abspath(QT_BIN)] + env["PATH"].split(os.pathsep))
test_dir = "/tmp/meson_qt_test"
os.makedirs(test_dir, exist_ok=True)
with open(os.path.join(test_dir, "meson.build"), "w") as f:
    f.write("""project('test', 'cpp')
qt_core = dependency('Qt6Core', method: 'config-tool', required: true)
message('Qt6Core found: ' + qt_core.found().to_string())
""")
build_dir = os.path.join(test_dir, "build")
result = subprocess.run(["meson", "setup", build_dir, test_dir],
                        capture_output=True, text=True, env=env)
print(f"returncode: {result.returncode}")
print(f"stdout:\n{result.stdout[-300:]}")
```

`test_qt_meson2.py` — 测试 auto 方法（显示完整日志）：
```python
#!/usr/bin/env python3
import os, subprocess, shutil
QT_BIN = "third_party/qt/6.10.1/macos/bin"
env = os.environ.copy()
env["PATH"] = os.pathsep.join([os.path.abspath(QT_BIN)] + env["PATH"].split(os.pathsep))
env["MACOSX_DEPLOYMENT_TARGET"] = "27.0"
test_dir = "/tmp/meson_qt_test2"
os.makedirs(test_dir, exist_ok=True)
with open(os.path.join(test_dir, "meson.build"), "w") as f:
    f.write("""project('test', 'cpp')
qt_core = dependency('Qt6Core', method: 'auto', required: true)
message('Qt6Core found: ' + qt_core.found().to_string())
""")
build_dir = os.path.join(test_dir, "build")
if os.path.exists(build_dir): shutil.rmtree(build_dir)
result = subprocess.run(["meson", "setup", build_dir, test_dir],
                        capture_output=True, text=True, env=env)
print(f"returncode: {result.returncode}")
log_path = os.path.join(build_dir, "meson-logs", "meson-log.txt")
if os.path.exists(log_path):
    with open(log_path) as f:
        for line in f:
            if any(k in line.lower() for k in ["qmake", "config", "qt6core", "found"]):
                print(f"LOG: {line.rstrip()}")
```

---

## 5. 关键问题总结

1. **Meson 的 cmake provider 在 macOS 上无法正确处理 Qt6 framework 的 include 路径** — 把 framework 根目录（含二进制）加到了 `-I` 路径
2. **Meson 的 cmake provider 无法识别 `-framework XXX` 形式的依赖** — 直接跳过
3. **Meson 1.11.1 对 Qt6 的 config-tool 方法不工作** — 日志中无 qmake 尝试记录
4. **moc 看不到 `Q_OS_MACOS`** — 可能与问题 1 相关，`<TargetConditionals.h>` 找不到

需要确认：
- Meson 对 Qt6 + macOS framework 是否有已知限制或 workaround？
- config-tool 方法在什么条件下对 Qt6 可用？
- 有没有办法让 Meson 的 cmake provider 正确处理 macOS framework？
