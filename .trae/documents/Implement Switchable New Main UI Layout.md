我将根据您的要求实现一套可切换的新主界面 UI。

### 1. 注册新设置
在 `launcher/Application.cpp` 中：
- 注册一个新的布尔设置 `UseNewUI`（默认为 `false`），用于控制界面布局的切换。

### 2. 在 MainWindow 中实现新 UI 逻辑
在 `launcher/ui/MainWindow.h` 和 `launcher/ui/MainWindow.cpp` 中修改初始化逻辑：
- **构造函数**: 在初始化时读取 `UseNewUI` 设置。
- **旧布局模式（默认）**: 保持原有逻辑，将 `InstanceView` 添加到 `ui->horizontalLayout`，并显示默认工具栏。
- **新布局模式**:
    - 创建 `setupNewLayout()` 方法来构建新的界面结构。
    - **隐藏原有工具栏**: 隐藏顶部的 `mainToolBar` 和右侧的 `instanceToolBar`。按要求保留底部的 `newsToolBar` 和状态栏 (`statusBar`)。
    - **构建新中心布局**: 创建一个新的容器控件，采用三列布局 (`QHBoxLayout`)：
        1.  **左侧栏 (Left Sidebar)**:
            -   **用户头像/名称**: 复用 `actionAccountsButton`（账户按钮），显示当前用户头像和名称。
            -   **设置**: 添加 `actionSettings`（设置）按钮。
            -   **工具栏**: 将原顶部工具栏的功能（如 `文件夹`、`帮助` 等）作为按钮移至此处。
            -   添加弹簧 (Spacer) 将内容向上对齐。
        2.  **中间区域 (Center)**:
            -   放置核心的实例列表视图 (`InstanceView`)。
        3.  **右侧栏 (Right Sidebar)**:
            -   **实例管理**: 将原实例工具栏的功能（`启动`、`编辑`、`删除`、`导出` 等）作为垂直排列的按钮放置在此处。
    - **应用布局**: 将这个新的容器控件设置为主窗口的中心控件 (`CentralWidget`)。

### 3. 界面切换机制
- 用户可以通过修改设置（`PrismLauncher.ini` 中的 `UseNewUI`）来切换界面。
- 切换后需要重启应用以生效（这是涉及主窗口结构变更的标准做法）。

此方案最大程度地复用了现有的业务逻辑（Action 系统），确保新旧界面在功能上完全一致，仅在视觉布局上有所区别。