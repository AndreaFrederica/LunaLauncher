# 忘却的旋律主题

独立的 NeoUI 外观，入口为 `pages/MelodyThemeApp.vue`。在 NeoUI 的外观设置中选择“忘却的旋律”，或打开 `/?theme=melody` / `/#/melody`。根组件 `NeoRoot.vue` 按主题加载对应应用，Melody 不会同时挂载完整的 PCL 应用、主页播放器和后台监听；现代窗口按需复用设置、下载、实例内容页。

主题源码按职责分层：`components/` 放基础控件和窗口宿主，`pages/windows/` 放各个独立窗口，`pages/` 放主题入口，`composables/` 放 Pinia/窗口组合逻辑，`services/` 放后端调用、背景和窗口上下文，`styles/` 放主题样式，`config/` 放 Vue 与 Rust 共用的窗口配置，`assets/` 放主题资源。新增窗口或控件应放入对应目录，根目录只保留类型、公共导出和说明文件。

普通主题界面只显示启动器窗口。“设置”打开现代启动器设置，“更新游戏”打开现代游戏下载，底部“客户端管理”打开客户端列表。右键空白处或按 **Alt+T** 打开窗口菜单，可以返回 NeoUI。浏览器菜单还可以单独查看下载、首次设置和组件示例；桌面端不显示组件演示入口。开发说明不出现在启动器窗口中。

## 组件与样式

从 `index.ts` 导入，容器添加 `melody-theme-root` 并载入 `styles/melody-theme.scss`。原版基础组件不依赖 PCL、Quasar 的视觉控件。现代窗口另外载入 `styles/modern.scss`，在 `components/MelodyModernWindow.vue` 中复用 PCL 业务页；共享控件样式仅在 Melody 文档内启用，以本主题的字体、透明外框、渐变按钮和颜色覆盖。PCL 的主题首选项和整套应用不会被复制到 Melody。

| 组件 | 用途 |
| --- | --- |
| MelodyButton | 普通、主要、透明、链接按钮及禁用状态 |
| MelodyInput | 文本、密码、数值文本输入，支持原生标签与属性 |
| MelodySelect | 原生键盘选择行为，使用原始下拉箭头资源 |
| MelodyChoice | 复选框、单选项的外观与状态 |
| MelodyRadioGroup | 同一字符串模型驱动整组互斥选择 |
| MelodyGroupBox | legend 压线的透明外框，不单独填充面板颜色 |
| MelodyFormRow | 标签、输入与单位的 Flex 组合 |
| MelodyProgress | 渐变进度槽，数值限于 0–100，含可访问属性 |
| MelodyTable | 表头、行选择、键盘操作、空列表 |
| MelodyWindow | 窗口边框、标题区域、关闭、拖动、键盘焦点循环 |
| MelodyDialog | 公共提示类对话框：连续渐变标题/正文、独立底部按钮栏，默认插槽与 actions 插槽 |
| MelodyMessageWindow | 使用 MelodyDialog 的文字提示，默认“确认”按钮，支持随文字加宽 |
| MelodyModernWindow | 现代内容的统一独立窗框与内容容器 |
| MelodyTaskPanel | 后端进行中的任务与取消操作 |
| MelodyWindowHost | 为每个窗口提供独立的位置、层级、生命周期上下文 |

`--melody-*` 变量定义主题字体、玻璃透明度、边框、输入与按钮渐变。主页和提示类弹窗采用微软雅黑，旧式表单和列表使用宋体。窗口内采用 Grid/Flex；绝对坐标仅用于桌面上的窗口位置、背景、关闭按钮和主页版权文字。

提示框以 `08-23-54`、`08-24-10`、`08-24-23`、`08-24-36` 的原始截图为依据：较矮的白字阴影标题，蓝色透明渐变连续过渡到近白正文，底部独立 50px 操作栏，100×34 灰色渐变确认按钮。调用方只提供内容、标题和操作，不再单独绘制这些区域。

```vue
<MelodyDialog title="设置" @close="closeWindow">
  <p>对话框内容</p>
  <template #actions>
    <MelodyButton variant="dialog" @click="closeWindow">确认</MelodyButton>
  </template>
</MelodyDialog>
```

## 窗口模型

`windows.json` 是 Vue 和 Rust 共用的尺寸注册表：主窗口 720×450、设置 1030×380、版本管理 1060×655、提示最小 460×200、下载 430×205、首次设置 580×250。提示框根据渲染文字加宽，最大 900px，超长文本在正文区域换行滚动，底部操作栏始终可见；浏览器和原生宿主使用相同宽度，重复打开原生提示时同步调整窗口大小。这些是 CSS/DIP 尺寸，不是截图的物理像素尺寸；Windows 缩放可能改变实际截图像素数。

- 浏览器：多个窗口同时存在；重复打开会置前并保留未保存表单；拖动单个窗口不移动其他窗口。提示框只禁用其所属窗口，关闭后恢复原焦点。
- Tauri：`src-tauri/src/melody.rs` 为设置、版本管理、提示等创建各自的透明、无系统边框 WebviewWindow；普通子窗口属于主窗口，提示窗口属于发起它的窗口。普通窗口按类型复用，客户端编辑按实例 ID 复用，带目标的下载窗口按实例 ID 与组件/资源类型复用；不同客户端不共用编辑器。提示关闭恢复父窗口的 enabled 状态与焦点；主窗口关闭会关闭所有 Melody 子窗口。
- 主窗口进入主题时使用 720×450，退出主题时恢复之前的窗口尺寸和可调整状态。外观选择保存在 NeoUI 的现有首选项中。
- `?theme=melody&melodyWindow=settings` 等入口用于单独检查窗口。原生窗口通过相同入口初始化。

## 现代窗口与主题设置

- 启动器设置：独立 1030×655 窗口，分组侧栏涵盖主题、账户、语言、启动、内存、Java、游戏、高级启动、联机、下载、路径、缓存、控制台、其他设置、高级设置、关于、更新、反馈和日志。业务控件复用 PclSetupPage，外观配置使用 Melody 自己的面板。
- 下载游戏：独立 1060×655 窗口，复用 Minecraft/加载器安装、整合包导入和资源目录，可指定资源安装的客户端。任务进入 NeoUI 任务系统，进度和取消使用 MelodyTaskPanel；安装完成广播实例列表变更。
- 客户端管理：900×600 列表窗口，支持搜索、设为当前游戏和打开独立编辑器。编辑器 1060×655，适配自 PCL 独立实例控制器，复用 PclInstancePage 内容；连接备注、重命名、目录、图标、设置、组件、日志、启动、导出与删除确认。未实现后端的旧收藏按钮不在此显示。
- 主窗口处理 NeoUI 任务交互，遇到确认请求不再自动取消启动任务。资源下载入口注入 Melody 窗口宿主，不跳转回 PCL 独立窗口。
- 浏览器中的普通窗口彼此独立关闭；只将提示框绑定到发起窗口。原生窗体仍由主窗口拥有。

主题使用标准选择框，可切换 NeoUI、PCL Style 和忘却的旋律；原生子窗口切换时会恢复主窗口并关闭 Melody 子窗口。

主题设置自动保存：

- `luna-melody.appearance` 保存底部文字、字号（10–24px）、颜色、不透明度、图片适配、背景模式及目录。已有单图设置会自动迁移。
- 图片 Blob 保存到 IndexedDB `luna-melody` 的 `images` 存储，支持 PNG/JPEG/WebP/GIF/BMP（32 MB 以内），解码验证后写入，不把大图片塞入 localStorage。使用可撤销的 object URL 显示，支持恢复原版背景。
- 窗口间通过 storage 事件同步外观；同一页面内通过本地事件同步图片。下次启动从本地库重新加载。
- 底部文字默认为 `MineCraft Launcher 5.31 by 忘却的旋律`，可切换为 `Luna Launcher Neo UI` 或输入自定义文字。预设和自定义文字均可调整字号、颜色、不透明度，并实时预览。
- 背景模式为原版、指定图片、目录随机。目录随机在主窗口启动时选择一张，也可点击“换一张”；多张可用图片时优先避开上一张。设置预览与主窗口同步同一选择，单独打开设置不会触发启动随机。
- 桌面版保存真实目录路径，通过 `list_melody_backgrounds` 在启动/换图时重新扫描目录本层，`read_melody_background` 以二进制返回当前图片，不依赖广泛文件访问权限。新增、移除图片在下一次扫描生效；空目录、损坏或不可读图片回退到原版。
- 浏览器使用目录文件选择导入图片到 IndexedDB，导入总大小上限 128 MB。浏览器中的目录是导入快照，文件夹后续变更需要重新选择目录；桌面版直接读取目录。

## 原版窗体

原版设置、版本管理、首次设置与小下载框保留在右键/Alt+T 菜单，用于原貌展示。历史云整合列表、旧用户名密码登录等不代表现代业务能力；日常操作使用新增窗口。密码不会写入本地存储。首次设置窗口不替代 NeoUI 完整的首次启动向导。

## 资源来源

`assets/minecraft-water.jpg`、`sky.jpg`、下拉 sprite 等来自此前的 EXE 静态提取，与 prototype 中的原文件一致。出处和偏移见 `../../prototypes/melody/assets/manifest.json` 与资源提取报告。源程序与图像的权利归原作者；组件代码的许可不改变这些资源的权利。

天空图用于预览桌面，不能当成已确认的第二张主启动背景。外部 `bgskin/*.jpg` 原图缺失时不生成替代图；可通过 `registerMelodyBackground(url)` 注册获得的图片，保存“随机皮肤”时从已注册集合选择。

## 验证

- `pnpm typecheck`
- `pnpm build`
- `cargo check --manifest-path src-tauri/Cargo.toml --offline`
- `cargo test --manifest-path src-tauri/Cargo.toml --offline melody::tests`
- 对构建产物启动 HTTP 服务后运行 `node tests/melody-smoke.cjs` 与 `node tests/melody-modern.cjs`。可配置 `MELODY_URL`、`PLAYWRIGHT_MODULE` 与 `PLAYWRIGHT_CHROMIUM_EXECUTABLE`。

本次环境中 esbuild.exe 写临时配置报 Access denied；类型检查正常。使用 `temp/melody-build-preload.cjs` 在当前 Node 进程中让 esbuild 返回字节，再由 Node 写入，完成同一 Quasar/Vite 生产构建；未修改依赖文件或系统 ACL。临时目录同样出现了 esbuild 写失败，不能仅凭目录 ACL 断定具体原因。

浏览器测试覆盖七种窗口、同时存在、拖动、单选组、提示框焦点恢复、保存重开、控件越界与运行时错误，并直接查看生成截图。现代窗口测试使用模拟原生桥验证设置分类与写入、客户端 ID、备注、重命名、安装请求及资源入口；图片保存、恢复和跨页面同步使用真实浏览器存储。Rust 编译、注册表和窗口标识隔离测试通过；尚未实际启动重编译后的 Tauri 应用验证 Windows 桌面透视、DPI 与原生窗口焦点效果，因此不宣称已达到原程序的逐像素一致。
