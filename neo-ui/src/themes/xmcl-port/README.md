# XMCL Port 主题

这是 NeoUI 的 XMCL 风格主壳层。源码按职责分层：

- `components/`：壳层、侧栏、系统栏、弹窗和任务中心；
- `pages/`：主页、实例、资源、账户、多人游戏、商店和设置页面；
- `assets/`：XMCL 风格图片与资源卡片素材；
- `NOTICE.md`：上游 XMCL 相关版权与许可说明。

`App.vue` 负责组合这些组件，业务状态和后端调用仍由 NeoUI 的公共层提供。
