<template>
  <section class="pcl-page pcl-setup-page" :class="{ 'is-embedded': embedded }">
    <PclPageNav v-if="!embedded" v-model="section" :groups="groups" @refresh="refreshSection" />

    <main class="pcl-page-main">
      <Transition name="pcl-page" mode="out-in">
        <div :key="section" class="pcl-page-view pcl-setup-stack">
          <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>
          <PclHint v-if="notice" theme="blue" closable @close="notice = ''">{{ notice }}</PclHint>

          <PclCard v-if="isInstance && sectionOverrideGroups.length" title="继承全局设置">
            <p class="pcl-muted">启用下列项目以使用当前实例的独立设置；关闭后恢复跟随全局。</p>
            <div class="pcl-check-grid">
              <PclCheckbox v-for="item in sectionOverrideGroups" :key="item.key" :model-value="bool(item.key)"
                :label="item.label" @update:model-value="save(item.key, $event)" />
            </div>
          </PclCard>

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
                  <PclSelect :model-value="windowMode" :options="windowOptions" @update:model-value="setWindowMode" style="width: 150px" />
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
            <PclCard title="高级启动选项">
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

          <template v-else-if="section === 'download-settings'">
            <PclCard title="下载引擎与镜像">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">下载引擎</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="num('DownloadBackend', 2)"
                    :options="downloadBackendOptions"
                    @update:model-value="save('DownloadBackend', $event)"
                  />
                </div>
                <span class="pcl-form-row__hint">控制资源、依赖和整合包下载方式</span>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">文件下载源</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="mirrorType"
                    :options="mirrorOptions"
                    @update:model-value="saveMirrorType($event)"
                  />
                </div>
                <span class="pcl-form-row__hint">Minecraft 资源与依赖</span>
              </div>

              <template v-if="mirrorType === 2">
                <PclHint theme="blue" class="pcl-advanced-hint">自定义镜像需要提供兼容 Mojang / Maven 路径的地址；留空的地址会按主地址的 BMCLAPI 目录结构补全。</PclHint>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">版本元数据地址</span>
                  <div class="pcl-form-row__control">
                    <PclInput
                      :model-value="draftValue('MetaURLOverride', '')"
                      placeholder="留空使用官方元数据地址"
                      @update:model-value="drafts.MetaURLOverride = $event"
                      @blur="save('MetaURLOverride', drafts.MetaURLOverride)"
                    />
                  </div>
                </div>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">Mojang 下载主地址</span>
                  <div class="pcl-form-row__control">
                    <PclInput
                      :model-value="draftValue('MojangDownloadsMirrorURL', '')"
                      placeholder="https://mirror.example.com/"
                      @update:model-value="drafts.MojangDownloadsMirrorURL = $event"
                      @blur="save('MojangDownloadsMirrorURL', drafts.MojangDownloadsMirrorURL)"
                    />
                  </div>
                </div>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">Libraries Maven 地址</span>
                  <div class="pcl-form-row__control">
                    <PclInput
                      :model-value="draftValue('LibrariesURL', '')"
                      placeholder="留空自动使用主地址 /maven/"
                      @update:model-value="drafts.LibrariesURL = $event"
                      @blur="save('LibrariesURL', drafts.LibrariesURL)"
                    />
                  </div>
                </div>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">资源对象地址</span>
                  <div class="pcl-form-row__control">
                    <PclInput
                      :model-value="draftValue('ResourceURLOverride', '')"
                      placeholder="留空自动使用主地址 /assets/"
                      @update:model-value="drafts.ResourceURLOverride = $event"
                      @blur="save('ResourceURLOverride', drafts.ResourceURLOverride)"
                    />
                  </div>
                </div>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">旧版 Forge Libraries 地址</span>
                  <div class="pcl-form-row__control">
                    <PclInput
                      :model-value="draftValue('LegacyFMLLibsURLOverride', '')"
                      placeholder="留空跟随 Libraries Maven 地址"
                      @update:model-value="drafts.LegacyFMLLibsURLOverride = $event"
                      @blur="save('LegacyFMLLibsURLOverride', drafts.LegacyFMLLibsURLOverride)"
                    />
                  </div>
                </div>
              </template>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">Modrinth 镜像</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="draftNumber('ModrinthMirror', 0)"
                    :options="modApiMirrorOptions"
                    @update:model-value="save('ModrinthMirror', $event)"
                  />
                  <PclInput
                    v-if="draftNumber('ModrinthMirror', 0) === 2"
                    :model-value="draftValue('ModrinthMirrorURL', '')"
                    placeholder="Modrinth 自定义 API 地址"
                    @update:model-value="drafts.ModrinthMirrorURL = $event"
                    @blur="save('ModrinthMirrorURL', drafts.ModrinthMirrorURL)"
                  />
                </div>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">CurseForge 镜像</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="draftNumber('CurseForgeMirror', 0)"
                    :options="modApiMirrorOptions"
                    @update:model-value="save('CurseForgeMirror', $event)"
                  />
                  <PclInput
                    v-if="draftNumber('CurseForgeMirror', 0) === 2"
                    :model-value="draftValue('CurseForgeMirrorURL', '')"
                    placeholder="CurseForge 自定义 API 地址"
                    @update:model-value="drafts.CurseForgeMirrorURL = $event"
                    @blur="save('CurseForgeMirrorURL', drafts.CurseForgeMirrorURL)"
                  />
                </div>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">最大下载线程</span>
                <div class="pcl-form-row__control">
                  <PclSlider
                    :model-value="draftNumber('PclDownloadThreadLimit', 8)"
                    :min="1"
                    :max="64"
                    :step="1"
                    @update:model-value="drafts.PclDownloadThreadLimit = $event"
                    @change="save('PclDownloadThreadLimit', $event)"
                  />
                </div>
                <span class="pcl-form-row__hint">每个下载任务 1–64 个连接</span>
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

              <PclCheckbox  :model-value="bool('PclDownloadFallbackToQt')" label="专用 .NET 下载引擎不可用时回退到 Qt 内置下载" @update:model-value="save('PclDownloadFallbackToQt', $event)" />
            </PclCard>
            <PclCard title="下载任务">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">同时处理的任务数</span>
                <div class="pcl-form-row__control"><PclNumberInput label="同时处理的任务数" :model-value="draftNumber('NumberOfConcurrentTasks', 10)" :min="1" :max="99" @update:model-value="save('NumberOfConcurrentTasks', $event)" /></div>
                <span class="pcl-form-row__hint">安装、更新和检查任务的总并发数</span>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">同时下载的文件数</span>
                <div class="pcl-form-row__control"><PclNumberInput label="同时下载的文件数" :model-value="draftNumber('NumberOfConcurrentDownloads', 6)" :min="1" :max="99" @update:model-value="save('NumberOfConcurrentDownloads', $event)" /></div>
                <span class="pcl-form-row__hint">限制同时建立的下载连接</span>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">失败重试次数</span>
                <div class="pcl-form-row__control"><PclNumberInput label="失败重试次数" :model-value="draftNumber('NumberOfManualRetries', 1)" :min="0" :max="99" @update:model-value="save('NumberOfManualRetries', $event)" /></div>
                <span class="pcl-form-row__hint">每个请求失败后的手动重试次数</span>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">请求超时</span>
                <div class="pcl-form-row__control"><PclNumberInput label="请求超时" :model-value="draftNumber('RequestTimeout', 60)" :min="0" :max="99" @update:model-value="save('RequestTimeout', $event)" /></div>
                <span class="pcl-form-row__hint">秒</span>
              </div>
            </PclCard>

            <PclCard title="aria2 下载引擎">
              <div class="pcl-check-grid">
                <PclCheckbox :model-value="bool('Aria2Enabled')" label="允许使用 aria2（下载引擎须选择 aria2）" @update:model-value="save('Aria2Enabled', $event)" />
                <PclCheckbox :model-value="bool('Aria2AutoInstall')" label="缺少时自动下载 aria2" @update:model-value="save('Aria2AutoInstall', $event)" />
                <PclCheckbox :model-value="bool('Aria2FallbackToQt')" label="aria2 不可用时回退到内置下载" @update:model-value="save('Aria2FallbackToQt', $event)" />
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">aria2c 程序路径</span>
                <div class="pcl-form-row__control"><PclInput :model-value="draftValue('Aria2ExecutablePath', '')" placeholder="留空自动检测" @update:model-value="drafts.Aria2ExecutablePath = $event" @blur="save('Aria2ExecutablePath', drafts.Aria2ExecutablePath)" /><PclIconButton icon="folder-open" label="选择 aria2c" :size="28" :disabled="busy" @click="chooseFilePath('Aria2ExecutablePath', 'aria2c 可执行文件')" /></div>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">下载器并发数</span>
                <div class="pcl-form-row__control"><PclNumberInput label="aria2 并发下载数" :model-value="draftNumber('Aria2MaxConcurrentDownloads', 6)" :min="1" :max="128" :disabled="bool('Aria2FollowLauncherDownloadLimits')" @update:model-value="save('Aria2MaxConcurrentDownloads', $event)" /></div>
                <span class="pcl-form-row__hint">aria2 同时下载任务数</span>
              </div>
              <PclCheckbox :model-value="bool('Aria2FollowLauncherDownloadLimits')" label="跟随上面的启动器下载限制" @update:model-value="save('Aria2FollowLauncherDownloadLimits', $event)" />
            </PclCard>

            <PclCard title="aria2 传输参数" collapsible :open="true">
              <div class="pcl-form-row"><span class="pcl-form-row__label">每台服务器连接数</span><div class="pcl-form-row__control"><PclNumberInput label="每台服务器连接数" :model-value="draftNumber('Aria2MaxConnectionPerServer', 8)" :min="1" :max="64" @update:model-value="save('Aria2MaxConnectionPerServer', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">分片数</span><div class="pcl-form-row__control"><PclNumberInput label="分片数" :model-value="draftNumber('Aria2Split', 8)" :min="1" :max="64" @update:model-value="save('Aria2Split', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">最小分片大小</span><div class="pcl-form-row__control"><PclInput :model-value="draftValue('Aria2MinSplitSize', '1M')" placeholder="例如 10M" @update:model-value="drafts.Aria2MinSplitSize = $event" @blur="save('Aria2MinSplitSize', drafts.Aria2MinSplitSize)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">文件分配方式</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('Aria2FileAllocation', 'none')" :options="[{ label: '不预分配', value: 'none' }, { label: '预分配', value: 'prealloc' }, { label: '截断', value: 'trunc' }, { label: 'falloc', value: 'falloc' }]" @update:model-value="save('Aria2FileAllocation', $event)" /></div></div>
              <PclCheckbox :model-value="bool('Aria2Continue')" label="继续未完成的 aria2 下载" @update:model-value="save('Aria2Continue', $event)" />
              <div class="pcl-form-row"><span class="pcl-form-row__label">RPC 端口</span><div class="pcl-form-row__control"><PclNumberInput label="RPC 端口" :model-value="draftNumber('Aria2RpcPort', 0)" :min="0" :max="65535" @update:model-value="save('Aria2RpcPort', $event)" /></div><span class="pcl-form-row__hint">0 表示随机端口</span></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">状态刷新间隔</span><div class="pcl-form-row__control"><PclNumberInput label="状态刷新间隔" :model-value="draftNumber('Aria2PollInterval', 500)" :min="250" :max="10000" @update:model-value="save('Aria2PollInterval', $event)" /></div><span class="pcl-form-row__hint">毫秒</span></div>
            </PclCard>

            <PclCard title="aria2 代理" collapsible>
              <PclCheckbox :model-value="bool('Aria2UseLauncherProxy')" label="使用启动器的代理设置" @update:model-value="save('Aria2UseLauncherProxy', $event)" />
              <div class="pcl-form-row"><span class="pcl-form-row__label">代理类型</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('Aria2ProxyType', 'None')" :disabled="bool('Aria2UseLauncherProxy')" :options="[{ label: '不使用代理', value: 'None' }, { label: 'HTTP', value: 'HTTP' }, { label: 'SOCKS5', value: 'SOCKS5' }]" @update:model-value="save('Aria2ProxyType', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">地址 / 端口</span><div class="pcl-form-row__control"><PclInput :model-value="draftValue('Aria2ProxyAddr', '')" :disabled="ariaProxyDisabled" placeholder="127.0.0.1" @update:model-value="drafts.Aria2ProxyAddr = $event" @blur="save('Aria2ProxyAddr', drafts.Aria2ProxyAddr)" /><PclNumberInput label="aria2 代理端口" :model-value="draftNumber('Aria2ProxyPort', 8080)" :min="1" :max="65535" :disabled="ariaProxyDisabled" @update:model-value="save('Aria2ProxyPort', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">账号 / 密码</span><div class="pcl-form-row__control"><PclInput :model-value="draftValue('Aria2ProxyUser', '')" :disabled="ariaProxyDisabled" placeholder="可选" @update:model-value="drafts.Aria2ProxyUser = $event" @blur="save('Aria2ProxyUser', drafts.Aria2ProxyUser)" /><PclSecretInput label="代理密码" :configured="secretConfigured('Aria2ProxyPass')" :disabled="ariaProxyDisabled" :save="value => save('Aria2ProxyPass', value)" /></div></div>
              <div class="pcl-form-row pcl-form-row--top"><span class="pcl-form-row__label">额外 aria2c 参数</span><div class="pcl-form-row__control pcl-form-row__control--column"><PclInput multiline :rows="3" :model-value="draftValue('Aria2ExtraArgs', '')" placeholder="每行一个参数，例如 --check-certificate=false" @update:model-value="drafts.Aria2ExtraArgs = $event" @blur="save('Aria2ExtraArgs', drafts.Aria2ExtraArgs)" /></div></div>
            </PclCard>

            <PclCard title="第三方服务">
              <div class="pcl-form-row"><span class="pcl-form-row__label">CurseForge 下载方式</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('CurseForgeDownloadBrowser', 'Embedded')" :options="[{ label: '内嵌浏览器', value: 'Embedded' }, { label: '系统浏览器', value: 'System' }, { label: '外部工具', value: 'External' }]" @update:model-value="save('CurseForgeDownloadBrowser', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">Modrinth Token</span><div class="pcl-form-row__control"><PclSecretInput label="Modrinth Token" :configured="secretConfigured('ModrinthToken')" :disabled="busy" :save="value => save('ModrinthToken', value)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">CurseForge API Key</span><div class="pcl-form-row__control"><PclSecretInput label="CurseForge API Key" :configured="secretConfigured('FlameKeyOverride')" :disabled="busy" :save="value => save('FlameKeyOverride', value)" /></div></div>
            </PclCard>
          </template>

          <template v-else-if="section === 'paths'">
            <PclCard title="文件夹位置">
              <div v-for="item in pathSettings" :key="item.key" class="pcl-form-row">
                <span class="pcl-form-row__label">{{ item.label }}</span>
                <div class="pcl-form-row__control"><PclInput :model-value="draftValue(item.key, '')" :placeholder="'placeholder' in item ? item.placeholder : '输入目录路径'" @update:model-value="drafts[item.key] = $event" @blur="save(item.key, drafts[item.key])" /><PclIconButton icon="folder-open" :label="`选择${item.label}`" :size="28" :disabled="busy" @click="chooseDirectory(item.key, item.label)" /></div>
              </div>
            </PclCard>
            <PclCard title="下载目录行为">
              <div class="pcl-check-grid"><PclCheckbox :model-value="bool('DownloadsDirWatchRecursive')" label="递归监视下载目录" @update:model-value="save('DownloadsDirWatchRecursive', $event)" /><PclCheckbox :model-value="bool('MoveModsFromDownloadsDir')" label="将下载的模组移动到实例模组目录" @update:model-value="save('MoveModsFromDownloadsDir', $event)" /></div>
            </PclCard>
            <PclCard title="外部工具路径">
              <PclCheckbox  :model-value="bool('CurseForgeExternalToolEnabled')" label="允许使用 CurseForge 外部下载工具" @update:model-value="save('CurseForgeExternalToolEnabled', $event)" />

              <div v-for="item in toolPathSettings" :key="item.key" class="pcl-form-row"><span class="pcl-form-row__label">{{ item.label }}</span><div class="pcl-form-row__control"><PclInput :model-value="draftValue(item.key, '')" placeholder="可选" @update:model-value="drafts[item.key] = $event" @blur="save(item.key, drafts[item.key])" /><PclIconButton icon="file" :label="`选择${item.label}`" :size="28" :disabled="busy" @click="chooseFilePath(item.key, item.label)" /></div></div>
            </PclCard>
          </template>

          <template v-else-if="section === 'console'">
            <PclCard title="控制台输出">
              <div class="pcl-form-row"><span class="pcl-form-row__label">最大保留行数</span><div class="pcl-form-row__control"><PclSlider :model-value="draftNumber('ConsoleMaxLines', 100000)" :min="100" :max="100000" :step="100" @update:model-value="drafts.ConsoleMaxLines = $event" @change="save('ConsoleMaxLines', $event)" /></div><span class="pcl-form-row__hint">{{ draftNumber('ConsoleMaxLines', 100000).toLocaleString() }} 行</span></div>
              <div class="pcl-check-grid"><PclCheckbox :model-value="bool('ConsoleOverflowStop')" label="日志溢出时停止输出" @update:model-value="save('ConsoleOverflowStop', $event)" /><PclCheckbox :model-value="bool('LogPrePostOutput')" label="记录启动前后输出" @update:model-value="save('LogPrePostOutput', $event)" /></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">控制台字体大小</span><div class="pcl-form-row__control"><PclNumberInput label="控制台字体大小" :model-value="draftNumber('ConsoleFontSize', 10)" :min="5" :max="72" @update:model-value="save('ConsoleFontSize', $event)" /></div><span class="pcl-form-row__hint">pt</span></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">配色方案</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('ConsoleColorScheme', 'Builtin Dark')" :options="consoleColorOptions" @update:model-value="save('ConsoleColorScheme', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">控制台字体</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('ConsoleFont')" :options="consoleFontOptions" @update:model-value="save('ConsoleFont', $event)" /><PclInput :model-value="draftValue('ConsoleFont', '')" placeholder="或输入自定义字体名称" @update:model-value="drafts.ConsoleFont = $event" @blur="save('ConsoleFont', drafts.ConsoleFont)" /></div></div>
              <PclHint theme="blue">字体用于 C++ 游戏日志和服务器终端，配色用于 C++ 服务器终端。</PclHint>
              <pre class="pcl-console-preview" :style="{ fontFamily: draftValue('ConsoleFont', 'monospace'), fontSize: `${draftNumber('ConsoleFontSize', 10)}pt` }">[12:00:00] [INFO] Minecraft 日志预览
[12:00:01] [WARN] 示例警告信息</pre>
            </PclCard>
            <PclCard v-if="!isInstance" title="日志上传">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">上传服务</span>
                <div class="pcl-form-row__control"><PclSelect :model-value="draftNumber('PastebinType', 3)" :disabled="busy" :options="[{ label: 'mclo.gs', value: 3 }, { label: '0x0.st', value: 0 }, { label: 'paste.gg', value: 2 }, { label: 'hastebin', value: 1 }]" @update:model-value="save('PastebinType', $event)" /></div>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">自定义服务地址</span>
                <div class="pcl-form-row__control"><PclInput :model-value="draftValue('PastebinCustomAPIBase', '')" placeholder="留空使用所选服务的默认地址" :disabled="busy" @update:model-value="drafts.PastebinCustomAPIBase = $event" /><PclButton size="sm" :disabled="busy" @click="savePasteUrl">保存</PclButton></div>
              </div>
              <PclHint theme="blue">自定义地址需兼容所选服务的 API。历史日志和实时控制台的「一键上传」共用此设置，上传前会显示目标地址并请求确认。</PclHint>
            </PclCard>
          </template>

          <template v-else-if="section === 'advanced-settings'">
            <PclCard title="资源文件校验">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">校验方式</span>
                <div class="pcl-form-row__control"><PclSelect :model-value="draftNumber('AssetVerificationMode', 1)" :options="assetVerificationOptions" @update:model-value="save('AssetVerificationMode', $event)" /></div>
              </div>
              <p class="pcl-muted">{{ assetVerificationDescriptions[draftNumber('AssetVerificationMode', 1)] }}</p>
              <div v-if="draftNumber('AssetVerificationMode', 1) === 2" class="pcl-form-row">
                <span class="pcl-form-row__label">缓存有效期</span>
                <div class="pcl-form-row__control"><PclNumberInput label="缓存有效期" :model-value="draftNumber('AssetCacheExpiryDays', 7)" :min="1" :max="365" unit="天" @update:model-value="save('AssetCacheExpiryDays', $event)" /></div>
              </div>
            </PclCard>
            <PclCard v-if="isInstance" title="实例完整性白名单">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">信任状态</span>
                <div class="pcl-form-row__control"><strong>{{ bool('AssetIntegrityTrusted') ? '已通过完整校验并信任' : '未加入白名单' }}</strong></div>
                <span class="pcl-form-row__hint">信任后会跳过重复的资源哈希计算，缺少文件仍会自动修复</span>
              </div>
              <div class="pcl-actions">
                <PclButton variant="highlight" icon="shield-check" :disabled="busy" @click="verifyAndTrust">完整校验并加入白名单</PclButton>
                <PclButton v-if="bool('AssetIntegrityTrusted')" variant="ghost" icon="x" :disabled="busy" @click="save('AssetIntegrityTrusted', false)">撤销白名单</PclButton>
              </div>
              <PclHint theme="yellow">只有完整校验成功后才会写入白名单。更换游戏版本或怀疑文件被修改时，可以撤销后重新校验。</PclHint>
            </PclCard>
            <PclCard title="兼容性与渲染">
              <div class="pcl-check-grid"><PclCheckbox :model-value="bool('OnlineFixes')" label="启用旧版本在线服务修复" @update:model-value="save('OnlineFixes', $event)" /><PclCheckbox :model-value="bool('UseNativeGLFW')" label="使用系统 GLFW" @update:model-value="save('UseNativeGLFW', $event)" /><PclCheckbox :model-value="bool('UseNativeOpenAL')" label="使用系统 OpenAL" @update:model-value="save('UseNativeOpenAL', $event)" /><PclCheckbox :model-value="bool('UseDiscreteGpu')" label="要求使用高性能显卡" @update:model-value="save('UseDiscreteGpu', $event)" /><PclCheckbox :model-value="bool('EnableFeralGamemode')" label="启用 Feral GameMode（Linux）" @update:model-value="save('EnableFeralGamemode', $event)" /><PclCheckbox :model-value="bool('EnableMangoHud')" label="启用 MangoHud（Linux）" @update:model-value="save('EnableMangoHud', $event)" /><PclCheckbox :model-value="bool('UseZink')" label="使用 Zink 渲染（Linux）" @update:model-value="save('UseZink', $event)" /></div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">GLFW 库文件</span>
                <div class="pcl-form-row__control"><PclInput :disabled="!bool('UseNativeGLFW')" :model-value="draftValue('CustomGLFWPath')" placeholder="留空使用系统库" @update:model-value="drafts.CustomGLFWPath = $event" @blur="save('CustomGLFWPath', drafts.CustomGLFWPath)" /><PclIconButton icon="folder-open" label="选择GLFW 库文件" :size="28" :disabled="busy || !bool('UseNativeGLFW')" @click="chooseFilePath('CustomGLFWPath', 'GLFW 库文件')" /></div>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">OpenAL 库文件</span>
                <div class="pcl-form-row__control"><PclInput :disabled="!bool('UseNativeOpenAL')" :model-value="draftValue('CustomOpenALPath')" placeholder="留空使用系统库" @update:model-value="drafts.CustomOpenALPath = $event" @blur="save('CustomOpenALPath', drafts.CustomOpenALPath)" /><PclIconButton icon="folder-open" label="选择OpenAL 库文件" :size="28" :disabled="busy || !bool('UseNativeOpenAL')" @click="chooseFilePath('CustomOpenALPath', 'OpenAL 库文件')" /></div>
              </div>
            </PclCard>
            <PclCard title="游戏时间">
              <div class="pcl-check-grid"><PclCheckbox :model-value="bool('ShowGameTime')" label="显示游戏时间" @update:model-value="save('ShowGameTime', $event)" /><PclCheckbox v-if="!isInstance" :model-value="bool('ShowGlobalGameTime')" label="显示总游戏时间" @update:model-value="save('ShowGlobalGameTime', $event)" /><PclCheckbox v-if="!isInstance" :model-value="bool('ShowGameTimeWithoutDays')" label="不显示天数" @update:model-value="save('ShowGameTimeWithoutDays', $event)" /><PclCheckbox :model-value="bool('RecordGameTime')" label="记录游戏时间" @update:model-value="save('RecordGameTime', $event)" /></div>
            </PclCard>
          </template>

          <template v-else-if="section === 'game'">
            <PclCard title="资源与整合包">
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
                    placeholder="输入目录路径"
                    @update:model-value="drafts.GlobalDataPacksPath = $event"
                    @blur="save('GlobalDataPacksPath', drafts.GlobalDataPacksPath)"
                  />
                  <PclIconButton icon="folder-open" label="选择全局数据包目录" :size="28" :disabled="busy" @click="chooseGlobalDataPacks" />
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
              <PclCard title="联机后端">
                <template #actions><PclButton size="sm" icon="refresh-cw" :disabled="onlineBusy" @click="loadOnlineBackends(true)">刷新</PclButton></template>
                <PclLoading v-if="onlineBusy" text="正在读取联机后端" inline />
                <div v-if="onlineBackends.length" class="pcl-tools-list">
                  <PclListItem v-for="backend in onlineBackends" :key="backend.id" :title="backend.name || backend.id" :info="`${backend.installed ? (backend.version || '已安装') : '未安装'} · ${backend.running ? '进程运行中' : '进程未启动'}`">
                    <template #icon><PclIcon name="bubbles" :size="16" /></template>
                  </PclListItem>
                </div>
                <PclHint v-else-if="!onlineBusy" theme="yellow">没有读取到联机后端。请确认启动器附带了 Terracotta 或 YukariConnect，并到“工具 → 联机”创建房间或加入房间。</PclHint>
                <PclHint theme="blue">联机控制、安装和房间操作使用与传统 C++ UI 相同的 multiplayer API，统一在“工具 → 联机”中管理；这里显示实时安装状态，避免保留不可用的 EasyTier 占位控件。</PclHint>
              </PclCard>
            </template>
          </template>

          <template v-else-if="section === 'cache'">
            <PclCard title="内建 Yggdrasil 缓存">
              <template #actions>
                <PclButton size="sm" icon="refresh-cw" :disabled="cacheBusy" @click="loadCache(true)">刷新</PclButton>
              </template>
              <PclLoading v-if="cacheBusy" text="正在读取缓存服务" inline />
              <PclHint v-if="cacheError" theme="red" closable @close="cacheError = ''">{{ cacheError }}</PclHint>
              <div v-if="cacheState.stats" class="pcl-about-grid">
                <span>服务状态</span><code>{{ cacheState.running ? '运行中' : '未运行' }}</code>
                <span>元数据缓存</span><code>{{ cacheState.enabled && cacheState.configured ? '已启用' : '未启用' }}</code>
                <span>账户快照缓存</span><code>{{ cacheState.accountCacheEnabled ? '已启用' : '未启用' }}</code>
                <span>缓存条目</span><code>{{ cacheState.stats.entries ?? 0 }}</code>
                <span>缓存大小</span><code>{{ formatBytes(cacheState.stats.bytes) }}</code>
                <span>命中 / 未命中</span><code>{{ cacheState.stats.hit ?? 0 }} / {{ cacheState.stats.miss ?? 0 }}</code>
                <span>元数据路径</span><code>{{ cacheState.metadataPath || '—' }}</code>
                <span>纹理路径</span><code>{{ cacheState.texturesPath || '—' }}</code>
              </div>
              <PclHint v-else-if="!cacheBusy" theme="blue">缓存服务尚未启动。点击刷新后会读取并显示内建 Yggdrasil 服务状态。</PclHint>
              <div class="pcl-actions">
                <PclButton variant="highlight" icon="trash-2" :disabled="cacheBusy || !cacheState.running" @click="purgeYggdrasilCache">清理 Yggdrasil 缓存</PclButton>
                <PclButton icon="trash-2" :disabled="busy" @click="clearMetadataCache">清理启动器元数据缓存</PclButton>
              </div>
            </PclCard>

            <PclCard title="缓存路由">
              <div v-if="cacheRoutes.length" class="pcl-tools-list">
                <PclListItem v-for="route in cacheRoutes" :key="route.id" :title="route.name || route.id" :info="`${route.listen_prefix || route.listenPrefix || '—'} · ${route.enabled ? '启用' : '停用'}`">
                  <template #icon><PclIcon name="layers" :size="16" /></template>
                  <template #actions><span class="pcl-muted">TTL {{ route.ttl_seconds ?? route.ttlSeconds ?? 0 }} 秒</span></template>
                </PclListItem>
              </div>
              <div v-else class="pcl-empty pcl-empty--compact">当前没有已配置的缓存路由。启用“本地服务缓存”后，启动器会自动配置元数据和纹理路由。</div>
            </PclCard>

            <PclCard title="Prism 元数据 HTTP 缓存">
              <div class="pcl-about-grid">
                <span>缓存条目</span><code>{{ cacheState.http?.entries ?? '—' }}</code>
                <span>缓存大小</span><code>{{ cacheState.http ? formatBytes(cacheState.http.bytes) : '—' }}</code>
                <span>缓存分组</span><code>{{ cacheState.http?.bases ?? '—' }}</code>
                <span>索引文件</span><code>{{ cacheState.http?.indexPath || '—' }}</code>
              </div>
              <PclHint theme="blue">这是启动器用于 Prism 元数据、资源目录和新闻等 HTTP 请求的本地缓存。元数据服务地址可在“下载与传输 → 自定义镜像 → 版本元数据地址”替换，缓存会按新的地址重新建立。</PclHint>
            </PclCard>
          </template>

          <template v-else-if="section === 'java'">
            <PclJavaPanel :scope="scope" :settings="settings" :busy="busy" :execute="execute" @changed="emit('changed')" />
          </template>

          <template v-else-if="section === 'ui'">
            <PclAppearancePanel />
            <PclCard title="C++ 界面与实例列表" collapsible>
              <PclHint theme="blue">这些选项用于 C++ 界面；PCL 风格的外观在上方设置。</PclHint>
              <div class="pcl-check-grid"><PclCheckbox :model-value="bool('MenuBarInsteadOfToolBar')" label="使用菜单栏代替工具栏" @update:model-value="save('MenuBarInsteadOfToolBar', $event)" /><PclCheckbox :model-value="bool('UseNewUI')" label="C++ 界面使用新版布局（重启生效）" @update:model-value="save('UseNewUI', $event)" /><PclCheckbox :model-value="bool('ShowNewsBar')" label="显示新闻栏" @update:model-value="save('ShowNewsBar', $event)" /><PclCheckbox :model-value="bool('StatusBarVisible')" label="显示状态栏" @update:model-value="save('StatusBarVisible', $event)" /><PclCheckbox :model-value="bool('ShowServerPreview')" label="显示服务器预览" @update:model-value="save('ShowServerPreview', $event)" /><PclCheckbox :model-value="bool('ServerPreviewDynamicWidth')" label="服务器预览自适应宽度" @update:model-value="save('ServerPreviewDynamicWidth', $event)" /><PclCheckbox :model-value="bool('ShowTerracottaToolBar')" label="显示 Terracotta 工具栏" @update:model-value="save('ShowTerracottaToolBar', $event)" /><PclCheckbox :model-value="bool('ShowYukariConnectToolBar')" label="显示 YukariConnect 工具栏" @update:model-value="save('ShowYukariConnectToolBar', $event)" /><PclCheckbox :model-value="bool('ShowTerracottaStatusBar')" label="显示 Terracotta 状态栏" @update:model-value="save('ShowTerracottaStatusBar', $event)" /><PclCheckbox :model-value="bool('ShowYukariConnectStatusBar')" label="显示 YukariConnect 状态栏" @update:model-value="save('ShowYukariConnectStatusBar', $event)" /></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">实例排序</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('InstSortMode', 'Name')" :options="[{ label: '按名称', value: 'Name' }, { label: '按最近启动', value: 'LastLaunch' }]" @update:model-value="save('InstSortMode', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">实例重命名</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('InstRenamingMode', 'AskEverytime')" :options="[{ label: '每次询问', value: 'AskEverytime' }, { label: '始终同步文件夹名', value: 'PhysicalDir' }, { label: '只改显示名称', value: 'MetadataOnly' }]" @update:model-value="save('InstRenamingMode', $event)" /></div></div>
            </PclCard>

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
                  <PclButton variant="highlight" icon="upload" @click="openUrl(projectUrl + '/pulls')">提交 Pull Request</PclButton>
                  <PclButton icon="book-marked" @click="openUrl(docsUrl)">查看项目文档</PclButton>
                </div>
              </div>
            </div>
          </template>

          <template v-else-if="section === 'misc'">
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
                <PclCheckbox :model-value="bool('PclDownloadFallbackToQt')" label="专用 .NET 下载失败时回退 Qt" @update:model-value="save('PclDownloadFallbackToQt', $event)" />
                <PclCheckbox :model-value="bool('Aria2FallbackToQt')" label="Aria2 失败时回退 Qt" @update:model-value="save('Aria2FallbackToQt', $event)" />
                <PclCheckbox :model-value="bool('DownloadsDirWatchRecursive')" label="递归监控下载目录" @update:model-value="save('DownloadsDirWatchRecursive', $event)" />
              </div>
            </PclCard>
          </template>

          <template v-else-if="section === 'about'">
            <PclCard :title="t('about.title')">
              <div class="pcl-about-head">
                <div class="pcl-about-logo"><img :src="lunaLogo" alt="Luna Launcher" /></div>
                <div>
                  <div class="pcl-about-name">{{ t('brand.name') }} <span>{{ t('brand.neoUi') }}</span></div>
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
                <PclButton icon="external-link" @click="openUrl(projectUrl)">{{ t('about.projectHome') }}</PclButton>
                <PclButton icon="book-marked" @click="openUrl(docsUrl)">{{ t('about.docs') }}</PclButton>
              </div>
            </PclCard>

            <PclCard :title="t('about.services')">
              <p class="pcl-muted">{{ t('about.servicesDescription') }}</p>
              <div class="pcl-actions">
                <PclButton icon="external-link" @click="openUrl(bmclApiUrl)">{{ t('about.bmclapi') }}</PclButton>
                <PclButton icon="external-link" @click="openUrl(mcimUrl)">{{ t('about.mcim') }}</PclButton>
              </div>
            </PclCard>

            <PclCard :title="t('about.credits')">
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

            <PclCard :title="t('about.contributors')">
              <div class="pcl-credit-tiles">
                <span v-for="name in contributors" :key="name">{{ name }}</span>
              </div>
              <p class="pcl-muted pcl-credit-note">以上为 Luna Neo UI 与上游项目的公开贡献者代表；完整名单见各项目仓库。</p>
            </PclCard>

            <PclCard :title="t('about.licenses')">
              <p class="pcl-muted">{{ t('about.licenseText') }}</p>
              <p class="pcl-muted">{{ t('about.pclIconCredit') }}</p>
              <div class="pcl-actions">
                <PclButton icon="file-box" @click="openUrl(licenseUrl)">Luna Launcher GPL-3.0</PclButton>
                <PclButton icon="file-box" @click="openUrl('https://github.com/PrismLauncher/PrismLauncher/blob/develop/LICENSE')">PrismLauncher 许可证</PclButton>
                <PclButton icon="file-box" @click="openUrl('https://lucide.dev/license')">{{ t('about.lucideLicense') }}</PclButton>
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
              <PclButton variant="highlight" icon="message-circle" @click="openUrl(projectUrl + '/issues')">前往反馈</PclButton>
            </PclCard>
            <PclCard title="反馈进度">
              <p class="pcl-muted">Luna Neo UI 的反馈会直接进入项目问题列表，提交时请附上复现步骤和相关日志。</p>
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
import { useI18n } from 'vue-i18n';
import { launcher } from '../../../boot/launcher-api';
import lunaLogo from '../../../assets/luna-launcher.png';
import { localeFromLauncherLanguage, setNeoLocale } from '../../../i18n';
import PclPageNav from '../../components/PclPageNav.vue';
import PclCard from '../../components/PclCard.vue';
import PclButton from '../../components/PclButton.vue';
import PclIconButton from '../../components/PclIconButton.vue';
import { pickPclDirectory, pickPclFile } from '../../composables/usePclFileDialog';
import PclHint from '../../components/PclHint.vue';
import PclInput from '../../components/PclInput.vue';
import PclNumberInput from '../../components/PclNumberInput.vue';
import PclSecretInput from '../../components/PclSecretInput.vue';
import PclSelect from '../../components/PclSelect.vue';
import PclCheckbox from '../../components/PclCheckbox.vue';
import PclRadio from '../../components/PclRadio.vue';
import PclSlider from '../../components/PclSlider.vue';
import PclLoading from '../../components/PclLoading.vue';
import PclListItem from '../../components/PclListItem.vue';
import PclIcon from '../../components/PclIcon.vue';
import PclAccountPage from '../accounts/PclAccountPage.vue';
import PclJavaPanel from './PclJavaPanel.vue';
import PclAppearancePanel from './PclAppearancePanel.vue';
import type { PclNavGroup } from '../../types';
import { hiddenPclFeatures, isPclFeatureHidden } from '../../composables/usePclFeatures';

type Execute = (operation: string, parameters?: Record<string, unknown>) => Promise<any>;
type ApiSetting = {
  key: string;
  value: unknown;
  default?: unknown;
  type?: string;
  category?: string;
  surface?: string;
  sensitive?: boolean;
  redacted?: boolean;
  isDefault?: boolean;
};

const props = defineProps<{ settings: ApiSetting[]; busy: boolean; scope: Record<string, unknown>; execute: Execute; accounts?: any[]; embedded?: boolean; initialSection?: string }>();
const emit = defineEmits<{ changed: [] }>();
const { t } = useI18n();

const section = ref('launch');
const error = ref(''), notice = ref('');
const drafts = reactive<Record<string, any>>({});
const languages = ref<any[]>([]), languageId = ref(''), useSystemLocale = ref(false), languagesLoading = ref(false);
const runtimeInfo = ref<any>({});
const updateState = ref<any>({}), updateCheck = ref<any>({}), updateChecked = ref(false), updateChecking = ref(false);
const proxy = reactive({ type: 'None', host: '', port: 7890, username: '', password: '' });
const proxyTestUrl = ref('https://api.minecraftservices.com/'), proxyTestResult = ref('');
const cacheState = ref<any>({}), cacheRoutes = ref<any[]>([]), cacheBusy = ref(false), cacheError = ref('');
const onlineBackends = ref<any[]>([]), onlineBusy = ref(false);

const isInstance = computed(() => props.scope.scope === 'instance');
watch(() => props.initialSection, value => { if (value && value !== section.value) section.value = value; }, { immediate: true });
const launcherGroups: PclNavGroup[] = [
  { label: '游戏', items: [
    { id: 'launch', label: '启动', icon: 'rocket', refresh: true },
    { id: 'java', label: 'Java', icon: 'coffee', refresh: true },
    { id: 'game', label: '资源与整合包', icon: 'book-marked', refresh: true },
    { id: 'advanced', label: '高级启动', icon: 'tool-case' },
  ] },
  { label: '工具', items: [
    { id: 'network', label: '联机', icon: 'bubbles', refresh: true },
  ] },
  { label: '启动器', items: [
    { id: 'accounts', label: '账户管理', icon: 'user', refresh: true },
    { id: 'ui', label: '个性化', icon: 'palette', refresh: true },
    { id: 'language', label: '语言', icon: 'earth', refresh: true },
    { id: 'misc', label: '网络代理', icon: 'monitor-cog', refresh: true },
    { id: 'download-settings', label: '下载与传输', icon: 'download', refresh: true },
    { id: 'cache', label: '缓存管理', icon: 'layers', refresh: true },
    { id: 'paths', label: '路径与工具', icon: 'folder-cog', refresh: true },
    { id: 'console', label: '控制台', icon: 'terminal', refresh: true },
    { id: 'advanced-settings', label: '高级兼容性', icon: 'settings', refresh: true },
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
    { id: 'advanced-settings', label: '兼容性与资源', icon: 'settings', refresh: true },
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
const downloadBackendOptions = [
  { label: 'Qt 原生下载', value: 0 },
  { label: 'Aria2', value: 1 },
  { label: 'Luna .NET 下载器（NativeAOT）', value: 2 },
];
const modApiMirrorOptions = [
  { label: '官方源', value: 0 },
  { label: 'MCIM 镜像', value: 1 },
  { label: '自定义镜像', value: 2 },
];
const proxyTypeOptions = [
  { label: '不使用代理', value: 'None' },
  { label: '跟随系统', value: 'Default' },
  { label: 'HTTP 代理', value: 'HTTP' },
  { label: 'SOCKS5 代理', value: 'SOCKS5' },
];
const ariaProxyDisabled = computed(() => busyOrFollowingProxy());
function formatBytes(value: unknown): string {
  const bytes = Number(value || 0);
  if (!Number.isFinite(bytes) || bytes < 1024) return `${Math.max(0, Math.round(bytes))} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
  if (bytes < 1024 * 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(1)} MiB`;
  return `${(bytes / 1024 / 1024 / 1024).toFixed(2)} GiB`;
}
function busyOrFollowingProxy() {
  return props.busy || bool('Aria2UseLauncherProxy') || draftValue('Aria2ProxyType', 'None') === 'None';
}
const consoleColorOptions = computed(() => [...new Set([
  'Builtin Dark', 'Builtin Light', 'Builtin Solarized Dark', 'Builtin Solarized Light',
  'Builtin Tango Dark', 'Builtin Tango Light', draftValue('ConsoleColorScheme', 'Builtin Dark'),
])].map(value => ({ label: value.replace('Builtin ', '内置 · '), value })));
const consoleFontOptions = computed(() => [...new Set([
  draftValue('ConsoleFont', 'Courier New'), 'Cascadia Mono', 'Consolas', 'Courier New', 'Menlo', 'Monospace',
])].filter(Boolean).map(value => ({ label: value, value })));
const assetVerificationOptions = [
  { label: '每次完整校验', value: 0 }, { label: '检查文件是否缺失或大小异常（推荐）', value: 1 },
  { label: '使用有期限的缓存', value: 2 }, { label: '跳过校验', value: 3 },
];
const assetVerificationDescriptions = [
  '启动时校验 SHA-1，需要时联网修复资源。', '复用有效索引，只下载缺失或大小异常的资源；完整缓存可离线使用。',
  '在有效期内使用缓存，到期后刷新索引。', '直接使用本地索引，适合已经确认资源文件完整的情况。',
];
const languageOptions = computed(() => languages.value.map(item => ({
  label: item.name || item.id,
  value: item.id,
})));

const settingsMap = computed(() => new Map(props.settings.map(setting => [setting.key, setting])));
const pathSettings = [
  { key: 'InstanceDir', label: '实例文件夹', placeholder: '实例的默认存放位置' },
  { key: 'CentralModsDir', label: '全局模组文件夹' },
  { key: 'IconsDir', label: '图标文件夹' },
  { key: 'DownloadsDir', label: '下载文件夹' },
  { key: 'SkinsDir', label: '皮肤文件夹' },
  { key: 'JavaDir', label: 'Java 文件夹' },
  { key: 'FTBAppInstancesPath', label: 'FTB 实例文件夹' },
  { key: 'JProfilerPath', label: 'JProfiler 文件夹' },
  { key: 'MCEditPath', label: 'MCEdit 文件夹' },
] as const;
const toolPathSettings = [
  { key: 'Aria2ExecutablePath', label: 'aria2c 可执行文件' },
  { key: 'CurseForgeExternalToolPath', label: 'CurseForge 外部工具' },
  { key: 'JVisualVMPath', label: 'VisualVM 可执行文件' },
  { key: 'JsonEditor', label: 'JSON 编辑器' },
] as const;
function val(key: string, fallback: any = ''): any {
  const setting = settingsMap.value.get(key);
  return setting ? setting.value : fallback;
}
function num(key: string, fallback = 0): number {
  const value = Number(val(key, fallback));
  return Number.isFinite(value) ? value : fallback;
}
function bool(key: string): boolean {
  const value = drafts[key] ?? val(key, false);
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
  { name: 'PrismLauncher', info: '实例格式与启动器核心兼容性来源', url: 'https://github.com/PrismLauncher/PrismLauncher' },
  { name: 'PCL-CE 图标资源', info: '仅使用整理后的 Lucide SVG 图标资源，不使用其页面或业务代码', url: 'https://github.com/PCL-Community/PCL-CE' },
  { name: 'BMCLAPI', info: 'Minecraft 下载镜像 API', url: 'https://bmclapidoc.bangbang93.com/' },
  { name: 'MCIM', info: 'Modrinth 与 CurseForge 社区镜像', url: 'https://mod.mcimirror.top/docs' },
  { name: 'Lucide', info: '线性 SVG 图标集', url: 'https://lucide.dev' },
  { name: 'Qt', info: '启动器与元数据工具链', url: 'https://www.qt.io' },
];
const contributors = ['Luna Neo UI', 'PrismLauncher', 'BMCLAPI', 'MCIM', 'Lucide', 'Qt', 'MCBBS 社区', 'Bangbang93', 'Pysio'];
const projectUrl = 'https://github.com/AndreaFrederica/LunaLauncher';
const licenseUrl = `${projectUrl}/blob/main/COPYING.md`;
const docsUrl = 'https://lunalauncher.sirrus.cc';
const bmclApiUrl = 'https://bmclapidoc.bangbang93.com/';
const mcimUrl = 'https://mod.mcimirror.top/docs';
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
const mirrorType = ref(0);

watch(() => props.settings, (items, previous = []) => {
  const old = new Map(previous.map(item => [item.key, item]));
  for (const setting of items) {
    if (setting.redacted) { delete drafts[setting.key]; continue; }
    if (drafts[setting.key] === undefined || drafts[setting.key] === old.get(setting.key)?.value)
      drafts[setting.key] = setting.value;
  }
  mirrorType.value = num('DownloadMirrorType', 0);
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
  if (value === 'cache' && !isInstance.value) void loadCache();
  if (value === 'network' && !isInstance.value) void loadOnlineBackends();
});
function setWindowMode(value: string | number) {
  windowMode.value = value as typeof windowMode.value;
  void save('LaunchMaximized', value === 'maximized');
  if (value === 'default') {
    void save('MinecraftWinWidth', 854);
    void save('MinecraftWinHeight', 480);
  }
}

function message(value: unknown): string {
  return value instanceof Error ? value.message : String(value);
}
function saveMirrorType(value: string | number) {
  mirrorType.value = Number(value);
  void save('DownloadMirrorType', mirrorType.value);
}
async function savePasteUrl() {
  const value = String(draftValue('PastebinCustomAPIBase', '')).trim().replace(/\/+$/, '');
  if (value) {
    try {
      if (!['http:', 'https:'].includes(new URL(value).protocol)) throw new Error();
    } catch { error.value = '请输入有效的 HTTP 或 HTTPS 服务地址，或留空使用默认地址。'; return; }
  }
  await save('PastebinCustomAPIBase', value);
}
async function chooseGlobalDataPacks() {
  const selected = await pickPclDirectory({ title: '选择全局数据包目录' });
  if (!selected) return;
  drafts.GlobalDataPacksPath = selected;
  save('GlobalDataPacksPath', selected);
}
async function chooseDirectory(key: string, label: string) {
  const selected = await pickPclDirectory({ title: `选择${label}` });
  if (!selected) return;
  drafts[key] = selected;
  await save(key, selected);
}
async function chooseFilePath(key: string, label: string) {
  const selected = await pickPclFile({ title: `选择${label}` });
  if (!selected) return;
  drafts[key] = selected;
  await save(key, selected);
}
const overrideGroups: Record<string, string[]> = {
  OverrideNativeWorkarounds: ['UseNativeGLFW', 'UseNativeOpenAL', 'CustomGLFWPath', 'CustomOpenALPath'],
  OverridePerformance: ['EnableFeralGamemode', 'EnableMangoHud', 'UseDiscreteGpu', 'UseZink'],
  OverrideLegacySettings: ['OnlineFixes'], OverrideGameTime: ['ShowGameTime', 'RecordGameTime'],
  OverrideAssetVerification: ['AssetVerificationMode', 'AssetCacheExpiryDays'],
  OverrideConsole: ['ShowConsole', 'AutoCloseConsole', 'ShowConsoleOnError', 'LogPrePostOutput'],
  OverrideWindow: ['LaunchMaximized', 'MinecraftWinWidth', 'MinecraftWinHeight'],
  OverrideMemory: ['MinMemAlloc', 'MaxMemAlloc', 'PermGen'],
  OverrideMiscellaneous: ['CloseAfterLaunch', 'QuitAfterGameStop'],
  OverrideCommands: ['PreLaunchCommand', 'WrapperCommand', 'PostExitCommand'],
  OverrideJavaArgs: ['JvmArgs'],
};
const sectionOverrideGroups = computed(() => {
  const sections: Record<string, { key: string; label: string }[]> = {
    launch: [{ key: 'OverrideWindow', label: '独立窗口设置' }, { key: 'OverrideConsole', label: '独立控制台行为' }, { key: 'OverrideMiscellaneous', label: '独立启动器行为' }],
    memory: [{ key: 'OverrideMemory', label: '独立内存配置' }],
    advanced: [{ key: 'OverrideCommands', label: '独立启动命令' }, { key: 'OverrideJavaArgs', label: '独立 JVM 参数' }],
    'advanced-settings': [{ key: 'OverrideAssetVerification', label: '独立资源校验' }, { key: 'OverrideNativeWorkarounds', label: '独立原生库配置' },
      { key: 'OverridePerformance', label: '独立性能选项' }, { key: 'OverrideLegacySettings', label: '独立旧版兼容选项' }, { key: 'OverrideGameTime', label: '独立游戏时间设置' }],
  };
  return (sections[section.value] || []).filter(item => settingsMap.value.has(item.key));
});
let saveQueue: Promise<unknown> = Promise.resolve();
function save(key: string, value: unknown): Promise<boolean> {
  const operation = saveQueue.then(() => persistSetting(key, value));
  saveQueue = operation.catch(() => {});
  return operation;
}
async function persistSetting(key: string, value: unknown): Promise<boolean> {
  const setting = settingsMap.value.get(key);
  if (value === undefined || !setting) return false;
  if (setting.redacted && value === '***') return false;
  error.value = '';
  const normalized = setting.type === 'QString' && typeof value === 'boolean' ? String(value) : value;
  try {
    if (isInstance.value) {
      const gate = Object.entries(overrideGroups).find(([, keys]) => keys.includes(key))?.[0];
      if (gate && !bool(gate)) {
        await props.execute('settings.set', { ...props.scope, key: gate, value: true });
        drafts[gate] = true;
      }
    }
    await props.execute('settings.set', { ...props.scope, key, value: normalized });
    if (!setting.redacted) drafts[key] = normalized;
    notice.value = '设置已保存';
    window.setTimeout(() => { if (notice.value === '设置已保存') notice.value = ''; }, 1800);
    emit('changed');
    return true;
  } catch (exception) {
    error.value = message(exception);
    return false;
  }
}
function secretConfigured(key: string): boolean {
  const setting = settingsMap.value.get(key);
  return setting?.redacted ? !setting.isDefault : Boolean(setting?.value);
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
  else if (id === 'cache' && !isInstance.value) void loadCache(true);
  else if (id === 'network' && !isInstance.value) void loadOnlineBackends(true);
  else emit('changed');
}

async function loadOnlineBackends(force = false) {
  if (onlineBusy.value && !force) return;
  onlineBusy.value = true;
  try {
    const result = await props.execute('multiplayer.backends');
    onlineBackends.value = Array.isArray(result) ? result : (result?.backends || []);
  } catch {
    onlineBackends.value = [];
  } finally {
    onlineBusy.value = false;
  }
}
async function verifyAndTrust() {
  if (!isInstance.value || !props.scope.instance) return;
  error.value = '';
  try {
    await props.execute('instance.verify', { instance: props.scope.instance, trust: true });
    drafts.AssetIntegrityTrusted = true;
    notice.value = '完整校验通过，实例已加入白名单';
    emit('changed');
  } catch (exception) {
    error.value = message(exception);
  }
}

async function loadCache(force = false) {
  if (cacheBusy.value && !force) return;
  cacheBusy.value = true;
  cacheError.value = '';
  try {
    const [yggdrasilResult, httpResult] = await Promise.allSettled([
      props.execute('yggdrasil.cache.status'),
      props.execute('launcher.cache.status'),
    ]);
    const yggdrasil = yggdrasilResult.status === 'fulfilled' ? yggdrasilResult.value : {};
    const http = httpResult.status === 'fulfilled' ? httpResult.value : {};
    cacheState.value = { ...(yggdrasil || {}), http: http || {} };
    cacheRoutes.value = Array.isArray(yggdrasil?.routes) ? yggdrasil.routes : [];
    if (yggdrasilResult.status === 'rejected' && httpResult.status === 'rejected')
      cacheError.value = message(yggdrasilResult.reason);
    else if (yggdrasilResult.status === 'rejected')
      cacheError.value = '内建 Yggdrasil 服务暂不可用，仍可管理启动器 HTTP 缓存。';
  } finally {
    cacheBusy.value = false;
  }
}
async function purgeYggdrasilCache() {
  if (!window.confirm('确定清理内建 Yggdrasil 的全部元数据和纹理缓存吗？')) return;
  cacheBusy.value = true;
  try {
    await props.execute('yggdrasil.cache.purge', { confirm: true });
    notice.value = 'Yggdrasil 缓存已清理';
    await loadCache(true);
  } catch (exception) {
    cacheError.value = message(exception);
  } finally {
    cacheBusy.value = false;
  }
}
async function clearMetadataCache() {
  if (!window.confirm('确定清理启动器 HTTP 元数据缓存吗？')) return;
  try {
    await props.execute('launcher.cache.clear', { confirm: true });
    notice.value = '启动器元数据缓存已清理';
    await loadCache(true);
  } catch (exception) {
    cacheError.value = message(exception);
  }
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
    if (languageId.value) setNeoLocale(localeFromLauncherLanguage(languageId.value));
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
    setNeoLocale(localeFromLauncherLanguage(languageId.value));
    notice.value = '语言已更新';
    emit('changed');
  } catch (exception) {
    error.value = message(exception);
  }
}
async function setSystemLocale(value: boolean) {
  useSystemLocale.value = value;
  if (value && typeof navigator !== 'undefined') setNeoLocale(navigator.language);
  else if (languageId.value) setNeoLocale(localeFromLauncherLanguage(languageId.value));
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

.pcl-console-preview { padding: 12px; overflow: auto; border-radius: 4px; background: var(--pcl-c8); color: var(--pcl-text); }

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

.pcl-about-logo img {
  width: 100%;
  height: 100%;
  object-fit: contain;
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
