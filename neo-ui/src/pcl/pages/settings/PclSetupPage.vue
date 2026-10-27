<template>
  <section class="pcl-page pcl-setup-page" :class="{ 'is-embedded': embedded }">
    <PclPageNav v-if="!embedded" v-model="section" :groups="groups" @refresh="refreshSection" />

    <main class="pcl-page-main">
      <div class="pcl-page-view pcl-setup-stack">
          <PclHint v-if="error" theme="red" closable @close="error = ''">{{ error }}</PclHint>
          <PclHint v-if="notice" theme="blue" closable @close="notice = ''">{{ notice }}</PclHint>

          <PclCard v-if="isInstance && sectionOverrideGroups.length" :title="t('pcl.auto2.kc5133a8b25')">
            <p class="pcl-muted">{{ t('pcl.auto2.k068fc09151') }}</p>
            <div class="pcl-check-grid">
              <PclCheckbox v-for="item in sectionOverrideGroups" :key="item.key" :model-value="bool(item.key)"
                :label="item.label" @update:model-value="save(item.key, $event)" />
            </div>
          </PclCard>

          <div v-show="section === 'accounts'" class="pcl-settings-section">
            <PclAccountPage :accounts="accounts || []" :busy="busy" :execute="execute" embedded @changed="emit('changed')" />
          </div>

          <div v-show="section === 'launch'" class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.k6c4942326d')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k2badbf857a') }}</span>
                <div class="pcl-form-row__control">
                  <PclSelect :model-value="'isolated'" :options="[{ label: t('pcl.auto2.ka16174eb79'), value: 'isolated' }]" disabled />
                </div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k8235323562') }}</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k4ebf21d7b3') }}</span>
                <div class="pcl-form-row__control">
                  <PclInput :model-value="String(val('WindowTitle', t('pcl.auto2.k8be3944d58')))" disabled />
                </div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k1c6d2e1081') }}</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k95b22c4d07') }}</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="bool('CloseAfterLaunch') ? 'close' : 'keep'"
                    :options="[{ label: t('pcl.auto2.k46f719b4c5'), value: 'keep' }, { label: t('pcl.auto2.k80c80f7e28'), value: 'close' }]"
                    @update:model-value="save('CloseAfterLaunch', $event === 'close')"
                  />
                </div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k387efacc97') }}</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k86a6775402') }}</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="bool('QuitAfterGameStop') ? 'quit' : 'keep'"
                    :options="[{ label: t('pcl.auto2.k5b096363ee'), value: 'keep' }, { label: t('pcl.auto2.kf7e6a52e54'), value: 'quit' }]"
                    @update:model-value="save('QuitAfterGameStop', $event === 'quit')"
                  />
                </div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k4f1fb55573') }}</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.ka599946fe1') }}</span>
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
                <span class="pcl-form-row__hint">{{ windowMode === 'maximized' ? t('pcl.auto2.k84e78ef1da') : t('pcl.auto2.k3edaf9c3ff') }}</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k77926b126a') }}</span>
                <div class="pcl-form-row__control pcl-check-row">
                  <PclCheckbox :model-value="bool('ShowConsole')" :label="t('pcl.auto2.kd6dca20f52')" @update:model-value="save('ShowConsole', $event)" />
                  <PclCheckbox :model-value="bool('AutoCloseConsole')" :label="t('pcl.auto2.kf7ef0f5cdc')" @update:model-value="save('AutoCloseConsole', $event)" />
                  <PclCheckbox :model-value="bool('ShowConsoleOnError')" :label="t('pcl.auto2.k522faf5a0b')" @update:model-value="save('ShowConsoleOnError', $event)" />
                </div>
              </div>
            </PclCard>

          </div>

          <div v-show="section === 'memory'" class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.k7d6742783a')">
              <PclHint v-if="maxMem < 2048" theme="yellow">{{ t('pcl.auto2.kb9999f751b') }}</PclHint>
              <div class="pcl-choice-row">
                <PclRadio :model-value="memoryMode" value="auto" :label="t('pcl.auto2.k716e01ff80')" @update:model-value="onMemoryMode" />
                <PclRadio :model-value="memoryMode" value="custom" :label="t('pcl.auto2.kc493338e8c')" @update:model-value="onMemoryMode" />
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k5a65c23c82') }}</span>
              </div>

              <div class="pcl-memory-live-head">
                <span>{{ t('pcl.auto2.ke2dadf543a') }}</span>
                <span class="pcl-memory-live-right">{{ t('pcl.auto2.kec461a8f29') }}<span class="pcl-memory-live-dot" />{{ t('pcl.auto2.k06df5e4583') }}</span>
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
                <span><b>{{ t('pcl.auto2.k97ebe06e74') }}</b>{{ memoryUsedPercent.toFixed(1) }}%</span>
                <span><b>{{ t('pcl.auto2.k31793b6729') }}</b>{{ formatMib(draftNumber('MinMemAlloc', 512)) }}</span>
                <span><b>{{ t('pcl.auto2.ked690ebb15') }}</b>{{ formatMib(draftNumber('MaxMemAlloc', 4096)) }}</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kccb8f3efaa') }}</span>
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
                <span class="pcl-form-row__hint">{{ t('ui.memoryUnit') }}</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k1f4bebe9b9') }}</span>
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
                <span class="pcl-form-row__hint">{{ t('ui.memoryUnit') }}</span>
              </div>
            </PclCard>

          </div>

          <div v-show="section === 'advanced'" class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.k86e9bcfc62')">
              <div class="pcl-form-row pcl-form-row--top">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kb001801f13') }}</span>
                <div class="pcl-form-row__control pcl-form-row__control--column">
                  <PclInput
                    multiline
                    :rows="4"
                    :model-value="draftValue('JvmArgs', '')"
                    :placeholder="t('pcl.auto2.k3496cb95b0')"
                    @update:model-value="drafts.JvmArgs = $event"
                    @blur="save('JvmArgs', drafts.JvmArgs)"
                  />
                </div>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k7e026da625') }}</span>
                <div class="pcl-form-row__control">
                  <PclInput
                    :model-value="draftValue('PreLaunchCommand', '')"
                    :placeholder="t('pcl.auto2.k7296ad4897')"
                    @update:model-value="drafts.PreLaunchCommand = $event"
                    @blur="save('PreLaunchCommand', drafts.PreLaunchCommand)"
                  />
                </div>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k49a71399fa') }}</span>
                <div class="pcl-form-row__control">
                  <PclInput
                    :model-value="draftValue('WrapperCommand', '')"
                    :placeholder="t('pcl.auto2.kc5c2ba325b')"
                    @update:model-value="drafts.WrapperCommand = $event"
                    @blur="save('WrapperCommand', drafts.WrapperCommand)"
                  />
                </div>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kfa4f7c58eb') }}</span>
                <div class="pcl-form-row__control">
                  <PclInput
                    :model-value="draftValue('PostExitCommand', '')"
                    :placeholder="t('pcl.auto2.kc80ce0c4e5')"
                    @update:model-value="drafts.PostExitCommand = $event"
                    @blur="save('PostExitCommand', drafts.PostExitCommand)"
                  />
                </div>
              </div>

              <div class="pcl-check-grid">
                <PclCheckbox :model-value="bool('UseNativeGLFW')" :label="t('pcl.auto2.kb6565ef62e')" @update:model-value="save('UseNativeGLFW', $event)" />
                <PclCheckbox :model-value="bool('UseNativeOpenAL')" :label="t('pcl.auto2.k7377d6632f')" @update:model-value="save('UseNativeOpenAL', $event)" />
                <PclCheckbox :model-value="bool('UseDiscreteGpu')" :label="t('pcl.auto2.k3ddf18f61b')" @update:model-value="save('UseDiscreteGpu', $event)" />
                <PclCheckbox :model-value="bool('OnlineFixes')" :label="t('pcl.auto2.k29eb9b95d0')" @update:model-value="save('OnlineFixes', $event)" />
                <PclCheckbox :model-value="bool('ShowGameTime')" :label="t('pcl.auto2.kff9e4486e9')" @update:model-value="save('ShowGameTime', $event)" />
                <PclCheckbox :model-value="bool('RecordGameTime')" :label="t('pcl.auto2.k19241e920e')" @update:model-value="save('RecordGameTime', $event)" />
              </div>

              <PclHint theme="blue" class="pcl-advanced-hint">{{ t('pcl.auto2.k49395925fa') }}</PclHint>
            </PclCard>
          </div>

          <div v-show="section === 'download-settings'" class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.k6ebe1abb5d')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k1165baa252') }}</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="num('DownloadBackend', 2)"
                    :options="downloadBackendOptions"
                    @update:model-value="save('DownloadBackend', $event)"
                  />
                </div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k2a2e0161b1') }}</span>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k64327d356b') }}</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="mirrorType"
                    :options="mirrorOptions"
                    @update:model-value="saveMirrorType($event)"
                  />
                </div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k01b7899496') }}</span>
              </div>

              <template v-if="mirrorType === 2">
                <PclHint theme="blue" class="pcl-advanced-hint">{{ t('pcl.auto2.kccbd1ffb95') }}</PclHint>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">{{ t('pcl.auto2.kdf4e39f071') }}</span>
                  <div class="pcl-form-row__control">
                    <PclInput
                      :model-value="draftValue('MetaURLOverride', '')"
                      :placeholder="t('pcl.auto2.k1cec1f1b79')"
                      @update:model-value="drafts.MetaURLOverride = $event"
                      @blur="save('MetaURLOverride', drafts.MetaURLOverride)"
                    />
                  </div>
                </div>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">{{ t('pcl.auto2.k2fa65ccaca') }}</span>
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
                  <span class="pcl-form-row__label">{{ t('pcl.auto2.kc0ad229574') }}</span>
                  <div class="pcl-form-row__control">
                    <PclInput
                      :model-value="draftValue('LibrariesURL', '')"
                      :placeholder="t('pcl.auto2.k041dea94f8')"
                      @update:model-value="drafts.LibrariesURL = $event"
                      @blur="save('LibrariesURL', drafts.LibrariesURL)"
                    />
                  </div>
                </div>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">{{ t('pcl.auto2.k0e1fb9c698') }}</span>
                  <div class="pcl-form-row__control">
                    <PclInput
                      :model-value="draftValue('ResourceURLOverride', '')"
                      :placeholder="t('pcl.auto2.k3fe4026953')"
                      @update:model-value="drafts.ResourceURLOverride = $event"
                      @blur="save('ResourceURLOverride', drafts.ResourceURLOverride)"
                    />
                  </div>
                </div>
                <div class="pcl-form-row">
                  <span class="pcl-form-row__label">{{ t('pcl.auto2.kc776b5ccce') }}</span>
                  <div class="pcl-form-row__control">
                    <PclInput
                      :model-value="draftValue('LegacyFMLLibsURLOverride', '')"
                      :placeholder="t('pcl.auto2.kecc137ec78')"
                      @update:model-value="drafts.LegacyFMLLibsURLOverride = $event"
                      @blur="save('LegacyFMLLibsURLOverride', drafts.LegacyFMLLibsURLOverride)"
                    />
                  </div>
                </div>
              </template>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k639b8f8d30') }}</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="draftNumber('ModrinthMirror', 0)"
                    :options="modApiMirrorOptions"
                    @update:model-value="save('ModrinthMirror', $event)"
                  />
                  <PclInput
                    v-if="draftNumber('ModrinthMirror', 0) === 2"
                    :model-value="draftValue('ModrinthMirrorURL', '')"
                    :placeholder="t('pcl.auto2.kedac1dee52')"
                    @update:model-value="drafts.ModrinthMirrorURL = $event"
                    @blur="save('ModrinthMirrorURL', drafts.ModrinthMirrorURL)"
                  />
                </div>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kd3e7415b66') }}</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="draftNumber('CurseForgeMirror', 0)"
                    :options="modApiMirrorOptions"
                    @update:model-value="save('CurseForgeMirror', $event)"
                  />
                  <PclInput
                    v-if="draftNumber('CurseForgeMirror', 0) === 2"
                    :model-value="draftValue('CurseForgeMirrorURL', '')"
                    :placeholder="t('pcl.auto2.k05899d2a38')"
                    @update:model-value="drafts.CurseForgeMirrorURL = $event"
                    @blur="save('CurseForgeMirrorURL', drafts.CurseForgeMirrorURL)"
                  />
                </div>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kf1b19d21bf') }}</span>
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
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k2e3f00b939') }}</span>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k7b5aca21d4') }}</span>
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
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k71fd57e730') }}</span>
              </div>

              <PclCheckbox  :model-value="bool('PclDownloadFallbackToQt')" :label="t('pcl.auto2.k9d5b1f1173')" @update:model-value="save('PclDownloadFallbackToQt', $event)" />
            </PclCard>
            <PclCard :title="t('pcl.auto2.k1ed8c078d6')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.ka832ab6015') }}</span>
                <div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.ka832ab6015')" :model-value="draftNumber('NumberOfConcurrentTasks', 10)" :min="1" :max="99" @update:model-value="save('NumberOfConcurrentTasks', $event)" /></div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k7571a8cbc9') }}</span>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.ka55f917b70') }}</span>
                <div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.ka55f917b70')" :model-value="draftNumber('NumberOfConcurrentDownloads', 6)" :min="1" :max="99" @update:model-value="save('NumberOfConcurrentDownloads', $event)" /></div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k15d325f0de') }}</span>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.ka9131cbf3c') }}</span>
                <div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.ka9131cbf3c')" :model-value="draftNumber('NumberOfManualRetries', 1)" :min="0" :max="99" @update:model-value="save('NumberOfManualRetries', $event)" /></div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.kf3191dd279') }}</span>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.ke41ce24cf7') }}</span>
                <div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.ke41ce24cf7')" :model-value="draftNumber('RequestTimeout', 60)" :min="0" :max="99" @update:model-value="save('RequestTimeout', $event)" /></div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.keb6aaba1a1') }}</span>
              </div>
            </PclCard>

            <PclCard :title="t('pcl.auto2.k09ca00a842')">
              <div class="pcl-check-grid">
                <PclCheckbox :model-value="bool('Aria2Enabled')" :label="t('pcl.auto2.kcc4cad514a')" @update:model-value="save('Aria2Enabled', $event)" />
                <PclCheckbox :model-value="bool('Aria2AutoInstall')" :label="t('pcl.auto2.k6b79c51cad')" @update:model-value="save('Aria2AutoInstall', $event)" />
                <PclCheckbox :model-value="bool('Aria2FallbackToQt')" :label="t('pcl.auto2.k303ee36411')" @update:model-value="save('Aria2FallbackToQt', $event)" />
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kb9f4c716af') }}</span>
                <div class="pcl-form-row__control"><PclInput :model-value="draftValue('Aria2ExecutablePath', '')" :placeholder="t('pcl.auto2.kc073f7d55a')" @update:model-value="drafts.Aria2ExecutablePath = $event" @blur="save('Aria2ExecutablePath', drafts.Aria2ExecutablePath)" /><PclIconButton icon="folder-open" :label="t('pcl.auto2.k0323866759')" :size="28" :disabled="busy" @click="chooseFilePath('Aria2ExecutablePath', t('pcl.auto2.ke332de5306'))" /></div>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k32d4fcb63a') }}</span>
                <div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.k26d00a6d80')" :model-value="draftNumber('Aria2MaxConcurrentDownloads', 6)" :min="1" :max="128" :disabled="bool('Aria2FollowLauncherDownloadLimits')" @update:model-value="save('Aria2MaxConcurrentDownloads', $event)" /></div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.k184120fd00') }}</span>
              </div>
              <PclCheckbox :model-value="bool('Aria2FollowLauncherDownloadLimits')" :label="t('pcl.auto2.k219d3abd7a')" @update:model-value="save('Aria2FollowLauncherDownloadLimits', $event)" />
            </PclCard>

            <PclCard :title="t('pcl.auto2.kd6b3cf622a')" collapsible :open="true">
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k9ccf3415b8') }}</span><div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.k9ccf3415b8')" :model-value="draftNumber('Aria2MaxConnectionPerServer', 8)" :min="1" :max="64" @update:model-value="save('Aria2MaxConnectionPerServer', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k35a288dea7') }}</span><div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.k35a288dea7')" :model-value="draftNumber('Aria2Split', 8)" :min="1" :max="64" @update:model-value="save('Aria2Split', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.kd5c81bf7ef') }}</span><div class="pcl-form-row__control"><PclInput :model-value="draftValue('Aria2MinSplitSize', '1M')" :placeholder="t('pcl.auto2.kcfafbaef6e')" @update:model-value="drafts.Aria2MinSplitSize = $event" @blur="save('Aria2MinSplitSize', drafts.Aria2MinSplitSize)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k5016a7bffb') }}</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('Aria2FileAllocation', 'none')" :options="[{ label: t('pcl.auto2.ka56ac5dc51'), value: 'none' }, { label: t('pcl.auto2.k9e16271d26'), value: 'prealloc' }, { label: t('pcl.auto2.kce920fef83'), value: 'trunc' }, { label: 'falloc', value: 'falloc' }]" @update:model-value="save('Aria2FileAllocation', $event)" /></div></div>
              <PclCheckbox :model-value="bool('Aria2Continue')" :label="t('pcl.auto2.k108a36d8d6')" @update:model-value="save('Aria2Continue', $event)" />
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k5f40f75c95') }}</span><div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.k5f40f75c95')" :model-value="draftNumber('Aria2RpcPort', 0)" :min="0" :max="65535" @update:model-value="save('Aria2RpcPort', $event)" /></div><span class="pcl-form-row__hint">{{ t('pcl.auto2.k8195cb3375') }}</span></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k3f6690e65a') }}</span><div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.k3f6690e65a')" :model-value="draftNumber('Aria2PollInterval', 500)" :min="250" :max="10000" @update:model-value="save('Aria2PollInterval', $event)" /></div><span class="pcl-form-row__hint">{{ t('pcl.auto2.kf3c3d56984') }}</span></div>
            </PclCard>

            <PclCard :title="t('pcl.auto2.k319ee56426')" collapsible>
              <PclCheckbox :model-value="bool('Aria2UseLauncherProxy')" :label="t('pcl.auto2.k48264472f0')" @update:model-value="save('Aria2UseLauncherProxy', $event)" />
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k6caded1529') }}</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('Aria2ProxyType', 'None')" :disabled="bool('Aria2UseLauncherProxy')" :options="[{ label: t('pcl.auto2.k4f187022e0'), value: 'None' }, { label: 'HTTP', value: 'HTTP' }, { label: 'SOCKS5', value: 'SOCKS5' }]" @update:model-value="save('Aria2ProxyType', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.kdd39ea863c') }}</span><div class="pcl-form-row__control"><PclInput :model-value="draftValue('Aria2ProxyAddr', '')" :disabled="ariaProxyDisabled" placeholder="127.0.0.1" @update:model-value="drafts.Aria2ProxyAddr = $event" @blur="save('Aria2ProxyAddr', drafts.Aria2ProxyAddr)" /><PclNumberInput :label="t('pcl.auto2.kb2e8679398')" :model-value="draftNumber('Aria2ProxyPort', 8080)" :min="1" :max="65535" :disabled="ariaProxyDisabled" @update:model-value="save('Aria2ProxyPort', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.kbd05aec3ce') }}</span><div class="pcl-form-row__control"><PclInput :model-value="draftValue('Aria2ProxyUser', '')" :disabled="ariaProxyDisabled" :placeholder="t('pcl.auto2.k53e32830a5')" @update:model-value="drafts.Aria2ProxyUser = $event" @blur="save('Aria2ProxyUser', drafts.Aria2ProxyUser)" /><PclSecretInput :label="t('pcl.auto2.kbd675552e9')" :configured="secretConfigured('Aria2ProxyPass')" :disabled="ariaProxyDisabled" :save="value => save('Aria2ProxyPass', value)" /></div></div>
              <div class="pcl-form-row pcl-form-row--top"><span class="pcl-form-row__label">{{ t('pcl.auto2.k2f42122d58') }}</span><div class="pcl-form-row__control pcl-form-row__control--column"><PclInput multiline :rows="3" :model-value="draftValue('Aria2ExtraArgs', '')" :placeholder="t('pcl.auto2.k4eb33f3f06')" @update:model-value="drafts.Aria2ExtraArgs = $event" @blur="save('Aria2ExtraArgs', drafts.Aria2ExtraArgs)" /></div></div>
            </PclCard>

            <PclCard :title="t('pcl.auto2.k5cd6bf739a')">
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.kf6e39cb4d2') }}</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('CurseForgeDownloadBrowser', 'Embedded')" :options="[{ label: t('pcl.auto2.k46a01866fe'), value: 'Embedded' }, { label: t('pcl.auto2.kcb80f053de'), value: 'System' }, { label: t('pcl.auto2.k1a818fb254'), value: 'External' }]" @update:model-value="save('CurseForgeDownloadBrowser', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('ui.modrinthToken') }}</span><div class="pcl-form-row__control"><PclSecretInput :label="t('ui.modrinthToken')" :configured="secretConfigured('ModrinthToken')" :disabled="busy" :save="value => save('ModrinthToken', value)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('ui.curseforgeApiKey') }}</span><div class="pcl-form-row__control"><PclSecretInput :label="t('ui.curseforgeApiKey')" :configured="secretConfigured('FlameKeyOverride')" :disabled="busy" :save="value => save('FlameKeyOverride', value)" /></div></div>
            </PclCard>
          </div>

          <div v-show="section === 'paths'" class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.k0e567795b0')">
              <div v-for="item in pathSettings" :key="item.key" class="pcl-form-row">
                <span class="pcl-form-row__label">{{ item.label }}</span>
                <div class="pcl-form-row__control"><PclInput :model-value="draftValue(item.key, '')" :placeholder="'placeholder' in item ? item.placeholder : t('pcl.setup.pathPlaceholder')" @update:model-value="drafts[item.key] = $event" @blur="save(item.key, drafts[item.key])" /><PclIconButton icon="folder-open" :label="t('pcl.setup.chooseNamed', { name: item.label })" :size="28" :disabled="busy" @click="chooseDirectory(item.key, item.label)" /></div>
              </div>
            </PclCard>
            <PclCard :title="t('pcl.auto2.k0c3031d3cd')">
              <div class="pcl-check-grid"><PclCheckbox :model-value="bool('DownloadsDirWatchRecursive')" :label="t('pcl.auto2.k74948c07d1')" @update:model-value="save('DownloadsDirWatchRecursive', $event)" /><PclCheckbox :model-value="bool('MoveModsFromDownloadsDir')" :label="t('pcl.auto2.k375d153246')" @update:model-value="save('MoveModsFromDownloadsDir', $event)" /></div>
            </PclCard>
            <PclCard :title="t('pcl.auto2.k6e09f08f81')">
              <PclCheckbox  :model-value="bool('CurseForgeExternalToolEnabled')" :label="t('pcl.auto2.kba5e83b7b2')" @update:model-value="save('CurseForgeExternalToolEnabled', $event)" />

              <div v-for="item in toolPathSettings" :key="item.key" class="pcl-form-row"><span class="pcl-form-row__label">{{ item.label }}</span><div class="pcl-form-row__control"><PclInput :model-value="draftValue(item.key, '')" :placeholder="t('pcl.auto2.k53e32830a5')" @update:model-value="drafts[item.key] = $event" @blur="save(item.key, drafts[item.key])" /><PclIconButton icon="file" :label="t('pcl.setup.chooseNamed', { name: item.label })" :size="28" :disabled="busy" @click="chooseFilePath(item.key, item.label)" /></div></div>
            </PclCard>
          </div>

          <div v-show="section === 'console'" class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.k3cfb31d332')">
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k1d0e36d2e0') }}</span><div class="pcl-form-row__control"><PclSlider :model-value="draftNumber('ConsoleMaxLines', 100000)" :min="100" :max="100000" :step="100" @update:model-value="drafts.ConsoleMaxLines = $event" @change="save('ConsoleMaxLines', $event)" /></div><span class="pcl-form-row__hint">{{ t('pcl.setup.lines', { count: draftNumber('ConsoleMaxLines', 100000).toLocaleString() }) }}</span></div>
              <div class="pcl-check-grid"><PclCheckbox :model-value="bool('ConsoleOverflowStop')" :label="t('pcl.auto2.kded97071fe')" @update:model-value="save('ConsoleOverflowStop', $event)" /><PclCheckbox :model-value="bool('LogPrePostOutput')" :label="t('pcl.auto2.kedb03b057e')" @update:model-value="save('LogPrePostOutput', $event)" /></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k27b3bf87b0') }}</span><div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.k27b3bf87b0')" :model-value="draftNumber('ConsoleFontSize', 10)" :min="5" :max="72" @update:model-value="save('ConsoleFontSize', $event)" /></div><span class="pcl-form-row__hint">{{ t('ui.pointUnit') }}</span></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.kc2aab6b7e9') }}</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('ConsoleColorScheme', 'Builtin Dark')" :options="consoleColorOptions" @update:model-value="save('ConsoleColorScheme', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k3245f18624') }}</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('ConsoleFont')" :options="consoleFontOptions" @update:model-value="save('ConsoleFont', $event)" /><PclInput :model-value="draftValue('ConsoleFont', '')" :placeholder="t('pcl.auto2.k0ef8d26ca8')" @update:model-value="drafts.ConsoleFont = $event" @blur="save('ConsoleFont', drafts.ConsoleFont)" /></div></div>
              <PclHint theme="blue">{{ t('pcl.auto2.kfa81d58223') }}</PclHint>
              <pre class="pcl-console-preview" :style="{ fontFamily: draftValue('ConsoleFont', 'monospace'), fontSize: `${draftNumber('ConsoleFontSize', 10)}pt` }">{{ t('pcl.auto2.kc36e1a4b33') }}</pre>
            </PclCard>
            <PclCard v-if="!isInstance" :title="t('pcl.auto2.k6be63dbd9f')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.ke0d4c75d53') }}</span>
                <div class="pcl-form-row__control"><PclSelect :model-value="draftNumber('PastebinType', 3)" :disabled="busy" :options="[{ label: 'mclo.gs', value: 3 }, { label: '0x0.st', value: 0 }, { label: 'paste.gg', value: 2 }, { label: 'hastebin', value: 1 }]" @update:model-value="save('PastebinType', $event)" /></div>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kc8fc6f804e') }}</span>
                <div class="pcl-form-row__control"><PclInput :model-value="draftValue('PastebinCustomAPIBase', '')" :placeholder="t('pcl.auto2.ka3893e289f')" :disabled="busy" @update:model-value="drafts.PastebinCustomAPIBase = $event" /><PclButton size="sm" :disabled="busy" @click="savePasteUrl">{{ t('pcl.auto2.kfadf24dbc5') }}</PclButton></div>
              </div>
              <PclHint theme="blue">{{ t('pcl.auto2.kb623f44173') }}</PclHint>
            </PclCard>
          </div>

          <div v-show="section === 'advanced-settings'" class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.k672450779d')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k320fb2d05d') }}</span>
                <div class="pcl-form-row__control"><PclSelect :model-value="draftNumber('AssetVerificationMode', 1)" :options="assetVerificationOptions" @update:model-value="save('AssetVerificationMode', $event)" /></div>
              </div>
              <p class="pcl-muted">{{ assetVerificationDescriptions[draftNumber('AssetVerificationMode', 1)] }}</p>
              <div v-if="draftNumber('AssetVerificationMode', 1) === 2" class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kf61c84d95c') }}</span>
                <div class="pcl-form-row__control"><PclNumberInput :label="t('pcl.auto2.kf61c84d95c')" :model-value="draftNumber('AssetCacheExpiryDays', 7)" :min="1" :max="365" :unit="t('pcl.setup.days')" @update:model-value="save('AssetCacheExpiryDays', $event)" /></div>
              </div>
            </PclCard>
            <PclCard v-if="isInstance" :title="t('pcl.auto2.k88cd2c41dc')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k389f4e6753') }}</span>
                <div class="pcl-form-row__control"><strong>{{ bool('AssetIntegrityTrusted') ? t('pcl.auto2.k0a9bd11e25') : t('pcl.auto2.k67ea0c37b8') }}</strong></div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.kde492f2031') }}</span>
              </div>
              <div class="pcl-actions">
                <PclButton variant="highlight" icon="shield-check" :disabled="busy" @click="verifyAndTrust">{{ t('pcl.auto2.k2fd46d0f03') }}</PclButton>
                <PclButton v-if="bool('AssetIntegrityTrusted')" variant="ghost" icon="x" :disabled="busy" @click="save('AssetIntegrityTrusted', false)">{{ t('pcl.auto2.kb614311d42') }}</PclButton>
              </div>
              <PclHint theme="yellow">{{ t('pcl.auto2.kd65b7ff3b2') }}</PclHint>
            </PclCard>
            <PclCard :title="t('pcl.auto2.kd5a33bb7e7')">
              <div class="pcl-check-grid"><PclCheckbox :model-value="bool('OnlineFixes')" :label="t('pcl.auto2.k72587f73c0')" @update:model-value="save('OnlineFixes', $event)" /><PclCheckbox :model-value="bool('UseNativeGLFW')" :label="t('pcl.auto2.kb6565ef62e')" @update:model-value="save('UseNativeGLFW', $event)" /><PclCheckbox :model-value="bool('UseNativeOpenAL')" :label="t('pcl.auto2.k7377d6632f')" @update:model-value="save('UseNativeOpenAL', $event)" /><PclCheckbox :model-value="bool('UseDiscreteGpu')" :label="t('pcl.auto2.k3ddf18f61b')" @update:model-value="save('UseDiscreteGpu', $event)" /><PclCheckbox :model-value="bool('EnableFeralGamemode')" :label="t('pcl.auto2.k71a5c775a3')" @update:model-value="save('EnableFeralGamemode', $event)" /><PclCheckbox :model-value="bool('EnableMangoHud')" :label="t('pcl.auto2.k69c86767a7')" @update:model-value="save('EnableMangoHud', $event)" /><PclCheckbox :model-value="bool('UseZink')" :label="t('pcl.auto2.k5a5d182f09')" @update:model-value="save('UseZink', $event)" /></div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k57dc7ecf81') }}</span>
                <div class="pcl-form-row__control"><PclInput :disabled="!bool('UseNativeGLFW')" :model-value="draftValue('CustomGLFWPath')" :placeholder="t('pcl.auto2.k5ae521723c')" @update:model-value="drafts.CustomGLFWPath = $event" @blur="save('CustomGLFWPath', drafts.CustomGLFWPath)" /><PclIconButton icon="folder-open" :label="t('pcl.auto2.k163290badb')" :size="28" :disabled="busy || !bool('UseNativeGLFW')" @click="chooseFilePath('CustomGLFWPath', t('pcl.auto2.k57dc7ecf81'))" /></div>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k0c29921cb8') }}</span>
                <div class="pcl-form-row__control"><PclInput :disabled="!bool('UseNativeOpenAL')" :model-value="draftValue('CustomOpenALPath')" :placeholder="t('pcl.auto2.k5ae521723c')" @update:model-value="drafts.CustomOpenALPath = $event" @blur="save('CustomOpenALPath', drafts.CustomOpenALPath)" /><PclIconButton icon="folder-open" :label="t('pcl.auto2.kf372315ecd')" :size="28" :disabled="busy || !bool('UseNativeOpenAL')" @click="chooseFilePath('CustomOpenALPath', t('pcl.auto2.k0c29921cb8'))" /></div>
              </div>
            </PclCard>
            <PclCard :title="t('pcl.auto2.k467ce31a84')">
              <div class="pcl-check-grid"><PclCheckbox :model-value="bool('ShowGameTime')" :label="t('pcl.auto2.kff9e4486e9')" @update:model-value="save('ShowGameTime', $event)" /><PclCheckbox v-if="!isInstance" :model-value="bool('ShowGlobalGameTime')" :label="t('pcl.auto2.k60f26bd0b9')" @update:model-value="save('ShowGlobalGameTime', $event)" /><PclCheckbox v-if="!isInstance" :model-value="bool('ShowGameTimeWithoutDays')" :label="t('pcl.auto2.k2ee9de6d49')" @update:model-value="save('ShowGameTimeWithoutDays', $event)" /><PclCheckbox :model-value="bool('RecordGameTime')" :label="t('pcl.auto2.k19241e920e')" @update:model-value="save('RecordGameTime', $event)" /></div>
            </PclCard>
          </div>

          <div v-show="section === 'game'" class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.kd4cbc9e6a2')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k765b5418a7') }}</span>
                <div class="pcl-form-row__control pcl-check-row">
                  <PclCheckbox
                    :model-value="!bool('ModDependenciesDisabled')"
                    :label="t('pcl.auto2.k21941e05fb')"
                    @update:model-value="save('ModDependenciesDisabled', !$event)"
                  />
                  <PclCheckbox
                    :model-value="bool('GlobalDataPacksEnabled')"
                    :label="t('pcl.auto2.ke3ac026340')"
                    @update:model-value="save('GlobalDataPacksEnabled', $event)"
                  />
                </div>
              </div>

              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kec623c3f23') }}</span>
                <div class="pcl-form-row__control">
                  <PclInput
                    :model-value="draftValue('GlobalDataPacksPath', '')"
                    :placeholder="t('pcl.auto2.kf36e479c03')"
                    @update:model-value="drafts.GlobalDataPacksPath = $event"
                    @blur="save('GlobalDataPacksPath', drafts.GlobalDataPacksPath)"
                  />
                  <PclIconButton icon="folder-open" :label="t('pcl.auto2.k5f869fce49')" :size="28" :disabled="busy" @click="chooseGlobalDataPacks" />
                </div>
              </div>
            </PclCard>

            <PclCard :title="t('pcl.auto2.kb040e698ea')">
              <div class="pcl-check-grid">
                <PclCheckbox :model-value="bool('ModMetadataDisabled')" :label="t('pcl.auto2.k6e4b7a0c56')" @update:model-value="save('ModMetadataDisabled', $event)" />
                <PclCheckbox :model-value="bool('SkipModpackUpdatePrompt')" :label="t('pcl.auto2.kb444441195')" @update:model-value="save('SkipModpackUpdatePrompt', $event)" />
                <PclCheckbox :model-value="bool('ShowModIncompat')" :label="t('pcl.auto2.k5d8dbaa02a')" @update:model-value="save('ShowModIncompat', $event)" />
                <PclCheckbox :model-value="bool('FallbackMRBlockedMods')" :label="t('pcl.auto2.kbea2a5f6f6')" @update:model-value="save('FallbackMRBlockedMods', $event)" />
              </div>
            </PclCard>
          </div>

          <div v-show="section === 'network'" class="pcl-settings-section">
            <PclCard v-if="isInstance" :title="t('pcl.auto2.k6aca62f330')">
              <p class="pcl-muted">{{ t('pcl.auto2.kf1bf44c5a9') }}</p>
              <PclHint theme="blue">{{ t('pcl.auto2.kbaa138bcf7') }}</PclHint>
            </PclCard>

            <template v-else>
              <PclCard :title="t('pcl.auto2.k22db8c6e46')">
                <template #actions><PclButton size="sm" icon="refresh-cw" :disabled="onlineBusy" @click="loadOnlineBackends(true)">{{ t('pcl.auto2.k38108eaa1d') }}</PclButton></template>
                <PclLoading v-if="onlineBusy" :text="t('pcl.auto2.ke4a515e814')" inline />
                <div v-if="onlineBackends.length" class="pcl-tools-list">
                  <PclListItem v-for="backend in onlineBackends" :key="backend.id" :title="backend.name || backend.id" :info="`${backend.installed ? (backend.version || t('pcl.auto2.keb88ff57c9')) : t('pcl.auto2.k6f7dc945ae')} · ${backend.running ? t('pcl.auto2.k9761f5d947') : t('pcl.auto2.k803638ec0a')}`">
                    <template #icon><PclIcon name="bubbles" :size="16" /></template>
                  </PclListItem>
                </div>
                <PclHint v-else-if="!onlineBusy" theme="yellow">{{ t('pcl.auto2.kb9e6473b23') }}</PclHint>
                <PclHint theme="blue">{{ t('pcl.auto2.k85f96ac696') }}</PclHint>
              </PclCard>
            </template>
          </div>

          <div v-show="section === 'cache'" class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.kab43b1565b')">
              <template #actions>
                <PclButton size="sm" icon="refresh-cw" :disabled="cacheBusy" @click="loadCache(true)">{{ t('pcl.auto2.k38108eaa1d') }}</PclButton>
              </template>
              <PclLoading v-if="cacheBusy" :text="t('pcl.auto2.k3e6f013d61')" inline />
              <PclHint v-if="cacheError" theme="red" closable @close="cacheError = ''">{{ cacheError }}</PclHint>
              <div v-if="cacheState.stats" class="pcl-about-grid">
                <span>{{ t('pcl.auto2.kd42da9e2ac') }}</span><code>{{ cacheState.running ? t('pcl.auto2.k5942497005') : t('pcl.auto2.kb2ba9634d9') }}</code>
                <span>{{ t('pcl.auto2.kc1c23347ef') }}</span><code>{{ cacheState.enabled && cacheState.configured ? t('pcl.auto2.k25d2843150') : t('pcl.auto2.k8bb38ef00c') }}</code>
                <span>{{ t('pcl.auto2.k31479ef943') }}</span><code>{{ cacheState.accountCacheEnabled ? t('pcl.auto2.k25d2843150') : t('pcl.auto2.k8bb38ef00c') }}</code>
                <span>{{ t('pcl.auto2.ke4b9d92f14') }}</span><code>{{ cacheState.stats.entries ?? 0 }}</code>
                <span>{{ t('pcl.auto2.kf05934a464') }}</span><code>{{ formatBytes(cacheState.stats.bytes) }}</code>
                <span>{{ t('pcl.auto2.kbc10822e07') }}</span><code>{{ cacheState.stats.hit ?? 0 }} / {{ cacheState.stats.miss ?? 0 }}</code>
                <span>{{ t('pcl.auto2.k121c44d74b') }}</span><code>{{ cacheState.metadataPath || '—' }}</code>
                <span>{{ t('pcl.auto2.k78acda3988') }}</span><code>{{ cacheState.texturesPath || '—' }}</code>
              </div>
              <PclHint v-else-if="!cacheBusy" theme="blue">{{ t('pcl.auto2.kd9647d9b52') }}</PclHint>
              <div class="pcl-actions">
                <PclButton variant="highlight" icon="trash-2" :disabled="cacheBusy || !cacheState.running" @click="purgeYggdrasilCache">{{ t('pcl.auto2.keb4580d4ae') }}</PclButton>
                <PclButton icon="trash-2" :disabled="busy" @click="clearMetadataCache">{{ t('pcl.auto2.kb7af0a1b98') }}</PclButton>
              </div>
            </PclCard>

            <PclCard :title="t('pcl.auto2.kbeb14fa3dd')">
              <div v-if="cacheRoutes.length" class="pcl-tools-list">
                <PclListItem v-for="route in cacheRoutes" :key="route.id" :title="route.name || route.id" :info="`${route.listen_prefix || route.listenPrefix || '—'} · ${route.enabled ? t('pcl.auto2.kd4e9ca3dd4') : t('pcl.auto2.kd989e55188')}`">
                  <template #icon><PclIcon name="layers" :size="16" /></template>
                  <template #actions><span class="pcl-muted">{{ t('pcl.setup.ttl', { value: route.ttl_seconds ?? route.ttlSeconds ?? 0 }) }}</span></template>
                </PclListItem>
              </div>
              <div v-else class="pcl-empty pcl-empty--compact">{{ t('pcl.auto2.kc6d728c26b') }}</div>
            </PclCard>

            <PclCard :title="t('pcl.auto2.k109dbaf249')">
              <div class="pcl-about-grid">
                <span>{{ t('pcl.auto2.ke4b9d92f14') }}</span><code>{{ cacheState.http?.entries ?? '—' }}</code>
                <span>{{ t('pcl.auto2.kf05934a464') }}</span><code>{{ cacheState.http ? formatBytes(cacheState.http.bytes) : '—' }}</code>
                <span>{{ t('pcl.auto2.k39287df5e0') }}</span><code>{{ cacheState.http?.bases ?? '—' }}</code>
                <span>{{ t('pcl.auto2.kfc7b7a21c5') }}</span><code>{{ cacheState.http?.indexPath || '—' }}</code>
              </div>
              <PclHint theme="blue">{{ t('pcl.auto2.k45798cf477') }}</PclHint>
            </PclCard>
          </div>

          <div v-show="section === 'java'" class="pcl-settings-section">
            <PclJavaPanel :scope="scope" :settings="settings" :busy="busy" :execute="execute" @changed="emit('changed')" />
          </div>

          <div v-show="section === 'ui'" class="pcl-settings-section">
            <PclAppearancePanel />
            <PclCard :title="t('pcl.auto2.k119e538904')" collapsible>
              <PclHint theme="blue">{{ t('pcl.auto2.k4bc53f2a29') }}</PclHint>
              <div class="pcl-check-grid"><PclCheckbox :model-value="bool('MenuBarInsteadOfToolBar')" :label="t('pcl.auto2.kcfe01d2e68')" @update:model-value="save('MenuBarInsteadOfToolBar', $event)" /><PclCheckbox :model-value="bool('UseNewUI')" :label="t('pcl.auto2.kfd0f0be669')" @update:model-value="save('UseNewUI', $event)" /><PclCheckbox :model-value="bool('ShowNewsBar')" :label="t('pcl.auto2.kbb3e9b3130')" @update:model-value="save('ShowNewsBar', $event)" /><PclCheckbox :model-value="bool('StatusBarVisible')" :label="t('pcl.auto2.ka80d0756b7')" @update:model-value="save('StatusBarVisible', $event)" /><PclCheckbox :model-value="bool('ShowServerPreview')" :label="t('pcl.auto2.k7fc810e95d')" @update:model-value="save('ShowServerPreview', $event)" /><PclCheckbox :model-value="bool('ServerPreviewDynamicWidth')" :label="t('pcl.auto2.k7372f1409c')" @update:model-value="save('ServerPreviewDynamicWidth', $event)" /><PclCheckbox :model-value="bool('ShowTerracottaToolBar')" :label="t('pcl.auto2.kbf6344c295')" @update:model-value="save('ShowTerracottaToolBar', $event)" /><PclCheckbox :model-value="bool('ShowYukariConnectToolBar')" :label="t('pcl.auto2.k875aaa909c')" @update:model-value="save('ShowYukariConnectToolBar', $event)" /><PclCheckbox :model-value="bool('ShowTerracottaStatusBar')" :label="t('pcl.auto2.k13ab5bf5eb')" @update:model-value="save('ShowTerracottaStatusBar', $event)" /><PclCheckbox :model-value="bool('ShowYukariConnectStatusBar')" :label="t('pcl.auto2.ke107eb3edb')" @update:model-value="save('ShowYukariConnectStatusBar', $event)" /></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.k0ac5a748d1') }}</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('InstSortMode', 'Name')" :options="[{ label: t('pcl.auto2.k0ea96767da'), value: 'Name' }, { label: t('pcl.auto2.kf04b9a3f81'), value: 'LastLaunch' }]" @update:model-value="save('InstSortMode', $event)" /></div></div>
              <div class="pcl-form-row"><span class="pcl-form-row__label">{{ t('pcl.auto2.kaa6f9f5c04') }}</span><div class="pcl-form-row__control"><PclSelect :model-value="draftValue('InstRenamingMode', 'AskEverytime')" :options="[{ label: t('pcl.auto2.k393704c0f8'), value: 'AskEverytime' }, { label: t('pcl.auto2.ka2c67e7514'), value: 'PhysicalDir' }, { label: t('pcl.auto2.k570d7259cd'), value: 'MetadataOnly' }]" @update:model-value="save('InstRenamingMode', $event)" /></div></div>
            </PclCard>

          </div>

          <div v-show="section === 'language'" class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.kcd99b225a0')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kd5d8edd12c') }}</span>
                <div class="pcl-form-row__control">
                  <PclSelect
                    :model-value="languageId"
                    :options="languageOptions"
                    :placeholder="t('pcl.auto2.kf4bbd91f79')"
                    @update:model-value="selectLanguage"
                  />
                </div>
                <PclButton icon="refresh-cw" :disabled="busy || languagesLoading" @click="loadLanguages(true)">{{ t('pcl.auto2.k38108eaa1d') }}</PclButton>
              </div>
              <PclCheckbox :model-value="useSystemLocale" :label="t('pcl.auto2.k726bce844f')" @update:model-value="setSystemLocale" />
              <PclLoading v-if="languagesLoading" :text="t('pcl.auto2.kffd7a06887')" inline />
            </PclCard>

            <div class="pcl-language-banner">
              <PclIcon name="earth" :size="190" class="pcl-language-banner__earth" />
              <div class="pcl-language-banner__copy">
                <div class="pcl-language-banner__title">{{ t('pcl.auto2.k12628589c6') }}<b>Luna</b>{{ t('pcl.auto2.k256bab05d0') }}</div>
                <p>{{ t('pcl.auto2.k23e1871996') }}</p>
                <p>{{ t('pcl.auto2.k2fd54e48a1') }}</p>
                <p>{{ t('pcl.auto2.kf3d356c728') }}</p>
                <div class="pcl-actions">
                  <PclButton variant="highlight" icon="upload" @click="openUrl(projectUrl + '/pulls')">{{ t('pcl.auto2.k5927da4a31') }}</PclButton>
                  <PclButton icon="book-marked" @click="openUrl(docsUrl)">{{ t('pcl.auto2.k550fd49891') }}</PclButton>
                </div>
              </div>
            </div>
          </div>

          <div v-show="section === 'misc'" class="pcl-settings-section">
            <PclCard v-if="!isInstance" :title="t('pcl.auto2.k0cbda6b524')">
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k6caded1529') }}</span>
                <div class="pcl-form-row__control">
                  <PclSelect v-model="proxy.type" :options="proxyTypeOptions" />
                </div>
                <span class="pcl-form-row__hint">{{ t('pcl.auto2.ke3fde22bff') }}</span>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k67d2d7970f') }}</span>
                <div class="pcl-form-row__control">
                  <PclInput v-model="proxy.host" placeholder="127.0.0.1" />
                </div>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.k6cbb7335b5') }}</span>
                <div class="pcl-form-row__control">
                  <PclInput v-model.number="proxy.port" type="number" placeholder="7890" style="width: 110px" />
                </div>
              </div>
              <div class="pcl-form-row">
                <span class="pcl-form-row__label">{{ t('pcl.auto2.kbd05aec3ce') }}</span>
                <div class="pcl-form-row__control">
                  <PclInput v-model="proxy.username" :placeholder="t('pcl.auto2.k53e32830a5')" />
                  <PclInput v-model="proxy.password" type="password" :placeholder="t('pcl.auto2.k53e32830a5')" />
                </div>
              </div>
              <div class="pcl-actions">
                <PclButton variant="highlight" icon="save" :disabled="busy" @click="saveProxy">{{ t('pcl.auto2.kc249ff9d56') }}</PclButton>
                <PclButton icon="network" :disabled="busy" @click="testProxy">{{ t('pcl.auto2.k10b7d8eccc') }}</PclButton>
              </div>
              <PclHint theme="yellow">{{ t('pcl.auto2.kab43ea5794') }}</PclHint>
              <div v-if="proxyTestResult" class="pcl-diagnostic">{{ proxyTestResult }}</div>
            </PclCard>

            <PclCard :title="t('pcl.auto2.k1758690108')" collapsible>
              <div class="pcl-check-grid">
                <PclCheckbox :model-value="bool('PclDownloadFallbackToQt')" :label="t('pcl.auto2.k30ad19d3a5')" @update:model-value="save('PclDownloadFallbackToQt', $event)" />
                <PclCheckbox :model-value="bool('Aria2FallbackToQt')" :label="t('pcl.auto2.ka8ffe0fe9c')" @update:model-value="save('Aria2FallbackToQt', $event)" />
                <PclCheckbox :model-value="bool('DownloadsDirWatchRecursive')" :label="t('pcl.auto2.kbdd3825058')" @update:model-value="save('DownloadsDirWatchRecursive', $event)" />
              </div>
            </PclCard>
          </div>

          <div v-show="section === 'about'" class="pcl-settings-section">
            <PclCard :title="t('about.title')">
              <div class="pcl-about-head">
                <div class="pcl-about-logo"><img :src="lunaLogo" alt="Luna Launcher" /></div>
                <div>
                  <div class="pcl-about-name">{{ t('brand.name') }} <span>{{ t('brand.neoUi') }}</span></div>
                  <div class="pcl-about-version">{{ applicationVersion }} · {{ runtimeInfo.platform || 'Windows' }}</div>
                </div>
              </div>
              <div class="pcl-about-grid">
                <span>{{ t('pcl.auto2.k9742cd9ffb') }}</span><code>{{ runtimeInfo.dataRoot || '—' }}</code>
                <span>{{ t('pcl.auto2.k8c27cb90e0') }}</span><code>{{ runtimeInfo.root || runtimeInfo.programRoot || '—' }}</code>
                <span>{{ t('pcl.auto2.k5068de9911') }}</span><code>{{ runtimeInfo.apiVersion || runtimeInfo.capabilities?.apiVersion || '1' }}</code>
                <span>{{ t('pcl.auto2.keb9d53ce7f') }}</span><code>{{ (runtimeInfo.capabilities || []).join(' / ') || '—' }}</code>
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
                  <PclButton size="sm" variant="ghost" icon="external-link" @click.stop="openUrl(credit.url)">{{ t('pcl.auto2.k65fc81e161') }}</PclButton>
                </template>
              </PclListItem>
            </PclCard>

            <PclCard :title="t('about.contributors')">
              <div class="pcl-credit-tiles">
                <span v-for="name in contributors" :key="name">{{ name }}</span>
              </div>
              <p class="pcl-muted pcl-credit-note">{{ t('pcl.auto2.k1c55d41283') }}</p>
            </PclCard>

            <PclCard :title="t('about.licenses')">
              <p class="pcl-muted">{{ t('about.licenseText') }}</p>
              <p class="pcl-muted">{{ t('about.pclIconCredit') }}</p>
              <div class="pcl-actions">
                <PclButton icon="file-box" @click="openUrl(licenseUrl)">{{ t('ui.licenseLabel') }}</PclButton>
                <PclButton icon="file-box" @click="openUrl('https://github.com/PrismLauncher/PrismLauncher/blob/develop/LICENSE')">{{ t('pcl.auto2.k2185d95c14') }}</PclButton>
                <PclButton icon="file-box" @click="openUrl('https://lucide.dev/license')">{{ t('about.lucideLicense') }}</PclButton>
              </div>
            </PclCard>
          </div>

          <div v-show="section === 'update'" class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.k45f9954c5b')">
              <div class="pcl-check-grid">
                <PclCheckbox :model-value="updateState.automatic" :label="t('pcl.auto2.kf54e16b861')" @update:model-value="configureUpdate({ automatic: $event })" />
                <PclCheckbox :model-value="updateState.beta" :label="t('pcl.auto2.k6d2e57a86f')" @update:model-value="configureUpdate({ beta: $event })" />
              </div>
              <div class="pcl-actions">
                <PclButton variant="highlight" icon="refresh-cw" :disabled="busy || updateChecking" @click="checkUpdate">{{ updateChecking ? t('pcl.auto2.k8872ba182f') : t('pcl.auto2.ka6df38586d') }}</PclButton>
                <PclButton icon="folder-open" @click="openUpdateLog">{{ t('pcl.auto2.ka040e64bcf') }}</PclButton>
              </div>
              <PclLoading v-if="updateChecking" :text="t('pcl.auto2.kec43764508')" inline />
            </PclCard>

            <PclCard v-if="updateCheck.available" :title="t('pcl.auto2.k01047404ef')">
              <div class="pcl-update-head">
                <div class="pcl-update-icon">↑</div>
                <div>
                  <div class="pcl-update-name">{{ updateCheck.versionName }} <span>{{ updateCheck.versionTag }}</span></div>
                  <div class="pcl-muted">{{ updateCheck.releaseTime }}</div>
                </div>
                <PclButton variant="accent" icon="download" :disabled="busy" @click="applyUpdate">{{ t('pcl.auto2.k087db63ab1') }}</PclButton>
              </div>
              <pre v-if="updateCheck.releaseNotes" class="pcl-release-notes">{{ updateCheck.releaseNotes }}</pre>
            </PclCard>

            <PclCard v-else-if="updateChecked" :title="t('pcl.auto2.k46e66f6321')">
              <p>{{ t('pcl.setup.upToDate', { time: updateState.lastCheck || t('pcl.auto2.k9e636642d6') }) }}</p>
            </PclCard>

            <PclCard v-else :title="t('pcl.auto2.k69d14924ff')">
              <div class="pcl-about-grid">
                <span>{{ t('pcl.auto2.k2f344bbb90') }}</span><code>{{ updateState.automatic ? t('pcl.auto2.k256783b7d4') : t('pcl.auto2.k6c14bd7f6f') }}</code>
                <span>{{ t('pcl.auto2.k21c42fae0b') }}</span><code>{{ updateState.beta ? t('pcl.auto2.k256783b7d4') : t('pcl.auto2.k6c14bd7f6f') }}</code>
                <span>{{ t('pcl.auto2.k6e85a9405c') }}</span><code>{{ updateState.lastCheck || '—' }}</code>
              </div>
            </PclCard>
          </div>

          <div v-show="section === 'feedback'" class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.k587d5249cb')">
              <p class="pcl-feedback-copy">{{ t('pcl.auto2.k038b4c7f6e') }}</p>
              <PclButton variant="highlight" icon="message-circle" @click="openUrl(projectUrl + '/issues')">{{ t('pcl.auto2.ke1b7172cbb') }}</PclButton>
            </PclCard>
            <PclCard :title="t('pcl.auto2.k4a3f0161ab')">
              <p class="pcl-muted">{{ t('pcl.auto2.k6272578ef4') }}</p>
            </PclCard>
          </div>

          <div v-show="section === 'logs'" class="pcl-settings-section">
            <PclCard :title="t('pcl.auto2.k5d95f08cb5')">
              <div class="pcl-actions pcl-log-actions">
                <PclButton variant="highlight" icon="folder-open" :disabled="busy" @click="openLogs">{{ t('pcl.auto2.ke153b1be0e') }}</PclButton>
                <PclButton icon="scroll-text" :disabled="busy" @click="openUpdateLog">{{ t('pcl.auto2.k185368440e') }}</PclButton>
                <PclButton icon="download" :disabled="true" :title="t('pcl.auto2.k347caf3067')">{{ t('pcl.auto2.k252fed9478') }}</PclButton>
                <PclButton variant="red" icon="trash-2" :disabled="true" :title="t('pcl.auto2.k0ba9ee0d69')">{{ t('pcl.auto2.k886c7cf056') }}</PclButton>
              </div>
            </PclCard>
            <PclCard :title="t('pcl.auto2.k7815a67182')">
              <pre v-if="updateState.updateLog" class="pcl-log-tail">{{ updateState.updateLog }}</pre>
              <div v-else class="pcl-empty">{{ t('pcl.auto2.k00ad5e6a6f') }}</div>
            </PclCard>
          </div>
        </div>
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
  { label: t('pcl.auto2.k2305ee3ca3'), items: [
    { id: 'launch', label: t('pcl.auto2.kebd26da421'), icon: 'rocket', refresh: true },
    { id: 'java', label: 'Java', icon: 'coffee', refresh: true },
    { id: 'game', label: t('pcl.auto2.kd4cbc9e6a2'), icon: 'book-marked', refresh: true },
    { id: 'advanced', label: t('pcl.auto2.kb85853c3fc'), icon: 'tool-case' },
  ] },
  { label: t('pcl.auto2.ka72ef18d9a'), items: [
    { id: 'network', label: t('pcl.auto2.k1d5e43605d'), icon: 'bubbles', refresh: true },
  ] },
  { label: t('pcl.auto2.ka992a7f81a'), items: [
    { id: 'accounts', label: t('pcl.auto2.k9823d89dbc'), icon: 'user', refresh: true },
    { id: 'ui', label: t('pcl.auto2.kcd4994e503'), icon: 'palette', refresh: true },
    { id: 'language', label: t('pcl.auto2.kcd99b225a0'), icon: 'earth', refresh: true },
    { id: 'misc', label: t('pcl.auto2.kb046fe277d'), icon: 'monitor-cog', refresh: true },
    { id: 'download-settings', label: t('pcl.auto2.kbcf7016f51'), icon: 'download', refresh: true },
    { id: 'cache', label: t('pcl.auto2.kc15668b86b'), icon: 'layers', refresh: true },
    { id: 'paths', label: t('pcl.auto2.kf9b678c1d7'), icon: 'folder-cog', refresh: true },
    { id: 'console', label: t('pcl.auto2.k5bd086d22a'), icon: 'terminal', refresh: true },
    { id: 'advanced-settings', label: t('pcl.auto2.kd501b653be'), icon: 'settings', refresh: true },
  ] },
  { label: t('pcl.auto2.kbed172efc9'), items: [
    { id: 'about', label: t('pcl.auto2.k638d2df6e1'), icon: 'info' },
    { id: 'update', label: t('pcl.auto2.k7e5ce811e8'), icon: 'refresh-cw' },
    { id: 'feedback', label: t('pcl.auto2.k42a36e9497'), icon: 'message-circle', refresh: true },
    { id: 'logs', label: t('pcl.auto2.kb923b26d33'), icon: 'scroll-text' },
  ] },
];
const instanceGroups: PclNavGroup[] = [
  { label: t('pcl.auto2.k1836ba7e88'), items: [
    { id: 'launch', label: t('pcl.auto2.k6c4942326d'), icon: 'rocket', refresh: true },
    { id: 'java', label: 'Java', icon: 'coffee', refresh: true },
    { id: 'memory', label: t('pcl.auto2.kd014ab7703'), icon: 'gauge', refresh: true },
    { id: 'network', label: t('pcl.auto2.k6aca62f330'), icon: 'server', refresh: true },
    { id: 'advanced', label: t('pcl.auto2.kc009d0ab82'), icon: 'tool-case' },
    { id: 'advanced-settings', label: t('pcl.auto2.ke362c495b7'), icon: 'settings', refresh: true },
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
  { label: t('pcl.auto2.kc8d09cf955'), value: 'default' },
  { label: t('pcl.auto2.kda2d806e5f'), value: 'maximized' },
  { label: t('pcl.auto2.k1387e89231'), value: 'custom' },
];
const mirrorOptions = [
  { label: t('pcl.auto2.k212e81e6d0'), value: 0 },
  { label: t('pcl.auto2.kf0aba8a42d'), value: 1 },
  { label: t('pcl.auto2.kfe7276cd17'), value: 2 },
];
const downloadBackendOptions = [
  { label: t('pcl.auto2.ka626fe7700'), value: 0 },
  { label: 'Aria2', value: 1 },
  { label: t('pcl.auto2.k3ab6cb62c4'), value: 2 },
];
const modApiMirrorOptions = [
  { label: t('pcl.auto2.k212e81e6d0'), value: 0 },
  { label: t('pcl.auto2.kc0fa397b56'), value: 1 },
  { label: t('pcl.auto2.kfe7276cd17'), value: 2 },
];
const proxyTypeOptions = [
  { label: t('pcl.auto2.k4f187022e0'), value: 'None' },
  { label: t('pcl.auto2.kf4bbd91f79'), value: 'Default' },
  { label: t('pcl.auto2.kd95ef5ba39'), value: 'HTTP' },
  { label: t('pcl.auto2.k865df8bce7'), value: 'SOCKS5' },
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
])].map(value => ({ label: value.replace('Builtin ', t('pcl.auto2.kacc7e7959f')), value })));
const consoleFontOptions = computed(() => [...new Set([
  draftValue('ConsoleFont', 'Courier New'), 'Cascadia Mono', 'Consolas', 'Courier New', 'Menlo', 'Monospace',
])].filter(Boolean).map(value => ({ label: value, value })));
const assetVerificationOptions = [
  { label: t('pcl.auto2.k1e3797b17e'), value: 0 }, { label: t('pcl.auto2.k06804c01de'), value: 1 },
  { label: t('pcl.auto2.ka9f4add9d6'), value: 2 }, { label: t('pcl.auto2.k53b916d50c'), value: 3 },
];
const assetVerificationDescriptions = [
  t('pcl.auto2.k05a706cafd'), t('pcl.auto2.k9512244fb6'),
  t('pcl.auto2.k941818ae16'), t('pcl.auto2.kc5d97cda92'),
];
const languageOptions = computed(() => languages.value.map(item => ({
  label: item.name || item.id,
  value: item.id,
})));

const settingsMap = computed(() => new Map(props.settings.map(setting => [setting.key, setting])));
const pathSettings = [
  { key: 'InstanceDir', label: t('pcl.auto2.k1252eac405'), placeholder: t('pcl.auto2.k1b269ad6f3') },
  { key: 'CentralModsDir', label: t('pcl.auto2.k4250e081b4') },
  { key: 'IconsDir', label: t('pcl.auto2.ka04545c951') },
  { key: 'DownloadsDir', label: t('pcl.auto2.kaf0bdd1ea7') },
  { key: 'SkinsDir', label: t('pcl.auto2.kc42aee88ea') },
  { key: 'JavaDir', label: t('pcl.auto2.k325cea939e') },
  { key: 'FTBAppInstancesPath', label: t('pcl.auto2.kcad8a90505') },
  { key: 'JProfilerPath', label: t('pcl.auto2.k314accf461') },
  { key: 'MCEditPath', label: t('pcl.auto2.ke756c0ecd0') },
] as const;
const toolPathSettings = [
  { key: 'Aria2ExecutablePath', label: t('pcl.auto2.ke332de5306') },
  { key: 'CurseForgeExternalToolPath', label: t('pcl.auto2.k6c0497a683') },
  { key: 'JVisualVMPath', label: t('pcl.auto2.kfeee3cc659') },
  { key: 'JsonEditor', label: t('pcl.auto2.kea37ad033c') },
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
  if (!value) return t('pcl.auto2.k142ff5c090');
  return value >= 1024 ? `${(value / 1024).toFixed(value % 1024 ? 1 : 0)} MiB/s` : `${value} KiB/s`;
}

const applicationVersion = computed(() => String(runtimeInfo.value.applicationVersion || runtimeInfo.value.version || runtimeInfo.value.launcherVersion || t('pcl.auto2.kd9892e1d1d')).trim());
const credits = [
  { name: 'PrismLauncher', info: t('pcl.auto2.k53ca260e72'), url: 'https://github.com/PrismLauncher/PrismLauncher' },
  { name: t('pcl.auto2.kc47f1374db'), info: t('pcl.auto2.k62b98f6a7e'), url: 'https://github.com/PCL-Community/PCL-CE' },
  { name: 'BMCLAPI', info: t('pcl.auto2.kc6d1ce501d'), url: 'https://bmclapidoc.bangbang93.com/' },
  { name: 'MCIM', info: t('pcl.auto2.k5ed7a01124'), url: 'https://mod.mcimirror.top/docs' },
  { name: 'Lucide', info: t('pcl.auto2.k2582b2c5c1'), url: 'https://lucide.dev' },
  { name: 'Qt', info: t('pcl.auto2.k9405b02a3e'), url: 'https://www.qt.io' },
];
const contributors = ['Luna Neo UI', 'PrismLauncher', 'BMCLAPI', 'MCIM', 'Lucide', 'Qt', t('pcl.auto2.ka7146a67de'), 'Bangbang93', 'Pysio'];
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
    } catch { error.value = t('pcl.auto2.k1bf0b9499a'); return; }
  }
  await save('PastebinCustomAPIBase', value);
}
async function chooseGlobalDataPacks() {
  const selected = await pickPclDirectory({ title: t('pcl.auto2.k5f869fce49') });
  if (!selected) return;
  drafts.GlobalDataPacksPath = selected;
  save('GlobalDataPacksPath', selected);
}
async function chooseDirectory(key: string, label: string) {
  const selected = await pickPclDirectory({ title: t('pcl.setup.chooseNamed', { name: label }) });
  if (!selected) return;
  drafts[key] = selected;
  await save(key, selected);
}
async function chooseFilePath(key: string, label: string) {
  const selected = await pickPclFile({ title: t('pcl.setup.chooseNamed', { name: label }) });
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
    launch: [{ key: 'OverrideWindow', label: t('pcl.auto2.k6d35264835') }, { key: 'OverrideConsole', label: t('pcl.auto2.kc0c465ebf4') }, { key: 'OverrideMiscellaneous', label: t('pcl.auto2.k42b3c4070c') }],
    memory: [{ key: 'OverrideMemory', label: t('pcl.auto2.k8952f181d8') }],
    advanced: [{ key: 'OverrideCommands', label: t('pcl.auto2.k347f57b633') }, { key: 'OverrideJavaArgs', label: t('pcl.auto2.k132a0f71b7') }],
    'advanced-settings': [{ key: 'OverrideAssetVerification', label: t('pcl.auto2.k38603922ea') }, { key: 'OverrideNativeWorkarounds', label: t('pcl.auto2.k1d8d0ba1d6') },
      { key: 'OverridePerformance', label: t('pcl.auto2.k5921928fa7') }, { key: 'OverrideLegacySettings', label: t('pcl.auto2.k74dc0961a0') }, { key: 'OverrideGameTime', label: t('pcl.auto2.k38b6f73b0a') }],
  };
  return (sections[section.value] || []).filter(item => settingsMap.value.has(item.key));
});
let saveQueue: Promise<unknown> = Promise.resolve();
function save(key: string, value: unknown): Promise<boolean> {
  const operation = saveQueue.then(() => persistSetting(key, value));
  saveQueue = operation.catch(() => {});
  return operation;
}
function sameSettingValue(left: unknown, right: unknown): boolean {
  if (Object.is(left, right)) return true;
  try { return JSON.stringify(left) === JSON.stringify(right); } catch { return String(left) === String(right); }
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
    const applied = await props.execute('settings.set', { ...props.scope, key, value: normalized });
    // The API returns the value held by SettingsObject after conversion and
    // persistence. Verify it before reporting success so a stale backend or a
    // type conversion bug cannot leave the UI claiming that a setting applied.
    if (applied?.key === key && !applied.redacted && !sameSettingValue(applied.value, normalized)) {
      throw new Error(t('pcl.setup.notApplied', { key }));
    }
    if (!setting.redacted) drafts[key] = applied?.key === key ? applied.value : normalized;
    notice.value = t('pcl.auto2.kcb84a961b0');
    window.setTimeout(() => { if (notice.value === t('pcl.auto2.kcb84a961b0')) notice.value = ''; }, 1800);
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
    notice.value = t('pcl.auto2.k772d16fa01');
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
      cacheError.value = t('pcl.auto2.k5a86d98bca');
  } finally {
    cacheBusy.value = false;
  }
}
async function purgeYggdrasilCache() {
  if (!window.confirm(t('pcl.auto2.k8680dc4e80'))) return;
  cacheBusy.value = true;
  try {
    await props.execute('yggdrasil.cache.purge', { confirm: true });
    notice.value = t('pcl.auto2.kd4a34f3220');
    await loadCache(true);
  } catch (exception) {
    cacheError.value = message(exception);
  } finally {
    cacheBusy.value = false;
  }
}
async function clearMetadataCache() {
  if (!window.confirm(t('pcl.auto2.k0c2737f9ba'))) return;
  try {
    await props.execute('launcher.cache.clear', { confirm: true });
    notice.value = t('pcl.auto2.kdc0ba46d10');
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
    notice.value = t('pcl.auto2.k2b01d892c2');
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
    if (updateCheck.value?.available) notice.value = t('pcl.setup.newVersion', { version: updateCheck.value.versionName || '' });
    else notice.value = t('pcl.auto2.kdb620f1761');
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
    notice.value = t('pcl.auto2.k7d8ec88815');
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
    notice.value = t('pcl.auto2.k7be2e7e740');
  } catch (exception) {
    error.value = message(exception);
  }
}
async function testProxy() {
  proxyTestResult.value = '';
  try {
    const result = await props.execute('network.proxy.test', { url: proxyTestUrl.value, timeoutMs: 10000 });
    proxyTestResult.value = result?.status
      ? t('pcl.setup.connectionSuccessHttp', { status: result.status, time: result.timeMs ?? '—' })
      : t('pcl.setup.connectionSuccess', { value: JSON.stringify(result) });
  } catch (exception) {
    proxyTestResult.value = t('pcl.setup.connectionFailed', { error: message(exception) });
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
    if (!root) throw new Error(t('pcl.auto2.k83bd4d1767'));
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
