# PCL Style 账户管理审阅报告

> 范围：`neo-ui/src/themes/pcl/pages/accounts/`、共享账户 store，以及账户 API/异步任务运行时。
>
> 结论类型：本报告先记录审阅结果，随后记录已落地的修复。优先级使用 P1（应优先修复）和 P2（应安排修复/清理）。

## 结论摘要

当前账户 API 的账户类型和引用契约是对齐的：后端返回小写 `microsoft`、`offline`、`yggdrasil`、`unified-pass`，并返回 `internalId`、`id`、`profileName`；PCL 页面使用的类型字符串和引用回退顺序与此匹配。当前 `pnpm typecheck` 通过。

审阅时确认了四个高影响问题：`busy` 注入后失去响应性；皮肤/披风操作会丢失完整账户对象；账户详情请求存在旧响应覆盖新选择的竞态；刷新失败写入只在登录弹窗显示的错误字段。它们可以解释“操作按钮状态不对、配置突然消失、详情串号、刷新失败无提示”等现象。

## 修复记录（2026-10-31）

- 账户页合并为单一 `provide`，`accounts` 和 `busy` 通过 `toRef` 注入；登录取消、账户详情、刷新任务均增加代次/终态保护。
- 皮肤、披风操作完成后按引用从当前账户列表解析完整对象，保留 Yggdrasil/UnifiedPass 配置；账户实体字段变化时会重新加载详情。
- 刷新失败写入账户页可见的 `accountError`，服务配置和 Authlib 操作也会反馈错误；刷新、登录期间相关账户按钮统一禁用。
- 同类修复已覆盖 PCL/XMCL provider 的 `busy` 响应性快照，以及目录详情、下载、资源安装、服务启动、Java 安装和任务中心对终态任务的判断。
- 仍可继续安排的改进：服务页单独的 loading 状态、按账户显示刷新失败明细，以及统一账户写操作的互斥状态。

## 已确认问题

### P1：账户页的 `busy` 是挂载时快照，操作期间不会更新

- **位置**：[PclAccountPage.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountPage.vue:334-336)、[PclAccountProfileSection.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountProfileSection.vue:5-8)、[PclAccountPanel.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountPanel.vue:24-29)。
- **原因**：`provide` 写的是 `busy: props.busy`，普通布尔值被复制进 context；后续 `App.busy` 改变时，注入的 `busy` 不会改变。`PclAccountPage` 的第二次 `provide` 还覆盖了第一次 context。
- **触发**：账户页已打开后执行登录、保存认证配置、移动/删除账户或其他长操作；观察账户行和配置按钮。
- **影响**：按钮不会按全局忙碌状态禁用，用户可以在请求未完成时重复提交或修改同一账户。`refreshAll` 使用独立的 `refreshing`，账户面板也没有接收该状态，因此刷新期间仍可删除、排序或改配置。
- **建议**：通过 `toRef(props, 'busy')` 或 `computed` 注入响应式引用；合并两个 `provide`；把登录、全部刷新和账户写操作纳入同一账户级互斥状态。

### P1：皮肤/披风操作后用临时 `{ id }` 重新选择，丢失账户认证配置

- **位置**：[PclAccountPanel.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountPanel.vue:227-231)、[PclAccountPanel.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountPanel.vue:184-201)。
- **原因**：`installSkin`、`uploadSkin`、`resetSkin`、`deleteSkin`、`chooseCape` 完成后调用 `select({ id: selected.value })`。`select` 会把这个临时对象写入 `selectedAccount`，并从它重新构造 `config`；对象没有 `type`、`yggdrasil` 或 `unifiedPass` 字段。
- **触发**：选中 Yggdrasil 或 UnifiedPass 账户，执行任一皮肤/披风操作。
- **影响**：认证配置卡片中的类型专属字段消失或被清空；随后保存可能进入错误分支，UnifiedPass 还可能把空的 `serverId/baseUrl` 发给后端。
- **建议**：操作后按引用从 `props.accounts` 找回完整账户对象，或让 `select` 接受引用并重新绑定当前列表对象；不要用不完整对象替换选择状态。

### P1：账户详情请求没有请求代次，旧响应可覆盖当前账户

- **位置**：[PclAccountPanel.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountPanel.vue:184-201)。
- **原因**：`select` 先更新 `selectedAccount`，再异步请求 `account.profile` 和 `account.skin.list`；没有在响应返回时确认 `selected.value` 仍是同一个引用，也没有在请求开始时清空旧 `profile/skins`。
- **触发**：快速连续点击两个账户，或切换账户后其中一个 API 请求失败/延迟。
- **影响**：旧账户的皮肤、披风和状态可能显示在新账户下；请求失败时旧详情仍会留在界面，用户容易对错误账户执行操作。
- **建议**：为每次选择生成 generation/token，响应提交前比对；切换时先清空详情或显示加载状态；失败时不要保留前一个账户的详情。

### P1：全部刷新失败被写进登录弹窗错误字段，通常对用户不可见

- **位置**：[PclAccountPage.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountPage.vue:289-310)。
- **原因**：`refreshAll` 的 catch 写入 `loginError.value`。该错误只在 [PclAccountLoginDialog.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountLoginDialog.vue:85) 渲染；从账户列表或工具页刷新时登录弹窗通常没有打开。
- **触发**：存在过期账户、网络失败或其中一个 `account.refresh` 任务失败，然后点击“全部刷新”。
- **影响**：刷新失败后页面没有可见错误，用户只看到数据没有更新；再次打开登录弹窗才可能看到旧错误。
- **建议**：使用账户页专用 `accountError/refreshError`，在触发刷新按钮所在区域显示，并保留每个失败账户及任务错误。

## 需要安排的次要问题

### P2：重复 `provide` 覆盖 context，且第二份丢失 `openSection`

- **位置**：[PclAccountPage.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountPage.vue:334-336)。
- Vue 对同一组件同一 key 的后一次 `provide` 取值；第二份没有 `openSection`。目前导航按钮在父模板中直接调用，子 section 暂未调用该字段，因此暂未观察到直接崩溃，但 context 契约已经不一致，后续子组件使用会得到 `undefined`。
- 建议保留一个完整 context，并用明确类型替代 `Record<string, any>`。

### P2：账户列表字段更新时，已选账户的 profile 不会自动重新读取

- **位置**：[PclAccountPanel.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountPanel.vue:204-217)、[launcher-session-store.ts](../neo-ui/src/common/runtime/launcher-session-store.ts:100-120)。
- store 会原地 patch 同一账户对象；PCL watcher 只观察“选中 ID + ID 列表”，同一个 ID 的 `state/profile/cache` 改变不会触发 `select`。账户行会更新，但详情里的 `account.profile`/皮肤库可能保留旧快照。
- 建议在账户实体版本或关键字段变化时刷新详情，并给 profile 请求加缓存/去重策略。

### P2：服务页加载失败被吞掉，且没有加载状态

- **位置**：[PclAccountPage.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountPage.vue:322-331)、[PclAccountServicesSection.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountServicesSection.vue:23-36)。
- 切换到 services 才开始加载；各加载函数把异常转换为空数组或默认值，页面没有 loading/error 提示。网络断开时可能显示“无服务”或“认证组件未知”，容易被误判为数据为空。
- 建议增加 servicesLoading/servicesError，并在首次进入或重试时显示状态。

### P2：服务配置写操作缺少统一错误反馈

- **位置**：[PclAccountPage.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountPage.vue:286-288)、[PclAccountServicesSection.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountServicesSection.vue:16-20)。
- `savePreset`、`removePreset`、`installAuthlib`、`setLocalSetting` 等入口没有统一的 `try/catch` 和页面错误状态；`catalogExecute` 抛错后可能只留下未处理 Promise，弹窗也不会说明失败原因。
- 建议复用账户页的操作包装器，并让错误显示在当前卡片或 toast。

### P2：取消登录存在轮询竞态

- **位置**：[PclAccountPage.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountPage.vue:198-205、223-264)。
- `cancelLogin` 关闭 UI、清空 task ID 后，`submitLogin` 的轮询仍可能在等待；取消与成功响应同时到达时，旧轮询可能更新已关闭的状态，极端情况下仍触发 `changed`。
- 当前后端 `neo.task.cancel` 会把任务标为 `finished=true/state=cancelled`，所以通常会落入失败分支；仍建议使用 generation/token，在取消后让旧轮询立即失效，并忽略“已取消”错误提示。

### P2：刷新轮询只看 `finished`，兼容旧任务快照时可能不退出

- **位置**：[PclAccountPage.vue](../neo-ui/src/themes/pcl/pages/accounts/PclAccountPage.vue:299-302)。
- 当前 `AsyncTaskRuntime::finish` 会同时设置 `finished=true`，所以现行后端路径不会复现；但 `refreshAll` 没有像登录轮询那样把 `succeeded/failed/cancelled/aborted` 视为终态。若连接到旧后端或代理只返回终态字符串，可能持续轮询。
- 建议复用统一 `isTaskFinished`，并增加超时与任务丢失处理。

## 已核对、暂不列为问题

- **账户类型**：后端 `LauncherApiModel::AccountObject::snapshot` 返回 `typeString()`；当前实现的枚举映射是小写 `microsoft/offline/yggdrasil/unified-pass`，与 PCL 判断一致。
- **账户引用**：后端为每个 `MinecraftAccount` 生成 UUID `internalId`，API 查找同时支持 `profileId/internalId/profileName`；PCL 的回退顺序在正常账户数据下不会碰撞。
- **刷新任务终态**：`AsyncTaskRuntime::finish` 明确写入 `finished=true`；因此“当前后端刷新必然无限轮询”不能作为已确认 bug，只保留兼容性风险。
- **登录中的额外交互**：账户登录产生的 profile 选择、无 Java profile 时的名称输入，会被全局 `TaskInteractionDialog` 通过 `neo.task.list/neo.task.respond` 接管；PCL 登录页本身没有处理它，但当前不会因此必然卡死。
- **空账户入口**：无账户时 PCL 提供 Microsoft 和符合条件的离线入口，Yggdrasil/UnifiedPass 从 services 页添加；这是当前页面设计路径，不算缺失功能。

## 验证记录

- `pnpm typecheck`（目录 `neo-ui`）：通过。
- `git diff --check`：通过；Git 对工作区既有文件给出的 LF/CRLF 提示不属于空白错误。

