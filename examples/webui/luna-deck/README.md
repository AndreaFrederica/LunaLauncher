# Luna Deck

浅色、深绿侧栏和橙色强调的独立静态测试 UI。没有构建依赖，复制整个目录到宿主
旁边的 `webui/luna-deck`，在 Web UI 管理器选择 **Luna Deck · 浅色测试界面**。

它使用真实实例列表、图标、实例信息和系统目录打开 API。左侧“界面加载设置”直接
通过 `luna.webUiSettings()` / `luna.updateWebUiSettings()` 编辑宿主设置，也可以在
页面内切换到其他 UI。实例编辑、账户、下载等完整功能通过“打开完整 Neo UI”进入。

需要包含加载设置 API 的新版宿主。普通浏览器可预览布局，但无法连接真实数据。
源码及自绘图形遵循 GPL-3.0-only；详细协议见 [Web UI 开发文档](../../../docs/WEBUI-DEVELOPMENT.md)。
