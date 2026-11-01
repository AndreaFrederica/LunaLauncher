# Endfield 主题

终末地风味的 PCL 组件皮肤。复用 `src/themes/pcl` 的页面、业务逻辑和控件，只替换视觉层，不复制启动、下载、实例管理、设置或账户逻辑。

## 变体卡

- 强调色：`#FFFA00`，只用于当前位置、可点击内容和醒目提醒。
- 纸色：浅色 `#F4F4F2`，深色 `#191919`；板 `#FFFFFF` / `#222222`。
- 切角：`8px`，只给按钮、导航项等可点击元素；卡片和正文保持直角。
- 背景：右上、左下各一片开放式等高线，墨色 9%。
- 密度：紧凑；动效 120–220ms，`mech` / `snap` 曲线，不做循环装饰动画。

## 结构

- `styles/endfield.scss`：Endfield token、PCL token 重映射和组件覆盖。
- `assets/fonts/Saira.ttf`：Saira 可变字体，SIL OFL 1.1，许可证见同目录 `Saira-OFL.txt`。
- `assets/contours/`：由 `endfield-ui` skill 附带的等高线素材，作为低对比背景。

## 使用方式

`neo-ui-preferences.ts` 的 `shell` 设为 `endfield`。根元素会同时得到：

```html
<html data-neo-shell="pcl" data-neo-skin="endfield">
```

`data-neo-shell="pcl"` 让现有 PCL 页面和控件继续生效；`data-neo-skin="endfield"` 让本目录的 token 和样式覆盖它们。这样新增主题不会复制业务代码，PCL 页面修复也会自动进入 Endfield。

## 约束

- 不使用游戏官方字体、标志、图标、立绘、截图或文案。
- 红色只用于删除；告警使用黄色块和墨线。
- 浅色主题里黄色只做填充，不作文字或 1px 线。
- 所有编号来自真实列表/计数；不添加假技术文字。
