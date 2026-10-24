<template>
  <section class="pcl-page pcl-setup-page" :class="{ 'is-embedded': embedded }">
    <PclPageNav v-if="!embedded" v-model="section" :groups="groups" @refresh="refreshSection" />

    <main class="pcl-page-main">
      <Transition name="pcl-page" mode="out-in">
        <div :key="section" class="pcl-page-view pcl-setup-stack">
          <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>
          <PclHint v-if="notice" theme="blue" closable @close="notice = ''">{{ notice }}</PclHint>

          <template v-if="section === 'accounts'">
            <PclAccountPage :accounts="accounts || []" :busy="busy" :execute="execute" embedded @changed="emit('changed')" />
          </template>

          <template v-else-if="section === 'launch'">
            <PclCard title="启动选项">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">默认实例隔离</span>
                <div class="pcl-form-row__control">
                  <PclSelect :model-value="'isolated'" :options="[{ label: '隔离可安装模组的实例', value: 'isolated' }]" disabled />
                </div>
                <span class="pcl-form-row__hint">Luna 实例默认使用独立目录</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">游戏窗口标题</span>
                <div class="pcl-form-row__control">
                  <PclInput :model-value="String(val('WindowTitle', '默认（实例名）'))" disabled />
                </div>
                <span class="pcl-form-row__hint">实例级设置</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">启动器可见性</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="bool('CloseAfterLaunch') ? 'close' : 'keep'"
                    :options="[{ label: '保持启动器可见', value: 'keep' }, { label: '启动后关闭启动器', value: 'close' }]"
                    @update:model-value="save('CloseAfterLaunch', $event === 'close')"
                  />
                </div>
                <span class="pcl-form-row__hint">启动成功后</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">游戏结束之后</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="bool('QuitAfterGameStop') ? 'quit' : 'keep'"
                    :options="[{ label: '保持启动器打开', value: 'keep' }, { label: '退出启动器', value: 'quit' }]"
                    @update:model-value="save('QuitAfterGameStop', $event === 'quit')"
                  />
                </div>
                <span class="pcl-form-row__hint">客户端退出后</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">窗口大小</span>
                <div class="pcl-form-row__control">
                  <PclSelect v-model="windowMode" :options="windowOptions" style="width: 150px" />
                  <template v-if="windowMode === 'custom'">
                    <PclInput
                      type="number"
                      :model-value="num('MinecraftWinWidth', 854)"
                      style="width: 70px"
                      @update:model-value="drafts.MinecraftWinWidth = $event"
                      @blur="save('MinecraftWinWidth', Number(drafts.MinecraftWinWidth || 854))"
                    />
                    <span class="pcl-form-row__hint">×</span>
                    <PclInput
                      type="number"
                      :model-value="num('MinecraftWinHeight', 480)"
                      style="width: 70px"
                      @update:model-value="drafts.MinecraftWinHeight = $event"
                      @blur="save('MinecraftWinHeight', Number(drafts.MinecraftWinHeight || 480))"
                    />
                  </template>
                </div>
                <span class="pcl-form-row__hint">{{ windowMode === 'maximized' ? '启动后最大化' : '像素' }}</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">控制台显示</span>
                <div class="pcl-form-row__control pcl-check-row">
                  <PclCheckbox :model-value="bool('ShowConsole')" label="显示控制台" @update:model-value="save('ShowConsole', $event)" />
                  <PclCheckbox :model-value="bool('AutoCloseConsole')" label="正常退出后关闭" @update:model-value="save('AutoCloseConsole', $event)" />
                  <PclCheckbox :model-value="bool('ShowConsoleOnError')" label="出错时显示" @update:model-value="save('ShowConsoleOnError', $event)" />
                </div>
              </div>
            </PclCard>

          </template>

          <template v-else-if="section === 'memory'">
            <PclCard title="游戏内存">
              <PclHint v-if="maxMem < 2048" theme="yellow">最大内存低于 2 GiB，部分整合包可能无法启动。</PclHint>
              <div class="pcl-choice-row">
                <PclRadio :model-value="memoryMode" value="auto" label="自动配置" @update:model-value="onMemoryMode" />
                <PclRadio :model-value="memoryMode" value="custom" label="自定义" @update:model-value="onMemoryMode" />
                <span class="pcl-form-row__hint">自动配置会设为 512 MiB – 4 GiB</span>
              </div>

              <div class="pcl-memory-live-head">
                <span>已使用内存 / 已安装内存</span>
                <span class="pcl-memory-live-right">
                  游戏分配
                  <span class="pcl-memory-live-dot" />
                  每 2 秒
                </span>
              </div>
              <div class="pcl-memory-bar">
                <span class="pcl-memory-bar__used" :style="{ width: `${memoryUsedPercent}%` }" />
                <span class="pcl-memory-bar__allocated" :style="{ left: `${memoryUsedPercent}%`, width: `${memoryAllocAfterPercent}%` }" />
                <span class="pcl-memory-bar__divider" :style="{ left: `${memoryUsedPercent}%` }" />
              </div>
              <div class="pcl-memory-values">
                <strong>{{ memoryUsedGiB }} GiB <em>/</em> {{ memoryTotalGiB }} GiB</strong>
                <strong>{{ formatMib(maxMem) }}</strong>
              </div>
              <div class="pcl-memory-legend">
                <span><b>系统占用</b>{{ memoryUsedPercent.toFixed(1) }}%</span>
                <span><b>最小</b>{{ formatMib(draftNumber('MinMemAlloc', 512)) }}</span>
                <span><b>最大</b>{{ formatMib(draftNumber('MaxMemAlloc', 4096)) }}</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">最小内存</span>
                <div class="pcl-form-row__control">
                  <PclSlider
                    :model-value="draftNumber('MinMemAlloc', 512)"
                    :min="256"
                    :max="16384"
                    :step="256"
                    :disabled="memoryMode === 'auto'"
                    :format="formatMib"
                    @update:model-value="drafts.MinMemAlloc = $event"
                    @change="save('MinMemAlloc', $event)"
                  />
                </div>
                <span class="pcl-form-row__hint">MiB</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">最大内存</span>
                <div class="pcl-form-row__control">
                  <PclSlider
                    :model-value="draftNumber('MaxMemAlloc', 4096)"
                    :min="512"
                    :max="32768"
                    :step="512"
                    :disabled="memoryMode === 'auto'"
                    :format="formatMib"
                    @update:model-value="drafts.MaxMemAlloc = $event"
                    @change="save('MaxMemAlloc', $event)"
                  />
                </div>
                <span class="pcl-form-row__hint">MiB</span>
              </div>
            </PclCard>

          </template>

          <template v-else-if="section === 'advanced'">
            <PclCard title="高级启动选项" collapsible>
              <div class="pcl-form-row pcl-form-row--top">
                <span class="pcl-form-row__label">JVM 参数</span>
                <div class="pcl-form-row__control pcl-form-row__control--column">
                  <PclInput
                    multiline
                    :rows="4"
                    :model-value="draftValue('JvmArgs', '')"
                    placeholder="留空使用 Luna 的默认参数"
                    @update:model-value="drafts.JvmArgs = $event"
                    @blur="save('JvmArgs', drafts.JvmArgs)"
                  />
                </div>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">启动前命令</span>
                <div class="pcl-form-row__control">
                  <PclInput
                    :model-value="draftValue('PreLaunchCommand', '')"
                    placeholder="在启动游戏前执行"
                    @update:model-value="drafts.PreLaunchCommand = $event"
                    @blur="save('PreLaunchCommand', drafts.PreLaunchCommand)"
                  />
                </div>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">包装命令</span>
                <div class="pcl-form-row__control">
                  <PclInput
                    :model-value="draftValue('WrapperCommand', '')"
                    placeholder="例如自定义启动包装器"
                    @update:model-value="drafts.WrapperCommand = $event"
                    @blur="save('WrapperCommand', drafts.WrapperCommand)"
                  />
                </div>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">退出后命令</span>
                <div class="pcl-form-row__control">
                  <PclInput
                    :model-value="draftValue('PostExitCommand', '')"
                    placeholder="游戏退出后执行"
                    @update:model-value="drafts.PostExitCommand = $event"
                    @blur="save('PostExitCommand', drafts.PostExitCommand)"
                  />
                </div>
              </div>

              <div class="pcl-check-grid">
                <PclCheckbox :model-value="bool('UseNativeGLFW')" label="使用系统 GLFW" @update:model-value="save('UseNativeGLFW', $event)" />
                <PclCheckbox :model-value="bool('UseNativeOpenAL')" label="使用系统 OpenAL" @update:model-value="save('UseNativeOpenAL', $event)" />
                <PclCheckbox :model-value="bool('UseDiscreteGpu')" label="要求使用高性能显卡" @update:model-value="save('UseDiscreteGpu', $event)" />
                <PclCheckbox :model-value="bool('OnlineFixes')" label="旧版本在线服务修复" @update:model-value="save('OnlineFixes', $event)" />
                <PclCheckbox :model-value="bool('ShowGameTime')" label="显示游戏时间" @update:model-value="save('ShowGameTime', $event)" />
                <PclCheckbox :model-value="bool('RecordGameTime')" label="记录游戏时间" @update:model-value="save('RecordGameTime', $event)" />
              </div>

              <PclHint theme="blue" class="pcl-advanced-hint">更多实例专属的 JVM、窗口和启动参数可以在实例设置中找到。</PclHint>
            </PclCard>
          </template>

          <template v-else-if="section === 'game'">
            <PclCard title="游戏资源获取行为">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">文件下载源</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="num('DownloadMirrorType', 0)"
                    :options="mirrorOptions"
                    @update:model-value="save('DownloadMirrorType', $event)"
                  />
                </div>
                <span class="pcl-form-row__hint">Minecraft 资源与依赖</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">最大下载线程</span>
                <div class="pcl-form-row__control">
                  <PclSlider
                    :model-value="draftNumber('PclDownloadThreadLimit', 63)"
                    :min="1"
                    :max="255"
                    :step="1"
                    @update:model-value="drafts.PclDownloadThreadLimit = $event"
                    @change="save('PclDownloadThreadLimit', $event)"
                  />
                </div>
                <span class="pcl-form-row__hint">1 – 255</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">下载速度限制</span>
                <div class="pcl-form-row__control">
                  <PclSlider
                    :model-value="draftNumber('PclDownloadSpeedLimitKBps', 0)"
                    :min="0"
                    :max="102400"
                    :step="1024"
                    :format="formatSpeed"
                    @update:model-value="drafts.PclDownloadSpeedLimitKBps = $event"
                    @change="save('PclDownloadSpeedLimitKBps', $event)"
                  />
                </div>
                <span class="pcl-form-row__hint">0 表示不限速</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">安装行为</span>
                <div class="pcl-form-row__control pcl-check-row">
                  <PclCheckbox
                    :model-value="!bool('ModDependenciesDisabled')"
                    label="自动检查并安装必需前置"
                    @update:model-value="save('ModDependenciesDisabled', !$event)"
                  />
                  <PclCheckbox
                    :model-value="bool('GlobalDataPacksEnabled')"
                    label="启用全局数据包"
                    @update:model-value="save('GlobalDataPacksEnabled', $event)"
                  />
                </div>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">全局数据包目录</span>
                <div class="pcl-form-row__control">
                  <PclInput
                    :model-value="draftValue('GlobalDataPacksPath', '')"
                    placeholder="留空使用默认目录"
                    @update:model-value="drafts.GlobalDataPacksPath = $event"
                    @blur="save('GlobalDataPacksPath', drafts.GlobalDataPacksPath)"
                  />
                </div>
              </div>
            </PclCard>

            <PclCard title="稳定性">
              <div class="pcl-check-grid">
                <PclCheckbox :model-value="bool('ModMetadataDisabled')" label="跳过模组元数据查询" @update:model-value="save('ModMetadataDisabled', $event)" />
                <PclCheckbox :model-value="bool('SkipModpackUpdatePrompt')" label="跳过整合包更新提示" @update:model-value="save('SkipModpackUpdatePrompt', $event)" />
                <PclCheckbox :model-value="bool('ShowModIncompat')" label="显示模组不兼容提示" @update:model-value="save('ShowModIncompat', $event)" />
                <PclCheckbox :model-value="bool('FallbackMRBlockedMods')" label="Modrinth 受阻模组回退下载" @update:model-value="save('FallbackMRBlockedMods', $event)" />
              </div>
            </PclCard>
          </template>

          <template v-else-if="section === 'network'">
            <PclCard v-if="isInstance" title="服务端">
              <p class="pcl-muted">服务端登录限制、地址和自动加入设置位于实例工作区的“服务端”页。</p>
              <PclHint theme="blue">当前页面只显示实例级服务端设置的入口说明，避免把启动器代理写进实例设置。</PclHint>
            </PclCard>

            <template v-else>
              <PclCard title="EasyTier">
                <PclHint theme="yellow">修改此处的设置后，需要重新启动大厅以使设置生效。</PclHint>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">用户名</span>
                  <div class="pcl-form-row__control">
                    <PclInput model-value="" placeholder="等待后端接入" disabled />
                  </div>
                </div>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">协议偏好</span>
                  <div class="pcl-form-row__control">
                    <PclSelect model-value="tcp" :options="[{ label: 'TCP', value: 'tcp' }, { label: 'UDP', value: 'udp' }]" disabled />
                  </div>
                </div>
                <div class="pcl-check-grid">
                  <PclCheckbox :model-value="false" label="选择最低延迟路径" disabled />
                  <PclCheckbox :model-value="false" label="对对称型 NAT 进行端口猜测" disabled />
                  <PclCheckbox :model-value="false" label="允许使用 IPv6 通信" disabled />
                  <PclCheckbox :model-value="false" label="在日志中输出 Cli 信息以用于调试" disabled />
                </div>
                <PclHint theme="blue" class="pcl-advanced-hint">
                  EasyTier 联机需要 Luna 后端新增 Link* 设置，当前仅保留 PCL 布局；启动器代理已移至“杂项 → 网络”。
                </PclHint>
              </PclCard>
            </template>
          </template>

          <template v-else-if="section === 'java'">
            <PclJavaPanel :scope="scope" :settings="settings" :busy="busy" :execute="execute" @changed="emit('changed')" />
          </template>

          <template v-else-if="section === 'ui'">
            <PclAppearancePanel />
          </template>

          <template v-else-if="section === 'language'">
            <PclCard title="语言">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">界面语言</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="languageId"
                    :options="languageOptions"
                    placeholder="跟随系统"
                    @update:model-value="selectLanguage"
                  />
                </div>
                <PclButton icon="refresh-cw" :disabled="busy || languagesLoading" @click="loadLanguages(true)">刷新</PclButton>
              </div>
              <PclCheckbox :model-value="useSystemLocale" label="跟随系统语言" @update:model-value="setSystemLocale" />
              <PclLoading v-if="languagesLoading" text="正在读取语言列表" inline />
            </PclCard>

            <div class="pcl-language-banner">
              <PclIcon name="earth" :size="190" class="pcl-language-banner__earth" />
              <div class="pcl-language-banner__copy">
                <div class="pcl-language-banner__title">让 <b>Luna</b> 走向国际化。</div>
                <p>Luna 正在持续补充翻译。你可以帮助改进术语、校对文案，或为新的语言提交完整的翻译文件。</p>
                <p>翻译文件使用标准的 Qt 翻译格式，与启动器版本一起发布。</p>
                <p>提交前请先阅读贡献指南，并在本地确认界面没有截断或错位。</p>
                <div class="pcl-actions">
                  <PclButton variant="highlight" icon="upload" @click="openUrl('https://github.com/PrismLauncher/PrismLauncher/blob/develop/CONTRIBUTING.md')">提交 Pull Request</PclButton>
                  <PclButton icon="book-marked" @click="openUrl('https://github.com/PrismLauncher/PrismLauncher/wiki')">查看开发文档</PclButton>
                </div>
              </div>
            </div>
          </template>

          <template v-else-if="section === 'misc'">
            <PclCard title="系统">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">实时日志行数</span>
                <div class="pcl-form-row__control">
                  <PclSlider
                    :model-value="draftNumber('ConsoleMaxLines', 1000)"
                    :min="100"
                    :max="100000"
                    :step="1000"
                    @update:model-value="drafts.ConsoleMaxLines = $event"
                    @change="save('ConsoleMaxLines', $event)"
                  />
                </div>
                <span class="pcl-form-row__hint">控制台保留行数</span>
              </div>
              <div class="pcl-check-grid">
                <PclCheckbox :model-value="bool('ConsoleOverflowStop')" label="日志溢出时停止输出" @update:model-value="save('ConsoleOverflowStop', $event)" />
                <PclCheckbox :model-value="bool('LogPrePostOutput')" label="记录启动前后输出" @update:model-value="save('LogPrePostOutput', $event)" />
              </div>
              <div class="pcl-actions">
                <PclButton icon="folder-open" :disabled="busy" @click="openLogs">打开日志目录</PclButton>
                <PclButton icon="download" :disabled="true" title="Luna 暂未提供设置导出文件操作">导出设置</PclButton>
                <PclButton icon="upload" :disabled="true" title="Luna 暂未提供设置导入文件操作">导入设置</PclButton>
              </div>
            </PclCard>

            <PclCard v-if="!isInstance" title="网络">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">代理类型</span>
                <div class="pcl-form-row__control">
                  <PclSelect v-model="proxy.type" :options="proxyTypeOptions" />
                </div>
                <span class="pcl-form-row__hint">不使用 / 系统 / HTTP / SOCKS5</span>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">地址</span>
                <div class="pcl-form-row__control">
                  <PclInput v-model="proxy.host" placeholder="127.0.0.1" />
                </div>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">端口</span>
                <div class="pcl-form-row__control">
                  <PclInput v-model.number="proxy.port" type="number" placeholder="7890" style="width: 110px" />
                </div>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">账号 / 密码</span>
                <div class="pcl-form-row__control">
                  <PclInput v-model="proxy.username" placeholder="可选" />
                  <PclInput v-model="proxy.password" type="password" placeholder="可选" />
                </div>
              </div>
              <div class="pcl-actions">
                <PclButton variant="highlight" icon="save" :disabled="busy" @click="saveProxy">应用代理</PclButton>
                <PclButton icon="network" :disabled="busy" @click="testProxy">测试连接</PclButton>
              </div>
              <PclHint theme="yellow">请勿填写不信任的代理地址。代理设置会同时作用于启动器请求和游戏下载。</PclHint>
              <div v-if="proxyTestResult" class="pcl-diagnostic">{{ proxyTestResult }}</div>
            </PclCard>

            <PclCard title="调试选项" collapsible>
              <div class="pcl-check-grid">
                <PclCheckbox :model-value="bool('PclDownloadFallbackToQt')" label="PCL 下载失败时回退 Qt" @update:model-value="save('PclDownloadFallbackToQt', $event)" />
                <PclCheckbox :model-value="bool('Aria2FallbackToQt')" label="Aria2 失败时回退 Qt" @update:model-value="save('Aria2FallbackToQt', $event)" />
                <PclCheckbox :model-value="bool('DownloadsDirWatchRecursive')" label="递归监控下载目录" @update:model-value="save('DownloadsDirWatchRecursive', $event)" />
              </div>
            </PclCard>
          </template>

          <template v-else-if="section === 'about'">
            <PclCard title="关于">
              <div class="pcl-about-head">
                <div class="pcl-about-logo">L</div>
                <div>
                  <div class="pcl-about-name">Luna Launcher <span>Neo UI</span></div>
                  <div class="pcl-about-version">{{ applicationVersion }} · {{ runtimeInfo.platform || 'Windows' }}</div>
                </div>
              </div>
              <div class="pcl-about-grid">
                <span>数据目录</span><code>{{ runtimeInfo.dataRoot || '—' }}</code>
                <span>程序目录</span><code>{{ runtimeInfo.root || runtimeInfo.programRoot || '—' }}</code>
                <span>API 版本</span><code>{{ runtimeInfo.apiVersion || runtimeInfo.capabilities?.apiVersion || '1' }}</code>
                <span>能力</span><code>{{ (runtimeInfo.capabilities || []).join(' / ') || '—' }}</code>
              </div>
              <div class="pcl-actions">
                <PclButton icon="external-link" @click="openUrl('https://github.com/PrismLauncher/PrismLauncher')">项目主页</PclButton>
                <PclButton icon="book-marked" @click="openUrl('https://github.com/PrismLauncher/PrismLauncher/wiki')">文档</PclButton>
              </div>
            </PclCard>

            <PclCard title="特别鸣谢">
              <PclListItem
                v-for="credit in credits"
                :key="credit.name"
                :title="credit.name"
                :info="credit.info"
                @click="openUrl(credit.url)"
              >
                <template #icon><PclIcon name="heart-filled" :size="18" /></template>
                <template #actions>
                  <PclButton size="sm" variant="ghost" icon="external-link" @click.stop="openUrl(credit.url)">打开</PclButton>
                </template>
              </PclListItem>
            </PclCard>

            <PclCard title="贡献者们">
              <div class="pcl-credit-tiles">
                <span v-for="name in contributors" :key="name">{{ name }}</span>
              </div>
              <p class="pcl-muted pcl-credit-note">以上为 Luna Neo UI 与上游项目的公开贡献者代表；完整名单见各项目仓库。</p>
            </PclCard>

            <PclCard title="许可与版权">
              <p class="pcl-muted">Luna Launcher 基于 PrismLauncher 与 PCL-CE 的公开实现。界面参考 PCL-CE 的布局与交互，代码按各自许可证发布。</p>
              <p class="pcl-muted">PCL-CE、PrismLauncher 及其依赖项的版权归各自作者所有。</p>
              <div class="pcl-actions">
                <PclButton icon="file-box" @click="openUrl('https://github.com/PCL-Community/PCL-CE/blob/dev/LICENSE')">PCL-CE 许可证</PclButton>
                <PclButton icon="file-box" @click="openUrl('https://github.com/PrismLauncher/PrismLauncher/blob/develop/LICENSE')">PrismLauncher 许可证</PclButton>
                <PclButton icon="file-box" @click="openUrl('https://lucide.dev/license')">Lucide 许可证</PclButton>
              </div>
            </PclCard>
          </template>

          <template v-else-if="section === 'update'">
            <PclCard title="更新设置">
              <div class="pcl-check-grid">
                <PclCheckbox :model-value="updateState.automatic" label="自动检查更新" @update:model-value="configureUpdate({ automatic: $event })" />
                <PclCheckbox :model-value="updateState.beta" label="接收测试版更新" @update:model-value="configureUpdate({ beta: $event })" />
              </div>
              <div class="pcl-actions">
                <PclButton variant="highlight" icon="refresh-cw" :disabled="busy || updateChecking" @click="checkUpdate">{{ updateChecking ? '正在检查' : '检查更新' }}</PclButton>
                <PclButton icon="folder-open" @click="openUpdateLog">查看更新日志</PclButton>
              </div>
              <PclLoading v-if="updateChecking" text="正在检查更新" inline />
            </PclCard>

            <PclCard v-if="updateCheck.available" title="发现新版本">
              <div class="pcl-update-head">
                <div class="pcl-update-icon">↑</div>
                <div>
                  <div class="pcl-update-name">{{ updateCheck.versionName }} <span>{{ updateCheck.versionTag }}</span></div>
                  <div class="pcl-muted">{{ updateCheck.releaseTime }}</div>
                </div>
                <PclButton variant="accent" icon="download" :disabled="busy" @click="applyUpdate">安装</PclButton>
              </div>
              <pre v-if="updateCheck.releaseNotes" class="pcl-release-notes">{{ updateCheck.releaseNotes }}</pre>
            </PclCard>

            <PclCard v-else-if="updateChecked" title="当前版本">
              <p>已是最新版本。最近检查：{{ updateState.lastCheck || '刚刚' }}</p>
            </PclCard>

            <PclCard v-else title="更新状态">
              <div class="pcl-about-grid">
                <span>自动检查</span><code>{{ updateState.automatic ? '开启' : '关闭' }}</code>
                <span>测试版通道</span><code>{{ updateState.beta ? '开启' : '关闭' }}</code>
                <span>上次检查</span><code>{{ updateState.lastCheck || '—' }}</code>
              </div>
            </PclCard>
          </template>

          <template v-else-if="section === 'feedback'">
            <PclCard title="提交反馈">
              <p class="pcl-feedback-copy">在提交反馈之前，请先搜索是否已经存在相同的问题。请尽量附上启动日志、复现步骤和系统信息；不要提交包含账户令牌或私人路径的内容。</p>
              <PclButton variant="highlight" icon="message-circle" @click="openUrl('https://github.com/PrismLauncher/PrismLauncher/issues')">前往反馈</PclButton>
            </PclCard>
            <PclCard title="反馈进度">
              <p class="pcl-muted">Luna 暂未接入 PCL 的反馈看板。你可以直接在上方打开问题列表查看处理状态。</p>
            </PclCard>
          </template>

          <template v-else-if="section === 'logs'">
            <PclCard title="日志操作">
              <div class="pcl-actions pcl-log-actions">
                <PclButton variant="highlight" icon="folder-open" :disabled="busy" @click="openLogs">打开日志目录</PclButton>
                <PclButton icon="scroll-text" :disabled="busy" @click="openUpdateLog">更新日志</PclButton>
                <PclButton icon="download" :disabled="true" title="Luna 暂未提供日志导出为 ZIP 的操作">导出日志</PclButton>
                <PclButton variant="red" icon="trash-2" :disabled="true" title="请在系统文件管理器中清理历史日志">清理历史日志</PclButton>
              </div>
            </PclCard>
            <PclCard title="最近的启动器更新日志">
              <pre v-if="updateState.updateLog" class="pcl-log-tail">{{ updateState.updateLog }}</pre>
              <div v-else class="pcl-empty">还没有可显示的更新日志。</div>
            </PclCard>
          </template>
        </div>
      </Transition>
    </main>
  </section>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, reactive, ref, watch } from 'vue';
import { launcher } from '../../../boot/launcher-api';
import PclPageNav from '../../components/PclPageNav.vue';
import PclCard from '../../components/PclCard.vue';
import PclButton from '../../components/PclButton.vue';
import PclHint from '../../components/PclHint.vue';
import PclInput from '../../components/PclInput.vue';
import PclSelect from '../../components/PclSelect.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclRadio from '../../components/PclRadio.vue';
import PclSlider from '../../components/PclSlider.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclAccountPage from '../accounts/PclAccountPage.vue';
import PclJavaPanel from './PclJavaPanel.vue';
import PclAppearancePanel from './PclAppearancePanel.vue';
import type { PclNavGroup } from '../../types';
import { hiddenPclFeatures, isPclFeatureHidden } from '../../composables/usePclFeatures';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
type ApiSetting = { key: string; value: unknown; type?: string; category?: string; surface?: string };

const props = defineProps<{ settings: ApiSetting[]; busy: boolean; scope: Record<string, unknown>; execute: Execute; accounts?: any[]; embedded?: boolean; initialSection?: string }>();
const emit = defineEmits<{ changed: [] }>();

const section = ref('launch');
const error = ref(''), notice = ref('');
const drafts = reactive<Record<string, any>>({});
const languages = ref<any[]>([]), languageId = ref(''), useSystemLocale = ref(false), languagesLoading = ref(false);
const runtimeInfo = ref<any>({});
const updateState = ref<any>({}), updateCheck = ref<any>({}), updateChecked = ref(false), updateChecking = ref(false);
const proxy = reactive({ type: 'None', host: '', port: 7890, username: '', password: '' });
const proxyTestUrl = ref('https://api.minecraftservices.com/'), proxyTestResult = ref('');

const isInstance = computed(() => props.scope.scope === 'instance');
watch(() => props.initialSection, value => { if (value && value !== section.value) section.value = value; }, { immediate: true });
const launcherGroups: PclNavGroup[] = [
  { label: '游戏', items: [
    { id: 'launch', label: '启动', icon: 'rocket', refresh: true },
    { id: 'java', label: 'Java', icon: 'coffee', refresh: true },
    { id: 'game', label: '管理', icon: 'book-marked', refresh: true },
  ] },
  { label: '工具', items: [
    { id: 'network', label: '联机', icon: 'bubbles', refresh: true },
  ] },
  { label: '启动器', items: [
    { id: 'accounts', label: '账户管理', icon: 'user', refresh: true },
    { id: 'ui', label: '个性化', icon: 'palette', refresh: true },
    { id: 'language', label: '语言', icon: 'earth', refresh: true },
    { id: 'misc', label: '杂项', icon: 'monitor-cog', refresh: true },
  ] },
  { label: '关于', items: [
    { id: 'about', label: '软件信息', icon: 'info' },
    { id: 'update', label: '软件更新', icon: 'refresh-cw' },
    { id: 'feedback', label: '反馈', icon: 'message-circle', refresh: true },
    { id: 'logs', label: '查看日志', icon: 'scroll-text' },
  ] },
];
const instanceGroups: PclNavGroup[] = [
  { label: '实例设置', items: [
    { id: 'launch', label: '启动选项', icon: 'rocket', refresh: true },
    { id: 'java', label: 'Java', icon: 'coffee', refresh: true },
    { id: 'memory', label: '内存', icon: 'gauge', refresh: true },
    { id: 'network', label: '服务端', icon: 'server', refresh: true },
    { id: 'advanced', label: '高级', icon: 'tool-case' },
  ] },
];
const groups = computed(() => {
  const source = isInstance.value ? instanceGroups : launcherGroups;
  return source
    .map(group => ({
      ...group,
      items: group.items.filter(item => item.id === 'launch' || !isPclFeatureHidden(`setup.${item.id}`)),
    }))
    .filter(group => group.items.length);
});

const windowOptions = [
  { label: '默认', value: 'default' },
  { label: '最大化', value: 'maximized' },
  { label: '自定义尺寸', value: 'custom' },
];
const mirrorOptions = [
  { label: '官方源', value: 0 },
  { label: 'BMCLAPI 镜像', value: 1 },
  { label: '自定义镜像', value: 2 },
];
const proxyTypeOptions = [
  { label: '不使用代理', value: 'None' },
  { label: '跟随系统', value: 'Default' },
  { label: 'HTTP 代理', value: 'HTTP' },
  { label: 'SOCKS5 代理', value: 'SOCKS5' },
];
const languageOptions = computed(() => languages.value.map(item => ({
  label: item.name || item.id,
  value: item.id,
})));

const settingsMap = computed(() => new Map(props.settings.map(setting => [setting.key, setting])));
function val(key: string, fallback: any = ''): any {
  const setting = settingsMap.value.get(key);
  return setting ? setting.value : fallback;
}
function num(key: string, fallback = 0): number {
  const value = Number(val(key, fallback));
  return Number.isFinite(value) ? value : fallback;
}
function bool(key: string): boolean {
  const value = val(key, false);
  if (typeof value === 'string') {
    const normalized = value.trim().toLowerCase();
    return normalized === 'true' || normalized === '1' || normalized === 'yes' || normalized === 'on';
  }
  return Boolean(value);
}
function draftNumber(key: string, fallback = 0): number {
  const value = Number(drafts[key]);
  return Number.isFinite(value) ? value : num(key, fallback);
}
function draftValue(key: string, fallback = ''): string {
  return drafts[key] === undefined || drafts[key] === null ? String(val(key, fallback)) : String(drafts[key]);
}
function formatMib(value: number): string {
  const gib = value / 1024;
  return `${Number.isInteger(gib) ? gib : gib.toFixed(1)} GiB`;
}
function formatSpeed(value: number): string {
  if (!value) return '不限速';
  return value >= 1024 ? `${(value / 1024).toFixed(value % 1024 ? 1 : 0)} MiB/s` : `${value} KiB/s`;
}

const applicationVersion = computed(() => String(runtimeInfo.value.applicationVersion || runtimeInfo.value.version || runtimeInfo.value.launcherVersion || '开发版本').trim());
const credits = [
  { name: 'PCL-CE', info: '界面布局、控件尺寸与交互参考', url: 'https://github.com/PCL-Community/PCL-CE' },
  { name: 'PrismLauncher', info: '实例、账户、下载与启动后端', url: 'https://github.com/PrismLauncher/PrismLauncher' },
  { name: 'PCL 上游', info: 'Plain Craft Launcher 原始项目', url: 'https://github.com/Meloong-Git/PCL' },
  { name: 'Lucide', info: '线性 SVG 图标集', url: 'https://lucide.dev' },
  { name: 'Qt', info: '启动器与元数据工具链', url: 'https://www.qt.io' },
];
const contributors = ['PCL-CE', 'PrismLauncher', 'Luna Neo UI', 'Lucide', 'Qt', 'MCBBS 社区', 'Bangbang93', 'Pysio'];
const maxMem = computed(() => draftNumber('MaxMemAlloc', 4096));
const memoryMode = ref<'auto' | 'custom'>('custom');
const memoryStats = ref({ totalMiB: 0, usedMiB: 0 });
const memoryTotalGiB = computed(() => (memoryStats.value.totalMiB / 1024).toFixed(1));
const memoryUsedGiB = computed(() => (memoryStats.value.usedMiB / 1024).toFixed(1));
const memoryUsedPercent = computed(() => memoryStats.value.totalMiB > 0
  ? Math.max(0, Math.min(100, memoryStats.value.usedMiB * 100 / memoryStats.value.totalMiB))
  : 0);
const memoryAllocPercent = computed(() => memoryStats.value.totalMiB > 0
  ? Math.max(0, Math.min(100, maxMem.value * 100 / memoryStats.value.totalMiB))
  : 0);
const memoryAllocAfterPercent = computed(() => Math.max(0, Math.min(100 - memoryUsedPercent.value, memoryAllocPercent.value)));
let memorySubscription = '';
let unlistenMemory: (() => void) | undefined;
async function refreshMemory() {
  try {
    const result = await props.execute('runtime.memory', {});
    const data = result?.data || result || {};
    const total = Number(data.totalMiB || 0), used = Number(data.usedMiB || 0);
    if (total > 0) memoryStats.value = { totalMiB: total, usedMiB: Math.max(0, Math.min(total, used)) };
  } catch {
    // Older backends may not expose runtime.memory.
  }
}
async function subscribeMemory() {
  try {
    const result: any = await props.execute('runtime.memory.subscribe');
    memorySubscription = String(result?.subscriptionId || '');
    if (!memorySubscription) return;
    unlistenMemory = await launcher.onStream((batch: any) => {
      if (!batch || batch.subscriptionId !== memorySubscription) return;
      for (const event of batch.events || []) {
        if (event.kind !== 'memory.sample') continue;
        const total = Number(event.totalMiB || 0);
        if (total > 0) memoryStats.value = { totalMiB: total, usedMiB: Math.max(0, Math.min(total, Number(event.usedMiB || 0))) };
      }
    });
  } catch {
    // Older backends may not expose the memory stream; the one-shot read remains.
  }
}
onMounted(() => {
  void refreshMemory();
  void subscribeMemory();
});
onBeforeUnmount(() => {
  unlistenMemory?.();
  if (memorySubscription) {
    const subscriptionId = memorySubscription;
    memorySubscription = '';
    void props.execute('event.unsubscribe', { subscriptionId }).catch(() => {});
  }
});
const windowMode = ref<'default' | 'maximized' | 'custom'>('default');

watch(() => props.settings, items => {
  for (const setting of items) drafts[setting.key] = setting.value;
  memoryMode.value = num('MinMemAlloc', 512) === 512 && num('MaxMemAlloc', 4096) === 4096 ? 'auto' : 'custom';
  windowMode.value = bool('LaunchMaximized') ? 'maximized'
    : (num('MinecraftWinWidth', 854) !== 854 || num('MinecraftWinHeight', 480) !== 480) ? 'custom' : 'default';
}, { immediate: true, deep: false });

watch(isInstance, () => { section.value = 'launch'; error.value = ''; notice.value = ''; });
watch(hiddenPclFeatures, () => {
  if (!groups.value.some(group => group.items.some(item => item.id === section.value))) section.value = 'launch';
}, { deep: true });
watch(section, value => {
  if (value === 'language') void loadLanguages();
  if (value === 'about' || value === 'logs') void loadRuntime();
  if (value === 'update') void loadUpdate();
  if (value === 'misc' && !isInstance.value) void loadProxy();
});
watch(windowMode, value => {
  if (value === 'maximized') void save('LaunchMaximized', true);
  else {
    void save('LaunchMaximized', false);
    if (value === 'custom') {
      if (num('MinecraftWinWidth', 854) === 854 && num('MinecraftWinHeight', 480) === 480) {
        void save('MinecraftWinWidth', 1280);
        void save('MinecraftWinHeight', 720);
      }
    }
  }
});

function message(value: unknown): string {
  return value instanceof Error ? value.message : String(value);
}
async function save(key: string, value: unknown) {
  error.value = '';
  const setting = settingsMap.value.get(key);
  const normalized = setting?.type === 'QString' && typeof value === 'boolean' ? String(value) : value;
  drafts[key] = normalized;
  try {
    await props.execute('settings.set', { ...props.scope, key, value: normalized });
    notice.value = '设置已保存';
    window.setTimeout(() => { if (notice.value === '设置已保存') notice.value = ''; }, 1800);
    emit('changed');
  } catch (exception) {
    error.value = message(exception);
  }
}
function onMemoryMode(value: string | number | boolean) {
  memoryMode.value = value === 'auto' ? 'auto' : 'custom';
  if (memoryMode.value === 'auto') {
    void save('MinMemAlloc', 512);
    void save('MaxMemAlloc', 4096);
  }
}
function refreshSection(id: string) {
  if (id === 'language') void loadLanguages(true);
  else if (id === 'update') void loadUpdate();
  else if (id === 'misc' && !isInstance.value) void loadProxy();
  else emit('changed');
}

async function loadLanguages(force = false) {
  if (languagesLoading.value) return;
  if (languages.value.length && !force) return;
  languagesLoading.value = true;
  error.value = '';
  try {
    const result = await props.execute('language.list');
    languages.value = result?.languages || [];
    languageId.value = result?.selected || '';
    useSystemLocale.value = Boolean(result?.useSystemLocale);
  } catch (exception) {
    error.value = message(exception);
  } finally {
    languagesLoading.value = false;
  }
}
async function selectLanguage(id: string | number) {
  error.value = '';
  try {
    const result = await props.execute('language.select', { language: String(id) });
    languageId.value = result?.selected || String(id);
    notice.value = '语言已更新';
    emit('changed');
  } catch (exception) {
    error.value = message(exception);
  }
}
async function setSystemLocale(value: boolean) {
  useSystemLocale.value = value;
  if (!languageId.value) return;
  try {
    await props.execute('language.select', { language: languageId.value, useSystemLocale: value });
    emit('changed');
  } catch (exception) {
    error.value = message(exception);
  }
}
async function loadRuntime() {
  try {
    runtimeInfo.value = await props.execute('runtime.info');
  } catch (exception) {
    error.value = message(exception);
  }
}
async function loadUpdate() {
  try {
    updateState.value = await props.execute('launcher.update.status');
  } catch (exception) {
    error.value = message(exception);
  }
}
async function configureUpdate(values: Record<string, unknown>) {
  try {
    updateState.value = await props.execute('launcher.update.configure', values);
    emit('changed');
  } catch (exception) {
    error.value = message(exception);
  }
}
async function checkUpdate() {
  if (updateChecking.value) return;
  updateChecking.value = true;
  error.value = '';
  try {
    updateCheck.value = await props.execute('launcher.update.check');
    updateChecked.value = true;
    if (updateCheck.value?.available) notice.value = `发现新版本 ${updateCheck.value.versionName || ''}`;
    else notice.value = '当前已是最新版本';
    await loadUpdate();
  } catch (exception) {
    error.value = message(exception);
  } finally {
    updateChecking.value = false;
  }
}
async function applyUpdate() {
  if (!updateCheck.value?.versionTag) return;
  error.value = '';
  try {
    await props.execute('launcher.update.apply', { versionTag: updateCheck.value.versionTag });
    notice.value = '更新程序已启动，请按提示完成安装';
  } catch (exception) {
    error.value = message(exception);
  }
}
async function loadProxy() {
  try {
    const result = await props.execute('network.proxy.get');
    proxy.type = result.type || 'None';
    proxy.host = result.host || '';
    proxy.port = Number(result.port) || 7890;
    proxy.username = result.username || '';
    proxy.password = result.password || '';
  } catch (exception) {
    error.value = message(exception);
  }
}
async function saveProxy() {
  error.value = '';
  try {
    const result = await props.execute('network.proxy.set', {
      type: proxy.type,
      host: proxy.host,
      port: Number(proxy.port) || 1080,
      username: proxy.username,
      password: proxy.password,
    });
    proxy.type = result.type || proxy.type;
    notice.value = '代理设置已应用';
  } catch (exception) {
    error.value = message(exception);
  }
}
async function testProxy() {
  proxyTestResult.value = '';
  try {
    const result = await props.execute('network.proxy.test', { url: proxyTestUrl.value, timeoutMs: 10000 });
    proxyTestResult.value = result?.status
      ? `连接成功：HTTP ${result.status}，耗时 ${result.timeMs ?? '—'} ms`
      : `连接成功：${JSON.stringify(result)}`;
  } catch (exception) {
    proxyTestResult.value = `连接失败：${message(exception)}`;
  }
}
async function openUrl(url: string) {
  try {
    await props.execute('desktop.open-url', { url });
  } catch (exception) {
    error.value = message(exception);
  }
}
async function openPath(path: string) {
  try {
    await props.execute('desktop.open-path', { path });
  } catch (exception) {
    error.value = message(exception);
  }
}
async function openLogs() {
  try {
    const info = runtimeInfo.value.dataRoot ? runtimeInfo.value : await props.execute('runtime.info');
    const root = info.dataRoot || info.root;
    if (!root) throw new Error('无法读取日志目录');
    await openPath(`${root}/logs`);
  } catch (exception) {
    error.value = message(exception);
  }
}
async function openUpdateLog() {
  try {
    const status = updateState.value.updateLogPath ? updateState.value : await props.execute('launcher.update.status');
    if (status.updateLogPath) await openPath(status.updateLogPath);
    else await openLogs();
  } catch (exception) {
    error.value = message(exception);
  }
}
</script>

<style scoped>
.pcl-setup-page {
  background: var(--pcl-page-bg);
}

.pcl-setup-stack {
  max-width: 100%;
}

.pcl-form-row--top {
  align-items: flex-start;
  padding-top: 4px;
}

.pcl-form-row__control--column {
  flex-direction: column;
  align-items: stretch;
}

.pcl-check-row {
  flex-wrap: wrap;
  gap: 8px 18px;
}

.pcl-check-grid {
  display: flex;
  flex-wrap: wrap;
  gap: 7px 24px;
  margin: 4px 0 2px;
}

.pcl-choice-row {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 10px 22px;
  min-height: 28px;
  margin-bottom: 10px;
}

.pcl-memory-live-head {
  display: flex;
  align-items: center;
  gap: 6px;
  margin-top: 4px;
  color: var(--pcl-muted);
  font-size: calc(11px * var(--pcl-font-scale, 1));
}

.pcl-memory-live-head strong {
  color: var(--pcl-text);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 400;
}

.pcl-memory-live-head em {
  margin: 0 2px;
  font-style: normal;
  opacity: .6;
}

.pcl-memory-live-dot {
  width: 6px;
  height: 6px;
  margin-left: auto;
  border-radius: 50%;
  background: #34d399;
  animation: pcl-memory-pulse 2s ease-out infinite;
}

.pcl-memory-live-label {
  font-size: calc(10px * var(--pcl-font-scale, 1));
}

@keyframes pcl-memory-pulse {
  0% { box-shadow: 0 0 0 0 rgba(52, 211, 153, .5); }
  70% { box-shadow: 0 0 0 5px rgba(52, 211, 153, 0); }
  100% { box-shadow: 0 0 0 0 rgba(52, 211, 153, 0); }
}

.pcl-memory-bar {
  position: relative;
  display: flex;
  width: 100%;
  height: 9px;
  margin: 8px 0 7px;
  overflow: hidden;
  border-radius: 3px;
  background: var(--pcl-c6);
}

.pcl-memory-bar.disabled {
  opacity: .45;
}

.pcl-memory-bar__allocated {
  position: absolute;
  top: 0;
  bottom: 0;
  border-radius: 0 3px 3px 0;
  background: #8fc4ff;
  transition: left .8s cubic-bezier(.16, 1, .3, 1), width .8s cubic-bezier(.16, 1, .3, 1);
}

.pcl-memory-bar__used {
  position: absolute;
  top: 0;
  bottom: 0;
  left: 0;
  border-radius: 3px 0 0 3px;
  background: var(--pcl-c3);
  transition: width .8s cubic-bezier(.16, 1, .3, 1);
}

.pcl-memory-bar__divider {
  position: absolute;
  top: 0;
  bottom: 0;
  width: 2px;
  background: #fff;
  box-shadow: 0 0 0 1px rgba(42, 128, 227, .32);
  transition: left .8s cubic-bezier(.16, 1, .3, 1);
}

.pcl-memory-values {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 12px;
  margin-bottom: 5px;
}

.pcl-memory-values strong {
  color: var(--pcl-text);
  font-size: calc(14px * var(--pcl-font-scale, 1));
  font-weight: 400;
}

.pcl-memory-values em {
  margin: 0 3px;
  color: var(--pcl-muted);
  font-style: normal;
}

.pcl-memory-live-right {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  margin-left: auto;
}

.pcl-memory-legend {
  display: flex;
  justify-content: space-between;
  gap: 12px;
  margin-bottom: 10px;
  color: var(--pcl-text);
  font-size: calc(11px * var(--pcl-font-scale, 1));
  opacity: .72;
}

.pcl-memory-legend b {
  margin-right: 5px;
  font-weight: 400;
  opacity: .7;
}

.pcl-advanced-hint {
  margin-top: 12px;
}

.pcl-actions {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 8px;
  margin-top: 14px;
}

.pcl-diagnostic {
  margin-top: 10px;
  padding: 9px 12px;
  border-radius: 3px;
  background: var(--pcl-c7);
  color: var(--pcl-muted);
  font: calc(12px * var(--pcl-font-scale, 1))/1.5 ui-monospace, Consolas, monospace;
  white-space: pre-wrap;
}

.pcl-language-banner {
  position: relative;
  display: flex;
  min-height: 230px;
  margin: 15px 0;
  padding: 25px;
  overflow: hidden;
  border-radius: 5px;
  background: var(--pcl-c7);
}

.pcl-language-banner__earth {
  position: absolute;
  top: -43px;
  right: -25px;
  color: var(--pcl-c5);
  opacity: .62;
}

.pcl-language-banner__copy {
  position: relative;
  z-index: 1;
  max-width: 560px;
  margin-right: 170px;
  color: var(--pcl-text);
}

.pcl-language-banner__title {
  margin-bottom: 10px;
  font-size: calc(22px * var(--pcl-font-scale, 1));
  font-weight: 700;
  line-height: 1.3;
}

.pcl-language-banner__title b {
  font-family: Bahnschrift, "Segoe UI", var(--pcl-font);
  font-size: calc(42px * var(--pcl-font-scale, 1));
  font-weight: 700;
  letter-spacing: -.02em;
}

.pcl-language-banner__copy p {
  margin: 5px 0;
  color: var(--pcl-text);
  font-size: calc(14px * var(--pcl-font-scale, 1));
  line-height: 23px;
  opacity: .82;
}

.pcl-about-head {
  display: flex;
  align-items: center;
  gap: 14px;
  padding: 4px 0 16px;
}

.pcl-about-logo {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 52px;
  height: 52px;
  border-radius: 12px;
  background: var(--pcl-c3);
  color: #fff;
  font-size: calc(28px * var(--pcl-font-scale, 1));
  font-weight: 800;
}

.pcl-about-name {
  font-size: calc(20px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-about-name span {
  margin-left: 5px;
  color: var(--pcl-c3);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 500;
  letter-spacing: .08em;
}

.pcl-about-version {
  margin-top: 3px;
  color: var(--pcl-muted);
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

.pcl-about-grid {
  display: grid;
  grid-template-columns: 90px minmax(0, 1fr);
  gap: 7px 12px;
  padding: 8px 0;
  font-size: calc(13px * var(--pcl-font-scale, 1));
}

.pcl-about-grid span {
  color: var(--pcl-muted);
}

.pcl-about-grid code {
  overflow: hidden;
  color: var(--pcl-text);
  font: calc(12px * var(--pcl-font-scale, 1))/1.5 ui-monospace, Consolas, monospace;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pcl-credit-tiles {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  padding: 4px 0 10px;
}

.pcl-credit-tiles span {
  padding: 5px 10px;
  border: 1px solid var(--pcl-border);
  border-radius: 4px;
  background: var(--pcl-c7);
  color: var(--pcl-text);
  font-size: calc(12px * var(--pcl-font-scale, 1));
}

.pcl-credit-note {
  margin: 0;
  font-size: calc(12px * var(--pcl-font-scale, 1));
  line-height: 1.7;
}

.pcl-update-head {
  display: flex;
  align-items: center;
  gap: 14px;
}

.pcl-update-icon {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 50px;
  height: 50px;
  border-radius: 8px;
  background: var(--pcl-c3);
  color: #fff;
  font-size: calc(24px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-update-name {
  font-size: calc(20px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

.pcl-update-name span {
  margin-left: 6px;
  color: var(--pcl-c3);
  font-size: calc(12px * var(--pcl-font-scale, 1));
  font-weight: 500;
}

.pcl-update-head .pcl-btn {
  margin-left: auto;
}

.pcl-release-notes,
.pcl-log-tail {
  max-height: 260px;
  margin: 14px 0 0;
  padding: 10px 12px;
  overflow: auto;
  border-radius: 3px;
  background: var(--pcl-c8);
  color: var(--pcl-muted);
  font: calc(12px * var(--pcl-font-scale, 1))/1.55 ui-monospace, Consolas, monospace;
  white-space: pre-wrap;
}

.pcl-feedback-copy {
  max-width: 620px;
  color: var(--pcl-text);
  font-size: calc(13px * var(--pcl-font-scale, 1));
  line-height: 1.8;
  opacity: .82;
}

.pcl-log-actions .pcl-btn {
  min-width: 120px;
}

.pcl-appearance-panel :deep(.preference-card) {
  border: 0;
  border-radius: 5px;
  background: var(--pcl-card);
  box-shadow: var(--pcl-shadow);
}

.pcl-appearance-panel :deep(.preference-card h2),
.pcl-appearance-panel :deep(.preference-card h3) {
  font-size: calc(13px * var(--pcl-font-scale, 1));
  font-weight: 700;
}

@media (max-width: 900px) {
  .pcl-language-banner__copy {
    margin-right: 60px;
  }
}

.pcl-setup-page.is-embedded {
  display: block;
  height: auto;
  max-width: none;
  margin: 0;
  overflow: visible;
}

.pcl-setup-page.is-embedded .pcl-page-main {
  width: 100%;
  height: auto;
  max-width: none;
  margin: 0;
  padding: 0;
  overflow: visible;
}
</style>
