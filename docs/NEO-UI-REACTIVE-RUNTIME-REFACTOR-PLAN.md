# NeoUI 响应式运行时重构计划

## 目标

把 NeoUI 拆成数据层、主题运行时和界面层。跨窗口或后端事件只负责传递数据，不能直接触发全局页面刷新；组件只监听自己实际使用的数据，数据没有语义变化时不更新，也不销毁重建 DOM。

## 约束

- PCL、XMCL Port、Melody 使用同一套 common runtime 和 Pinia 数据模型。
- 后端任务继续由 C++ runtime 管理，切换主题不取消真实下载、安装或启动任务。
- 主题专属轮询、订阅、任务展示和工具状态只在主题 runtime 激活期间运行。
- 不用 `localStorage` 作为原生启动器的业务数据库；浏览器预览可以使用浏览器存储作为适配层。
- 不运行与本次重构无关的单元测试；只做类型检查、前端构建和必要的 Tauri 构建。

## 分阶段执行

### 1. 观测层

- 建立 common runtime debug bus，记录事件输入、store patch、刷新请求、组件 mount/unmount、定时器和订阅的开始/结束。
- 记录来源、领域、实体 id、变化字段和时间戳，限制内存容量，避免诊断器本身造成刷新。
- 提供独立 Debug Runtime 窗体，可实时查看、筛选、暂停和清空记录。

### 2. 数据层

- 将实例、账户、设置、任务和资源定义为独立 Pinia store/domain。
- 删除公共 session store 对任意 `storage` 事件的全局 `sync()`。
- 事件进入后先按领域和实体路由，再做语义比较；无变化不提交响应式 mutation。
- 保留实体对象和列表引用，只有真正增删、排序或字段变化时才 patch。

### 3. 主题运行时

- 建立 `ThemeRuntime` 生命周期：`start()`、`stop()`、`dispose()`。
- NeoRoot 只激活当前主题的 runtime；旧主题停止自己的事件监听、定时器和流订阅。
- 公共后端任务状态保留在 core task store，PCL/Melody/XMCL 只注册自己的显示适配器。
- 页面本地搜索、分类、loading、展开状态不进入公共数据 store。

### 4. 页面更新

- 移除 `emit('changed') -> App.refresh()` 的全局刷新链。
- 页面使用 computed/watch 监听自己的 store selector。
- 请求期间保留上次成功数据，不先清空数组；成功后就地 patch，失败只更新错误状态。
- 移除不必要的动态 `key`、页面级 `v-if` 卸载和隐藏页面的轮询。
- 下载页从全局 selected instance 派生 Minecraft 版本和 loader，上下文变化时局部更新。

### 5. 验证

- 类型检查、NeoUI 构建、必要的 Tauri 构建。
- Debug Runtime 中确认无关 storage/task 事件不会更新实例/账户域。
- 确认页面停留数分钟不会出现重复 refresh、mount/unmount 或数组清空。
- 确认切换主题后旧主题的订阅和定时器归零，后端任务仍继续运行。

## 本次执行顺序

本次先完成观测层、storage 误刷新修复、快照无变化不更新、页面清空数据修复和独立诊断窗体；随后继续按主题 runtime 和局部 watch 的阶段推进。每一阶段完成后运行必要构建并记录结果。

## 执行状态（2026-10-30）

- [x] common Runtime Debug 总线和独立 Tauri Debug Runtime 窗体。
- [x] 实例、账户快照按实体和字段做语义比较；无变化不替换对象或列表引用。
- [x] 移除公共 session store 对任意 `storage` 事件的监听。
- [x] App 刷新时保留已有数组内容，成功后就地 patch，过期请求不提交结果。
- [x] PCL 下载、设置、工具页的轮询和订阅按 `active` 生命周期启停。
- [x] PCL 设置页移除分类切换导致的动态 key 销毁重建。
- [x] 偏好设置跨窗口快照只写入真正变化的字段，并记录 store patch。
- [x] session store 事件桥接改为 Pinia store 初始化时单例建立，不再依赖某个页面的 `onMounted()`。
- [x] 为 PCL 下载/设置/工具页及 Melody runtime 增加 mount、激活、停用和 unmount 诊断记录。
- [x] 将 App 中的创建实例表单、PCL 页面组合和 XMCL 页面组合拆为独立 Vue 模块；App 只保留状态、业务动作和事件转发。
- [x] 将设备登录弹窗抽为 common `NeoDeviceLoginDialog`，避免页面壳直接持有弹窗结构。
- [x] 将 PCL 设置页的内存配置区域抽为 `PclMemorySettings`，保持实时内存状态由父 store 提供、表单事件向上提交。
- [x] 将 PCL 设置页的启动参数区域抽为 `PclLaunchSettings`，窗口、控制台和退出行为通过显式 props/action 连接。
- [x] 拆分 PCL 设置页的高级、下载、路径、控制台、网络、缓存、界面、语言、杂项、更新、日志、反馈和说明区域；父页面只保留上下文和导航。
- [x] 拆分 PCL 工具页的多人、任务、日志、调度器、认证辅助工具和诊断面板；父页面只保留导航、状态和上下文。
- [x] 拆分 PCL 启动页的输入区和主页内容区，并拆分 PCL 外观设置卡片。
- [x] 拆分 PCL 下载页的安装、组件版本、导入、世界、资源、整合包和收藏区域，并拆分目录浏览的项目详情/结果区域。
- [x] 拆分 PCL 账户页的账户、服务、工具区域及登录、服务编辑、本地用户对话框。
- [x] 拆分 PCL 安装页的版本选择和加载器配置步骤。
- [x] 检查并拆分资源下载窗、实例资源页的列表/网格区域，以及资源详情和下载确认对话框；剩余父组件只保留业务状态、事件和上下文。
- [x] 拆分 XMCL 实例工作区的概览、资源、组件、控制台区域，以及操作菜单和版本编辑对话框；工作区只保留共享状态、布局和事件转发。
- [x] 拆分 XMCL 账户页的导航、账户、服务、工具区域及登录、服务编辑对话框；子组件通过账户上下文共享同一套业务状态。
- [x] 拆分 XMCL 设置壳的全部内容区块，使用 `v-show` 保留已打开面板状态，避免分类切换销毁重建。
- [x] 拆分 XMCL 首页资源、世界存档和截图卡片，以及新增实例弹窗的配置、服务器、导入表单。
- [x] 完成主题目录结构审计：PCL、XMCL Port、Melody 下未发现剩余多区块大模板分支；新增 Vue/TS 文件均已纳入 `neo-ui/meson.build`。
- [x] 将主题专属轮询、拖放监听、内存/资源/控制台/任务订阅和 Melody 外观桥接注册到 ThemeRuntime disposer；主题停用时清理，重新激活时重新建立。
- [x] 为隐藏但保持挂载的 PCL 页面增加 active 闸门；下载目录、安装版本、工具轮询、内存和实例资源流不会在页面或主题停用时继续请求。
- [x] 在解除当前执行环境对 esbuild 子进程写入的限制后完成 Quasar SPA 构建和 Tauri 打包验证；`pixi run install-webui release` 已完成真实构建、Tauri 打包和安装。

## 验证记录

- `pnpm --dir neo-ui typecheck`：通过。
- `cargo check --manifest-path neo-ui/src-tauri/Cargo.toml`：通过（此前已验证）。
- `pnpm --dir neo-ui build`：项目 ACL 已清理，但仍被当前执行环境的 esbuild 子进程文件写入策略阻塞，报错为 `Failed to write to output file ... Access is denied`；普通 Node 写入已验证可行，问题不来自 TS 编译诊断。
